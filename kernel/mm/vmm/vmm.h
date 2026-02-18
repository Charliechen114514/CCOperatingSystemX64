/* ==============================================================================
 * CCOS - Virtual Memory Manager
 * ==============================================================================
 * This module provides high-level virtual memory management, including
 * address space layout, kernel page allocation, and user/kernel space
 * isolation for the x86_64 architecture.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"
#include "mm/pframe/pframe.h"
#include "mm/vmm/page.h"

/* ==============================================================================
 * Virtual Memory Address Space Layout
 * ==============================================================================
 *
 * x86_64 Canonical Address Space:
 * - 0x0000000000000000 - 0x00007FFFFFFFFFFF : User Space (128 TB)
 * - Non-canonical hole
 * - 0xFFFF800000000000 - 0xFFFFFFFFFFFFFFFF : Kernel Space (128 TB)
 *
 * Kernel Layout (higher half):
 */

/* Direct physical map region */
#define KERNEL_VIRT_BASE        0xFFFF800000000000ULL
#define KERNEL_VIRT_END         0xFFFF800001000000ULL  /* 256MB direct map */

/* Kernel code/data regions */
#define KERNEL_TEXT_BASE        0xFFFFFFFF80000000ULL
#define KERNEL_TEXT_SIZE        (2 * 1024 * 1024)     /* 2MB */
#define KERNEL_DATA_BASE        (KERNEL_TEXT_BASE + KERNEL_TEXT_SIZE)
#define KERNEL_DATA_SIZE        (2 * 1024 * 1024)     /* 2MB */

/* Kernel heap region (for future kmalloc implementation) */
#define KERNEL_HEAP_BASE        0xFFFFFFFF81000000ULL
#define KERNEL_HEAP_SIZE        (128 * 1024 * 1024)   /* 128MB */
#define KERNEL_HEAP_MAX         (KERNEL_HEAP_BASE + KERNEL_HEAP_SIZE)

/* Physical map offset for direct mapping */
#define PHYS_MAP_OFFSET         KERNEL_VIRT_BASE

/* User space boundaries */
#define USER_BASE               0x0000000000400000ULL  /* 4MB (skip NULL page) */
#define USER_END                0x00007FFFFFFFFFFFULL  /* 128TB user limit */

/* ============================================================================
 * Address Conversion Macros
 * ============================================================================ */

/**
 * phys_to_virt - Convert physical address to kernel virtual address
 * @param phys Physical address
 * @return Kernel virtual address in direct-mapped region
 */
static inline virtual_addr_t phys_to_virt_offset(physical_addr_t phys) {
    return phys + PHYS_MAP_OFFSET;
}

/**
 * virt_to_phys - Convert kernel virtual address to physical address
 * @param virt Kernel virtual address in direct-mapped region
 * @return Physical address
 */
static inline physical_addr_t virt_to_phys_offset(virtual_addr_t virt) {
    return virt - PHYS_MAP_OFFSET;
}

/* ============================================================================
 * VMM Result Codes
 * ============================================================================ */

#ifndef VMM_RESULT_T_DEFINED
#define VMM_RESULT_T_DEFINED
typedef enum {
    VMM_OK = 0,
    VMM_ERR_NOT_INIT = -1,
    VMM_ERR_OOM = -2,
    VMM_ERR_INVALID = -3,
    VMM_ERR_PERM = -4,          /* Permission denied */
    VMM_ERR_NOT_MAPPED = -5,
    VMM_ERR_ALREADY_MAPPED = -6,
} vmm_result_t;
#endif

/* ============================================================================
 * Memory Region Descriptor
 * ============================================================================ */

/**
 * @brief Memory region descriptor for tracking mapped areas
 */
typedef struct memory_region {
    virtual_addr_t start;       /* Region start (inclusive) */
    virtual_addr_t end;         /* Region end (exclusive) */
    physical_addr_t phys_start; /* Physical address (0 if unmapped) */
    uint64_t flags;             /* Protection flags */
    char name[32];              /* Region name for debugging */
} memory_region_t;

/* ============================================================================
 * VMM Statistics
 * ============================================================================ */

typedef struct {
    uint64_t total_pages;       /* Total virtual pages managed */
    uint64_t mapped_pages;      /* Currently mapped pages */
    uint64_t kernel_pages;      /* Kernel space pages */
    uint64_t user_pages;        /* User space pages */
    uint64_t page_tables;       /* Number of page tables allocated */
} vmm_stats_t;

/* ============================================================================
 * VMM Core API
 * ============================================================================ */

/**
 * vmm_init - Initialize the virtual memory manager
 *
 * Sets up kernel address space, initializes page tables,
 * and prepares memory regions. Must be called after page_init().
 *
 * @return VMM_OK on success, error code otherwise
 */
vmm_result_t vmm_init(void);

/**
 * vmm_get_kernel_pml4 - Get the kernel's PML4 physical address
 *
 * @return Physical address of kernel PML4
 */
physical_addr_t vmm_get_kernel_pml4(void);

