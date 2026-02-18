/* ==============================================================================
 * CCOS - VMM Configuration
 * ==============================================================================
 * Configurable parameters for virtual memory management.
 * These values can be adjusted to change the system's memory layout and behavior.
 * ==============================================================================
 */

#pragma once

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

/* Kernel heap region */
#define KERNEL_HEAP_BASE        0xFFFFFFFF81000000ULL
#define KERNEL_HEAP_SIZE        (128 * 1024 * 1024)   /* 128MB */
#define KERNEL_HEAP_MAX         (KERNEL_HEAP_BASE + KERNEL_HEAP_SIZE)

/* General kernel allocation region (for VMM demo, temporary mappings, etc.) */
#define KERNEL_GENERAL_BASE     0xFFFFFFFF88000000ULL
#define KERNEL_GENERAL_SIZE     (128 * 1024 * 1024)   /* 128MB */
#define KERNEL_GENERAL_MAX      (KERNEL_GENERAL_BASE + KERNEL_GENERAL_SIZE)

/* Physical map offset for direct mapping */
#define PHYS_MAP_OFFSET         KERNEL_VIRT_BASE

/* User space boundaries */
#define USER_BASE               0x0000000000400000ULL  /* 4MB (skip NULL page) */
#define USER_END                0x00007FFFFFFFFFFFULL  /* 128TB user limit */

/* ============================================================================
 * Copy-on-Write Configuration
 * ============================================================================ */

/* Default hash table size for COW tracking */
#define COW_HASH_SIZE  256

/* ============================================================================
 * Page Fault Handler Configuration
 * ============================================================================ */

/* Enable demand paging feature */
#define PF_ENABLE_DEMAND_PAGING  1
