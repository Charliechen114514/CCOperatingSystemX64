/* ==============================================================================
 * CCOS - Page Table Management for x86_64
 * ==============================================================================
 * This module provides low-level page table management for the x86_64
 * architecture, including four-level page table structures (PML4/PDPT/PD/PT),
 * page table entry manipulation, and address translation functions.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"
#include "mm/pframe/pframe.h"  /* For PAGE_SIZE, PAGE_SHIFT, physical_addr_t */

/* ==============================================================================
 * Page Size Constants
 * Note: PAGE_SIZE and PAGE_SHIFT are defined in pframe/pframe.h
 * ============================================================================== */

/* Define PAGE_SHIFT locally if not already defined */
#ifndef PAGE_SHIFT
#define PAGE_SHIFT       12
#endif

/* Define PAGE_SIZE locally if not already defined */
#ifndef PAGE_SIZE
#define PAGE_SIZE        (1ULL << PAGE_SHIFT)   /* 4096 bytes */
#endif

/* Huge page sizes */
#define PAGE_SIZE_2MB    (2ULL * 1024 * 1024)   /* 2097152 bytes */
#define PAGE_SIZE_1GB    (1ULL * 1024 * 1024 * 1024)  /* 1073741824 bytes */
#define PAGE_SHIFT_2MB   21
#define PAGE_SHIFT_1GB   30

/* Page table levels */
#define PML4_SHIFT       39
#define PDPT_SHIFT       30
#define PD_SHIFT         21
#define PT_SHIFT         12

/* Number of entries per page table */
#define PT_ENTRIES       512

/* ============================================================================
 * Page Table Entry Flag Bits
 * ============================================================================ */

#define PAGE_PRESENT     (1ULL << 0)   /* P: Page present in memory */
#define PAGE_WRITE       (1ULL << 1)   /* R/W: Read/write (1=writable) */
#define PAGE_USER        (1ULL << 2)   /* U/S: User/supervisor (1=user) */
#define PAGE_WRITE_THRU  (1ULL << 3)   /* PWT: Write-through caching */
#define PAGE_NO_CACHE    (1ULL << 4)   /* PCD: Disable cache */
#define PAGE_ACCESSED    (1ULL << 5)   /* A: Page was accessed */
#define PAGE_DIRTY       (1ULL << 6)   /* D: Page was written to */
#define PAGE_GLOBAL      (1ULL << 8)   /* G: Global page (ignored in user mode) */
#define PAGE_NO_EXEC     (1ULL << 63)  /* NX: No-execute bit (must be in bit 63) */

/* Huge page support */
#define PAGE_HUGE_PD    (1ULL << 7)    /* PS: Page size bit for PD (2MB pages) */
#define PAGE_HUGE_PDPT  (1ULL << 7)    /* PS: Page size bit for PDPT (1GB pages) */

/* Address mask (physical address bits in a PTE) */
#define PTE_ADDR_MASK    0x000FFFFFFFFFF000ULL  /* Bits 12-51 for physical address */

/* Standard kernel page flags */
#define PAGE_KERN_DEFAULT (PAGE_PRESENT | PAGE_WRITE)
#define PAGE_KERN_RO      (PAGE_PRESENT)
#define PAGE_USER_DEFAULT (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)
#define PAGE_USER_RO      (PAGE_PRESENT | PAGE_USER)

/* ============================================================================
 * Virtual Address Index Macros
 * ============================================================================ */

#define PML4_INDEX(vaddr) (((vaddr) >> PML4_SHIFT) & 0x1FF)
#define PDPT_INDEX(vaddr) (((vaddr) >> PDPT_SHIFT) & 0x1FF)
#define PD_INDEX(vaddr)  (((vaddr) >> PD_SHIFT) & 0x1FF)
#define PT_INDEX(vaddr)  (((vaddr) >> PT_SHIFT) & 0x1FF)

/* ============================================================================
 * Page Table Entry Structure
 * ============================================================================ */

/**
 * @brief Page table entry union for x86_64
 *
 * Provides both bit-level access and raw value access to page table entries.
 * The x86_64 page table entry format is:
 * - Bits 0:    Present (P)
 * - Bits 1:    Read/Write (R/W)
 * - Bits 2:    User/Supervisor (U/S)
 * - Bits 3:    Page-Level Write-Through (PWT)
 * - Bits 4:    Page-Level Cache Disable (PCD)
 * - Bits 5:    Accessed (A)
 * - Bits 6:    Dirty (D) - only for PD and PT entries
 * - Bits 7:    Page Size (PS) or PAT
 * - Bits 8:    Global (G)
 * - Bits 9-11: Available for OS use
 * - Bits 12-51: Physical frame number (40 bits)
 * - Bits 52-62: Available for OS use
 * - Bit 63:    No-execute (NX)
 */