/**
 * vmm_map_physical - Map a physical page into kernel virtual address space
 *
 * Creates a temporary mapping for a physical page. The virtual address
 * is chosen from the kernel's available mapping region.
 *
 * @param phys Physical address to map
 * @param flags Mapping flags (VMAP_FLAG_*)
 * @return Virtual address of mapping, or 0 on failure
 */
virtual_addr_t vmm_map_physical(physical_addr_t phys, uint64_t flags);

/**
 * vmm_unmap_physical - Unmap a previously mapped physical page
 *
 * @param virt Virtual address to unmap
 * @return VMM_OK on success, error code otherwise
 */
vmm_result_t vmm_unmap_physical(virtual_addr_t virt);

/**
 * vmm_alloc_pages - Allocate and map virtual pages
 *
 * Allocates physical frames and maps them into kernel virtual address space.
 *
 * @param count Number of pages to allocate
 * @param flags Mapping flags (VMAP_FLAG_*)
 * @return Virtual address base, or 0 on failure
 */
virtual_addr_t vmm_alloc_pages(uint64_t count, uint64_t flags);

/**
 * vmm_alloc_pages_at - Allocate and map pages at a specific virtual address
 *
 * @param vaddr Virtual address to map at (must be page-aligned)
 * @param count Number of pages to allocate
 * @param flags Mapping flags (VMAP_FLAG_*)
 * @return VMM_OK on success, error code otherwise
 */
vmm_result_t vmm_alloc_pages_at(virtual_addr_t vaddr, uint64_t count, uint64_t flags);

/**
 * vmm_free_pages - Free previously allocated virtual pages
 *
 * Unmaps the virtual pages and frees the associated physical frames.
 *
 * @param virt Virtual address base
 * @param count Number of pages to free
 * @return VMM_OK on success, error code otherwise
 */
vmm_result_t vmm_free_pages(virtual_addr_t virt, uint64_t count);

/**
 * vmm_get_stats - Get VMM statistics
 *
 * @param stats Pointer to statistics structure to fill
 * @return VMM_OK on success, error code otherwise
 */
vmm_result_t vmm_get_stats(vmm_stats_t* stats);

/**
 * vmm_dump - Dump VMM state for debugging
 */
void vmm_dump(void);

/* ============================================================================
 * Address Space Management
 * ============================================================================ */

/**
 * vmm_is_kernel_addr - Check if an address is in kernel space
 *
 * @param addr Virtual address to check
 * @return true if address is in kernel space
 */
static inline bool vmm_is_kernel_addr(virtual_addr_t addr) {
    return addr >= KERNEL_VIRT_BASE;
}

/**
 * vmm_is_user_addr - Check if an address is in user space
 *
 * @param addr Virtual address to check
 * @return true if address is in user space
 */
static inline bool vmm_is_user_addr(virtual_addr_t addr) {
    return addr >= USER_BASE && addr < USER_END;
}

/**
 * vmm_is_canonical - Check if an address is canonical (valid x86_64 address)
 *
 * @param addr Virtual address to check
 * @return true if address is canonical
 */
static inline bool vmm_is_canonical(virtual_addr_t addr) {
    /* x86_64 canonical addresses have bits 63:48 all 0 or all 1 */
    return ((addr >> 48) == 0) || ((addr >> 48) == 0xFFFF);
}

/**
 * vmm_create_user_space - Create a new user address space
 *
 * Creates a new page table structure for a user process.
 *
 * @param out_pml4 Pointer to store the new PML4 physical address
 * @return VMM_OK on success, error code otherwise
 */
vmm_result_t vmm_create_user_space(physical_addr_t* out_pml4);

/**
 * vmm_destroy_user_space - Destroy a user address space
 *
 * Frees all page tables associated with a user address space.
 *
 * @param pml4 Physical address of the PML4 to destroy
 * @return VMM_OK on success, error code otherwise
 */
vmm_result_t vmm_destroy_user_space(physical_addr_t pml4);

/**
 * vmm_map_to_user - Map pages into a user address space
 *
 * @param pml4 User space PML4 physical address
 * @param vaddr Virtual address in user space
 * @param paddr Physical address to map
 * @param count Number of pages to map
 * @param flags Mapping flags (VMAP_FLAG_* including VMAP_FLAG_USER)
 * @return VMM_OK on success, error code otherwise
 */
vmm_result_t vmm_map_to_user(physical_addr_t pml4,
                            virtual_addr_t vaddr,
                            physical_addr_t paddr,
                            uint64_t count,
                            uint64_t flags);

/**
 * vmm_dump_memory_map - Dump virtual to physical memory mappings
 *
 * Displays a table showing virtual addresses and their corresponding
 * physical addresses for debugging purposes.
 *
 * @param pml4_phys Physical address of PML4 to dump (use 0 for kernel PML4)
 * @param max_entries Maximum number of entries to display
 */
void vmm_dump_memory_map(physical_addr_t pml4_phys, uint32_t max_entries);
