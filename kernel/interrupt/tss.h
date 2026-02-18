/* ==============================================================================
 * CCOS - Task State Segment (TSS) for x86_64
 * ==============================================================================
 * This module provides TSS management for the kernel. The TSS is used for:
 * - Setting kernel stack pointer for user->kernel transitions (RSP0)
 * - Interrupt Stack Table (IST) for handling critical exceptions
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"

/* ============================================================================
 * TSS Structure for x86_64
 * ============================================================================ */

/**
 * @brief Task State Segment structure
 *
 * The x86_64 TSS is much simpler than the 32-bit version.
 * It mainly stores stack pointers for privilege level changes and IST entries.
 */
typedef struct PACKED tss {
    uint32_t reserved0;    /* Reserved */
    uint64_t rsp0;         /* Ring 0 stack pointer (used for user->kernel) */
    uint64_t rsp1;         /* Ring 1 stack pointer (unused) */
    uint64_t rsp2;         /* Ring 2 stack pointer (unused) */
    uint64_t reserved1;    /* Reserved */
    uint64_t ist1;         /* IST1: Double Fault stack */
    uint64_t ist2;         /* IST2: NMI stack */
    uint64_t ist3;         /* IST3: Debug stack */
    uint64_t ist4;         /* IST4: Stack Fault stack */
    uint64_t ist5;         /* IST5: (reserved) */
    uint64_t ist6;         /* IST6: (reserved) */
    uint64_t ist7;         /* IST7: (reserved) */
    uint64_t reserved2;    /* Reserved */
    uint16_t reserved3;    /* Reserved */
    uint16_t iomap_base;   /* I/O permission bitmap base offset (0xFFFF = none) */
} __attribute__((aligned(16))) tss_t;

/* ============================================================================
 * IST (Interrupt Stack Table) Indices
 * ============================================================================ */

#define IST_NONE     0    /* No IST switch */
#define IST_DF       1    /* Double Fault (#DF) uses IST1 */
#define IST_NMI      2    /* NMI uses IST2 */
#define IST_DEBUG    3    /* Debug exception uses IST3 */
#define IST_SS       4    /* Stack Fault (#SS) uses IST4 */

/* ============================================================================
 * IST Stack Configuration
 * ============================================================================ */

/**
 * Stack size for each IST entry
 * 16KB should be sufficient for exception handling
 */
#define IST_STACK_SIZE  (16 * 1024)  /* 16KB */

/* ============================================================================
 * TSS API
 * ============================================================================ */

/**
 * @brief Initialize the TSS
 *
 * Allocates IST stacks and sets up the TSS structure.
 * Must be called before loading the TSS into the GDT.
 */
void tss_init(void);

/**
 * @brief Get a pointer to the TSS structure
 * @return Pointer to the TSS
 */
tss_t* tss_get(void);

/**
 * @brief Set the kernel stack for user->kernel transitions
 * @param stack_top Top of the kernel stack (stack grows down)
 */
static inline void tss_set_kernel_stack(virtual_addr_t stack_top) {
    tss_t* tss = tss_get();
    if (tss) {
        tss->rsp0 = stack_top;
    }
}

/**
 * @brief Get the kernel stack for user->kernel transitions
 * @return Top of the kernel stack
 */
static inline virtual_addr_t tss_get_kernel_stack(void) {
    tss_t* tss = tss_get();
    return tss ? tss->rsp0 : 0;
}

/**
 * @brief Set an IST stack
 * @param ist_index IST index (1-7)
 * @param stack_top Top of the stack (stack grows down)
 */
void tss_set_ist_stack(uint8_t ist_index, virtual_addr_t stack_top);

/**
 * @brief Get an IST stack
 * @param ist_index IST index (1-7)
 * @return Top of the stack, or 0 if invalid index
 */
virtual_addr_t tss_get_ist_stack(uint8_t ist_index);

/**
 * @brief Dump TSS state for debugging
 */
void tss_dump(void);