typedef union {
    struct {
        uint64_t present      : 1;   /* Bit 0: Present */
        uint64_t writable     : 1;   /* Bit 1: Read/Write */
        uint64_t user         : 1;   /* Bit 2: User/Supervisor */
        uint64_t pwt          : 1;   /* Bit 3: Page-level Write-Through */
        uint64_t pcd          : 1;   /* Bit 4: Page-level Cache Disable */
        uint64_t accessed     : 1;   /* Bit 5: Accessed */
        uint64_t dirty        : 1;   /* Bit 6: Dirty (PD/PT only) */
        uint64_t pat          : 1;   /* Bit 7: Page-Attribute Table (or PS) */
        uint64_t global       : 1;   /* Bit 8: Global */
        uint64_t available    : 3;   /* Bits 9-11: Available for OS use */
        uint64_t frame        : 40;  /* Bits 12-51: Physical frame number */
        uint64_t available2   : 11;  /* Bits 52-62: Available for OS use */
        uint64_t nx           : 1;   /* Bit 63: No-Execute */
    } bits;
    uint64_t value;
} page_table_entry_t;

/* ============================================================================
 * Page Table Level Types
 * ============================================================================ */

/**
 * @brief Generic page table structure
 *
 * Each level of the x86_64 page table hierarchy uses the same structure:
 * an array of 512 page table entries, aligned to a 4KB boundary.
 */
