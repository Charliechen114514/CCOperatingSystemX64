/* ==============================================================================
 * CCOS - Exception Handlers for x86_64
 * ==============================================================================
 * This module provides handlers for critical exceptions:
 * - Double Fault (#DF)
 * - Stack Fault (#SS)
 * - General Protection Fault (#GP)
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"
#include "interrupt/idt.h"

/* No forward declaration needed - interrupt_frame_t is defined in idt.h */

/* ============================================================================
 * Exception Result Codes
 * ============================================================================ */

typedef enum {
    EXC_SUCCESS,           /* Exception handled, can continue */
    EXC_TERMINATE_PROCESS, /* Should terminate current process */
    EXC_KERNEL_PANIC,      /* Unrecoverable, must halt */
} exception_result_t;

/* ============================================================================
 * GPF Error Code Parsing
 * ============================================================================ */

/**
 * @brief General Protection Fault error code structure
 *
 * GPF error code format (x86_64):
 * - Bit 0: External event (1) or not (0)
 * - Bit 1: IDT descriptor violation (1) or table (0)
 * - Bit 2: GDT/LDT violation (1) or not (0)
 * - Bits 3-15: Selector index
 */
typedef struct {
    bool external;           /* External event (interrupt, etc.) */
    bool idt_descriptor;     /* IDT descriptor violation */
    bool gdt_table;          /* GDT/LDT table violation */
    bool ti;                 /* Table Indicator (0=GDT, 1=LDT) */
    uint16_t selector_index; /* Selector index if table violation */
} gpf_error_info_t;

/* ============================================================================
 * Exception Handler API
 * ============================================================================ */

/**
 * @brief Initialize exception handlers
 *
 * Registers handlers for Double Fault, Stack Fault, and GPF.
 * Should be called after IDT initialization.
 */
void exception_init(void);

/* ============================================================================
 * Specific Exception Handlers
 * ============================================================================ */

/**
 * @brief Double Fault handler (vector 8)
 *
 * Occurs when an exception happens while handling another exception.
 * This is a critical error that usually means stack corruption.
 *
 * @param frame Interrupt stack frame
 * @param error_code Error code (non-zero indicates nested page fault)
 */
void double_fault_handler(interrupt_frame_t* frame, uint64_t error_code);

/**
 * @brief Stack Fault handler (vector 12)
 *
 * Occurs on stack-related errors:
 * - Stack limit exceeded
 * - Stack segment not present
 * - Invalid stack segment reference
 *
 * @param frame Interrupt stack frame
 * @param error_code Error code with stack fault details
 */
void stack_fault_handler(interrupt_frame_t* frame, uint64_t error_code);

/**
 * @brief General Protection Fault handler (vector 13)
 *
 * Occurs on various protection violations:
 * - Privileged instruction in user mode
 * - I/O port access without permission
 * - Writing to read-only memory
 * - Segment violations
 *
 * @param frame Interrupt stack frame
 * @param error_code Error code with GPF details
 */
void gp_fault_handler(interrupt_frame_t* frame, uint64_t error_code);

/* ============================================================================
 * Error Code Parsing
 * ============================================================================ */

/**
 * @brief Parse GPF error code
 * @param error_code Raw error code from CPU
 * @param info Pointer to structure to fill
 */
void gp_parse_error_code(uint64_t error_code, gpf_error_info_t* info);

/**
 * @brief Parse Stack Fault error code
 * @param error_code Raw error code from CPU
 * @return String describing the error
 */
const char* ss_parse_error_code(uint64_t error_code);

/* ============================================================================
 * Exception Statistics
 * ============================================================================ */

typedef struct {
    uint64_t df_count;        /* Double Fault count */
    uint64_t ss_count;        /* Stack Fault count */
    uint64_t gp_count;        /* GPF count */
    uint64_t gp_user_count;   /* User-mode GPF count */
    uint64_t gp_kernel_count; /* Kernel-mode GPF count */
} exception_stats_t;

/**
 * @brief Get exception statistics
 * @param stats Pointer to stats structure to fill
 */
void exception_get_stats(exception_stats_t* stats);
