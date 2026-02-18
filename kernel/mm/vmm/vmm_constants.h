/* ==============================================================================
 * CCOS - VMM Constants
 * ==============================================================================
 * Hardware-related constants for x86_64 virtual memory management.
 * These values are derived from the x86_64 architecture specification and
 * should not be modified unless porting to a different architecture.
 * ==============================================================================
 */

#pragma once

#include "mm/page_config.h"     /* For PAGE_SIZE, PAGE_SHIFT, huge page sizes */

/* Page table levels */
#define PML4_SHIFT       39
#define PDPT_SHIFT       30
#define PD_SHIFT         21
#define PT_SHIFT         12

/* Number of entries per page table */
#define PT_ENTRIES       512

/* ============================================================================
 * Page Table Entry Flag Bits
 * ============================================================================
 * x86_64 hardware-defined page table entry flags
 */

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
 * Copy-on-Write Constants
 * ============================================================================ */

/**
 * COW flag - stored in PTE available bit 9
 * When set, this page is shared and should trigger COW on write.
 */
#define COW_FLAG_MASK    (1ULL << 9)

/**
 * Maximum reference count for a COW page
 */
#define COW_MAX_REFCOUNT 0xFFFF

/* ============================================================================
 * Page Fault Error Code Bits
 * ============================================================================
 * The page fault error code is pushed by the CPU when a #PF occurs.
 * It provides information about the cause of the fault.
 */

#define PF_ERR_PRESENT   (1 << 0)  /* Bit 0: P=0 if page not present, P=1 if protection fault */
#define PF_ERR_WRITE     (1 << 1)  /* Bit 1: W=1 if write operation, W=0 if read */
#define PF_ERR_USER      (1 << 2)  /* Bit 2: U=1 if user mode, U=0 if supervisor mode */
#define PF_ERR_RESERVED  (1 << 3)  /* Bit 3: Reserved bit set in page table */
#define PF_ERR_INSTR     (1 << 4)  /* Bit 4: I=1 if instruction fetch, I=0 if data access */
