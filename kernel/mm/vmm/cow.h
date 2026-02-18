/* ==============================================================================
 * CCOS - Copy-on-Write (COW) Implementation
 * ==============================================================================
 * This module provides copy-on-write memory management, which is essential
 * for efficient fork() system call implementation. Multiple processes can
 * share the same physical pages until one of them attempts to write.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"
#include "mm/vmm/page.h"

/* Forward declaration */
struct interrupt_frame;

/* ============================================================================
 * COW Constants
 * ============================================================================ */

/**
 * COW flag - stored in PTE available bit 9
 * When set, this page is shared and should trigger COW on write.
 */
#define COW_FLAG_MASK    (1ULL << 9)

/**
 * Maximum reference count for a COW page
 */
#define COW_MAX_REFCOUNT  0xFFFF

/* Default hash table size for COW tracking */
#define COW_HASH_SIZE  256

/* ============================================================================
 * COW Data Structures
 * ============================================================================ */

/**
 * @brief COW block - tracks a shared physical page
 *
 * The orig_phys field serves as both the key and value - we store the
 * physical address in the block itself and use the block's address as
 * the hashmap key (pointer-based hashing).
 */
typedef struct cow_block {
    physical_addr_t    orig_phys;     /* Original physical page address (acts as key) */
    uint16_t           refcount;      /* Reference count (1-COW_MAX_REFCOUNT) */
} cow_block_t;

/**
 * @brief COW region - for tracking larger COW regions (used by fork)
 */
typedef struct cow_region {
    virtual_addr_t     start;         /* Region start (page-aligned) */
    uint64_t           page_count;    /* Number of pages */
    physical_addr_t    pml4;          /* Address space this belongs to */
    struct cow_region* next;          /* Linked list */
} cow_region_t;

/**
 * @brief COW statistics
 */
typedef struct {
    uint64_t    cow_faults_handled;   /* Number of COW faults processed */
    uint64_t    cow_pages_allocated;  /* Total COW pages allocated */
    uint64_t    cow_pages_freed;      /* Total COW pages freed */
    uint64_t    cow_current_blocks;   /* Current number of COW blocks */
    uint64_t    cow_write_faults;     /* Write faults on COW pages */
    uint64_t    cow_coalesced;        /* Pages that didn't need copy (refcount=1) */
} cow_stats_t;

/**
 * @brief COW result codes
 */
typedef enum {
    COW_OK = 0,
    COW_ERR_NOT_INIT = -1,
    COW_ERR_OOM = -2,
    COW_ERR_INVALID = -3,
    COW_ERR_NOT_COW = -4,
    COW_ERR_MAX_REFCOUNT = -5,
} cow_result_t;

/* ============================================================================
 * COW Core API
 * ============================================================================ */

/**
 * @brief Initialize the COW subsystem
 * @return COW_OK on success, error code otherwise
 */
cow_result_t cow_init(void);

/**
 * @brief Check if COW subsystem is initialized
 * @return true if initialized
 */
bool cow_is_initialized(void);

/**
 * @brief Get COW statistics
 * @param stats Pointer to stats structure to fill
 * @return COW_OK on success
 */
cow_result_t cow_get_stats(cow_stats_t* stats);

/* ============================================================================
 * COW Page Management
 * ============================================================================ */

/**
 * @brief Add a page to COW tracking
 * @param phys Physical address of the page
 * @return COW_OK on success, error code otherwise
 */
cow_result_t cow_add_page(physical_addr_t phys);

/**
 * @brief Increment reference count for a COW page
 * @param phys Physical address of the page
 * @return COW_OK on success, COW_ERR_NOT_COW if not tracked
 */
cow_result_t cow_inc_refcount(physical_addr_t phys);

/**
 * @brief Decrement reference count for a COW page
 * @param phys Physical address of the page
 * @return COW_OK on success, COW_ERR_NOT_COW if not tracked
 * @note If refcount reaches 0, the page is removed from tracking
 */
cow_result_t cow_dec_refcount(physical_addr_t phys);

