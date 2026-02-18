/* ==============================================================================
 * CCOS - Global Descriptor Table (GDT) for x86_64
 * ==============================================================================
 * This module provides GDT management including TSS loading.
 * In x86_64, most segmentation is disabled, but we still need GDT for:
 * - Code segment selectors (required by hardware)
 * - TSS segment for IST stacks
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"

/* ============================================================================
 * GDT Entry Structures
 * ============================================================================ */

/**
 * @brief GDT entry (normal segment descriptor)
 */
typedef struct PACKED {
    uint16_t limit_low;       /* Limit bits 0-15 */
    uint16_t base_low;        /* Base bits 0-15 */
    uint8_t  base_middle;     /* Base bits 16-23 */
    uint8_t  access;          /* Access byte */
    uint8_t  granularity;     /* Granularity flags */
    uint8_t  base_high;       /* Base bits 24-31 */
} __attribute__((packed)) gdt_entry_t;

/**
 * @brief GDT TSS entry (64-bit TSS descriptor)
 *
 * The x86_64 TSS descriptor is 16 bytes (128 bits) instead of 8.
 */
typedef struct PACKED {
    uint16_t limit_low;       /* Limit bits 0-15 */
    uint16_t base_low;        /* Base bits 0-15 */
    uint8_t  base_middle;     /* Base bits 16-23 */
    uint8_t  access;          /* Access byte (0x89 for available 64-bit TSS) */
    uint8_t  granularity;     /* Limit bits 16-19 and flags */
    uint8_t  base_high;       /* Base bits 24-31 */
    uint32_t base_upper;      /* Base bits 32-63 */
    uint32_t reserved;        /* Reserved, must be 0 */
} __attribute__((packed)) gdt_tss_entry_t;

/**
 * @brief GDT pointer structure (for lgdt instruction)
 */
typedef struct PACKED {
    uint16_t limit;           /* GDT size - 1 */
    uint64_t base;            /* GDT base address */
} __attribute__((packed)) gdt_ptr_t;

/* ============================================================================
 * GDT Selector Values
 * ============================================================================ */

/**
 * GDT selectors
 * These are the segment selector values that will be used in the kernel.
 */
#define GDT_NULL        0x00  /* Null descriptor */
#define GDT_KERNEL_CODE 0x08  /* Kernel 64-bit code */
#define GDT_KERNEL_DATA 0x10  /* Kernel data */
#define GDT_USER_CODE   0x18  /* User 64-bit code */
#define GDT_USER_DATA   0x20  /* User data */
#define GDT_TSS         0x28  /* TSS */

/* Access byte values */
#define GDT_ACCESS_PRESENT    (1 << 7)   /* Present bit */
#define GDT_ACCESS_DPL0       (0 << 5)   /* DPL 0 */
#define GDT_ACCESS_DPL3       (3 << 5)   /* DPL 3 */
#define GDT_ACCESS_SYSTEM     (1 << 4)   /* System flag (0 for system segments) */
#define GDT_ACCESS_TYPE_CODE  (0xA)      /* Code segment, execute/read */
#define GDT_ACCESS_TYPE_DATA  (0x2)      /* Data segment, read/write */
#define GDT_ACCESS_TYPE_TSS   (0x9)      /* 64-bit TSS (available) */

/* Granularity byte values */
#define GDT_GRANULARITY_4K    (1 << 7)   /* 4KB granularity */
#define GDT_GRANULARITY_32BIT (1 << 6)   /* 32-bit protected mode (ignored in long mode) */
#define GDT_GRANULARITY_64BIT (1 << 5)   /* 64-bit code segment */

/* ============================================================================
 * GDT API
 * ============================================================================ */

/**
 * @brief Initialize the kernel GDT
 *
 * Creates a new GDT with entries for kernel/user code/data and TSS.
 * This replaces the bootloader's GDT with our own.
 */
void gdt_init(void);

/**
 * @brief Load GDT and TSS
 *
 * Called by gdt_init(). Loads the GDT using lgdt and TSS using ltr.
 * Implemented in gdt.asm.
 */
void gdt_load(void);

/**
 * @brief Get GDT pointer (for debugging)
 * @return Pointer to GDT structure
 */
const gdt_ptr_t* gdt_get_ptr(void);

/**
 * @brief Dump GDT entries for debugging
 */
void gdt_dump(void);