typedef struct {
    page_table_entry_t entries[PT_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) page_table_t;

/* Type aliases for clarity */
typedef page_table_t pml4_t;   /* Page Map Level 4 */
typedef page_table_t pdpt_t;   /* Page Directory Pointer Table */
typedef page_table_t pd_t;     /* Page Directory */
typedef page_table_t pt_t;     /* Page Table */

/* ============================================================================
 * Page Table Result Codes
 * ============================================================================ */

typedef enum {
    PAGE_OK = 0,
    PAGE_ERR_NOT_INIT = -1,
    PAGE_ERR_OOM = -2,           /* Out of memory */
    PAGE_ERR_INVALID = -3,       /* Invalid argument */
    PAGE_ERR_NOT_PRESENT = -4,   /* Page not present */
    PAGE_ERR_ALREADY_MAPPED = -5,
    PAGE_ERR_NOT_MAPPED = -6,
    PAGE_ERR_ALIGNMENT = -7,
} page_result_t;

/* ============================================================================
 * Page Table Mapping Flags
 * ============================================================================ */

typedef enum {
    VMAP_FLAG_NONE = 0,
    VMAP_FLAG_WRITE = (1 << 0),      /* Writable mapping */
    VMAP_FLAG_USER = (1 << 1),       /* User-accessible mapping */
    VMAP_FLAG_NO_EXEC = (1 << 2),    /* No-execute mapping */
    VMAP_FLAG_WRITE_THRU = (1 << 3), /* Write-through caching */
    VMAP_FLAG_NO_CACHE = (1 << 4),   /* Cache disable */
    VMAP_FLAG_HUGE_2MB = (1 << 5),   /* Use 2MB huge pages */
    VMAP_FLAG_HUGE_1GB = (1 << 6),   /* Use 1GB huge pages */
} vmap_flags_t;

/* ============================================================================
 * Page Table Query Result
 * ============================================================================ */

typedef struct {
    physical_addr_t phys_addr;  /* Physical address of mapped page */
    uint64_t flags;             /* Page flags (combination of vmap_flags_t) */
    bool present;               /* Whether mapping exists */
    bool is_huge;               /* Whether this is a huge page (2MB/1GB) */
} page_query_result_t;

/* ============================================================================
 * CR Register Access Functions
 * ============================================================================ */

/**
 * @page_get_cr3 - Get the current CR3 register value
 * @return CR3 register value (contains PML4 physical address)
 */
static inline uint64_t page_get_cr3(void) {
    uint64_t cr3;
    __asm__ volatile("movq %%cr3, %0" : "=r"(cr3));
    return cr3;
}

/**
 * @page_set_cr3 - Set the CR3 register (load new page table)
 * @param cr3 CR3 value to load (physical address of PML4, must be 4KB aligned)
 *
 * Loading CR3 invalidates the TLB for all non-global pages.
 */
static inline void page_set_cr3(uint64_t cr3) {
    __asm__ volatile("movq %0, %%cr3" : : "r"(cr3) : "memory");
}

/**
 * @page_get_cr2 - Get the CR2 register value (page fault address)
 * @return CR2 register value (virtual address that caused the page fault)
 */
static inline uint64_t page_get_cr2(void) {
    uint64_t cr2;
    __asm__ volatile("movq %%cr2, %0" : "=r"(cr2));
    return cr2;
}

/**
 * @page_invalidate_tlb - Invalidate TLB entry for a specific address
 * @param vaddr Virtual address to invalidate
 */
static inline void page_invalidate_tlb(virtual_addr_t vaddr) {
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

/* ============================================================================
 * Page Table Management API
 * ============================================================================ */

/**
 * page_init - Initialize the page table management subsystem
 *
 * Reads CR3 to get the current PML4 and initializes internal state.
 * Must be called after pframe_init().
 *
 * @return PAGE_OK on success, error code otherwise
 */
page_result_t page_init(void);

/**
 * page_get_pml4 - Get the current PML4 physical address
 *
 * @return Physical address of the current PML4
 */
physical_addr_t page_get_pml4(void);

/**
 * page_create_table - Allocate and initialize a new page table
 *
 * Allocates a physical frame for a new page table and clears it.
 *
 * @param out_phys Pointer to store the physical address of allocated table
 * @return PAGE_OK on success, PAGE_ERR_OOM if out of memory
 */
page_result_t page_create_table(physical_addr_t* out_phys);

/**
 * page_free_table - Free a previously allocated page table
 *
 * @param phys Physical address of the table to free
 * @return PAGE_OK on success
 */
page_result_t page_free_table(physical_addr_t phys);

/**
 * page_map_page - Map a single virtual page to a physical page
 *
 * Creates a page table mapping from vaddr to paddr. If alloc_missing is true,
 * missing intermediate page tables will be allocated automatically.
 *
 * @param pml4_phys Physical address of the PML4 to use
 * @param vaddr Virtual address to map (must be page-aligned)
 * @param paddr Physical address to map to (must be page-aligned)
 * @param flags Mapping flags (VMAP_FLAG_*)
 * @param alloc_missing Whether to allocate missing page tables
 * @return PAGE_OK on success, error code otherwise
 */
page_result_t page_map_page(physical_addr_t pml4_phys,
                           virtual_addr_t vaddr,
                           physical_addr_t paddr,
                           uint64_t flags,
                           bool alloc_missing);

/**
 * page_unmap_page - Unmap a single virtual page
 *
 * Removes the page table mapping for vaddr. If free_table is true,
 * empty page tables will be freed.
 *
 * @param pml4_phys Physical address of the PML4
 * @param vaddr Virtual address to unmap (must be page-aligned)
 * @param free_table Whether to free empty page tables
 * @return PAGE_OK on success, error code otherwise
 */
page_result_t page_unmap_page(physical_addr_t pml4_phys,
                             virtual_addr_t vaddr,
                             bool free_table);

/**
 * page_query - Query the mapping of a virtual address
 *
 * @param pml4_phys Physical address of the PML4
 * @param vaddr Virtual address to query
 * @param result Pointer to store query result
 * @return PAGE_OK on success, PAGE_ERR_NOT_PRESENT if not mapped
 */
page_result_t page_query(physical_addr_t pml4_phys,
                        virtual_addr_t vaddr,
                        page_query_result_t* result);

/**
 * page_virt_to_phys - Convert virtual address to physical address
 *
 * @param pml4_phys Physical address of the PML4
 * @param vaddr Virtual address to translate
 * @param out_phys Pointer to store physical address
 * @return PAGE_OK if mapped, PAGE_ERR_NOT_PRESENT otherwise
 */
page_result_t page_virt_to_phys(physical_addr_t pml4_phys,
                               virtual_addr_t vaddr,
                               physical_addr_t* out_phys);

/**
 * page_map_range - Map a range of virtual pages to physical pages
 *
 * Maps a contiguous range of virtual pages to a contiguous range of
 * physical pages.
 *
 * @param pml4_phys Physical address of the PML4
 * @param vaddr_base Base virtual address
 * @param paddr_base Base physical address
 * @param page_count Number of pages to map
 * @param flags Mapping flags
 * @return PAGE_OK on success, error code otherwise
 */
page_result_t page_map_range(physical_addr_t pml4_phys,
                            virtual_addr_t vaddr_base,
                            physical_addr_t paddr_base,
                            uint64_t page_count,
                            uint64_t flags);

/**
 * page_unmap_range - Unmap a range of virtual pages
 *
 * @param pml4_phys Physical address of the PML4
 * @param vaddr_base Base virtual address
 * @param page_count Number of pages to unmap
 * @return PAGE_OK on success, error code otherwise
 */
page_result_t page_unmap_range(physical_addr_t pml4_phys,
                              virtual_addr_t vaddr_base,
                              uint64_t page_count);

/**
 * page_flush_cache - Flush CPU caches for a memory range
 *
 * @param vaddr Virtual address
 * @param size Size in bytes
 */
void page_flush_cache(virtual_addr_t vaddr, size_t size);

/**
 * page_dump_mapping - Dump page table mapping for debugging
 *
 * @param vaddr Virtual address to dump mapping for
 * @param levels Number of levels to dump (1-4, 4 = full detail)
 */
void page_dump_mapping(virtual_addr_t vaddr, int levels);

/**
 * page_dump_pml4 - Dump entire PML4 structure summary
 *
 * @param pml4_phys Physical address of PML4 to dump
 */
void page_dump_pml4(physical_addr_t pml4_phys);