/**
 * @brief Get reference count for a COW page
 * @param phys Physical address of the page
 * @param out_refcount Pointer to store refcount
 * @return COW_OK on success, COW_ERR_NOT_COW if not tracked
 */
cow_result_t cow_get_refcount(physical_addr_t phys, uint16_t* out_refcount);

/**
 * @brief Look up COW block by physical address
 * @param phys Physical address to look up
 * @return Pointer to COW block, or NULL if not found
 */
cow_block_t* cow_lookup_block(physical_addr_t phys);

/* ============================================================================
 * COW Region Management
 * ============================================================================ */

/**
 * @brief Register a memory region as copy-on-write
 * @param pml4 Address space PML4 physical address
 * @param base Base virtual address (must be page-aligned)
 * @param size Size of region in bytes (must be multiple of page size)
 * @return COW_OK on success, error code otherwise
 *
 * This function:
 * 1. Validates the region alignment
 * 2. Adds each mapped page to COW tracking
 * 3. Marks each page as read-only with COW flag
 */
cow_result_t cow_register_region(physical_addr_t pml4,
                                 virtual_addr_t base,
                                 size_t size);

/**
 * @brief Unregister a COW region
 * @param pml4 Address space PML4 physical address
 * @param base Base virtual address
 * @return COW_OK on success
 *
 * Decrements refcounts for all pages in the region.
 * Pages with refcount=1 are removed from tracking.
 */
cow_result_t cow_unregister_region(physical_addr_t pml4,
                                   virtual_addr_t base);

/* ============================================================================
 * COW Page Fault Handling
 * ============================================================================ */

/**
 * @brief Handle a copy-on-write page fault
 * @param pml4 Current address space PML4
 * @param fault_addr Address that caused the fault
 * @return COW_OK if handled, error code otherwise
 *
 * This function:
 * 1. Queries the current mapping
 * 2. Checks if it's a COW page
 * 3. If refcount=1: makes page writable directly
 * 4. If refcount>1: allocates new page and copies content
 */
cow_result_t cow_handle_fault(physical_addr_t pml4, virtual_addr_t fault_addr);

/**
 * @brief Mark a page as read-only with COW flag
 * @param pml4 Address space PML4
 * @param vaddr Virtual address to mark
 * @return COW_OK on success
 */
cow_result_t cow_mark_page_readonly(physical_addr_t pml4, virtual_addr_t vaddr);

/**
 * @brief Make a COW page writable (when refcount=1)
 * @param pml4 Address space PML4
 * @param vaddr Virtual address
 * @param phys Current physical address
 * @return COW_OK on success
 */
cow_result_t cow_make_writable(physical_addr_t pml4,
                               virtual_addr_t vaddr,
                               physical_addr_t phys);

/**
 * @brief Perform the actual copy-on-write operation
 * @param pml4 Address space PML4
 * @param vaddr Virtual address that faulted
 * @param orig_phys Original physical page
 * @param block COW block containing refcount
 * @return COW_OK on success, error code otherwise
 */
cow_result_t cow_copy_on_write(physical_addr_t pml4,
                               virtual_addr_t vaddr,
                               physical_addr_t orig_phys,
                               cow_block_t* block);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if a page table entry has COW flag set
 * @param pte_flags Page table entry flags
 * @return true if COW flag is set
 */
static inline bool cow_is_cow_page(uint64_t pte_flags) {
    return (pte_flags & COW_FLAG_MASK) != 0;
}

/**
 * @brief Set COW flag in page table entry flags
 * @param pte_flags Current flags
 * @return New flags with COW bit set
 */
static inline uint64_t cow_set_cow_flag(uint64_t pte_flags) {
    return pte_flags | COW_FLAG_MASK;
}

/**
 * @brief Clear COW flag from page table entry flags
 * @param pte_flags Current flags
 * @return New flags with COW bit cleared
 */
static inline uint64_t cow_clear_cow_flag(uint64_t pte_flags) {
    return pte_flags & ~COW_FLAG_MASK;
}
