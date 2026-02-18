/**
 * @file syscall.h
 * @brief System call framework for x86_64
 */

#pragma once

#include "defines/types.h"
#include "syscall_numbers.h"

/* ============================================================================
 * System Call Frame
 * ============================================================================ */

/**
 * @brief System call frame structure
 *
 * This structure represents the state passed from user mode to kernel mode
 * during a system call. It follows the System V AMD64 ABI convention.
 */
typedef struct PACKED {
    uint64_t syscall_number;    /* System call number (from RAX) */
    uint64_t arg0;              /* First argument (RDI) */
    uint64_t arg1;              /* Second argument (RSI) */
    uint64_t arg2;              /* Third argument (RDX) */
    uint64_t arg3;              /* Fourth argument (R10) */
    uint64_t arg4;              /* Fifth argument (R8) */
    uint64_t arg5;              /* Sixth argument (R9) - stored separately */
} syscall_frame_t;

/**
 * @brief Extended frame for legacy int 0x80 (includes arg5 inline)
 */
typedef struct PACKED {
    uint64_t syscall_number;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    uint64_t arg5;              /* Arg5 inline for int 0x80 */
} syscall_frame_int80_t;

/* ============================================================================
 * System Call Handler Function Type
 * ============================================================================ */

/**
 * @brief Type for system call handler functions
 *
 * @param frame Pointer to the syscall frame
 * @return int64_t Return value (negative for errors, per syscall_result_t)
 */
typedef int64_t (*syscall_handler_fn)(syscall_frame_t* frame);

/* ============================================================================
 * System Call Statistics
 * ============================================================================ */

/**
 * @brief System call statistics
 */
typedef struct {
    uint64_t total_calls;       /* Total syscall invocations */
    uint64_t syscall_calls[256];/* Per-syscall call count */
    uint64_t errors;            /* Total errors */
    uint64_t not_impl_count;    /* Unimplemented syscall count */
} syscall_stats_t;

/* ============================================================================
 * MSR Configuration Values
 * ============================================================================ */

/* FMASK: RFLAGS bits to clear on syscall entry */
#define SYSCALL_FMASK_DEFAULT 0x200  /* Clear IF (interrupt flag) */

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize the system call framework
 *
 * This function:
 * 1. Enables the SCE (System Call Extension) bit in CR4
 * 2. Configures MSR registers for syscall/sysret
 * 3. Sets up the syscall handler table
 * 4. Registers the int 0x80 fallback
 *
 * Must be called after GDT/TSS initialization.
 */
void syscall_init(void);

/**
 * @brief Register a system call handler
 *
 * @param number System call number
 * @param handler Handler function
 * @param name Human-readable name (for debugging)
 * @return 0 on success, negative on error
 */
int syscall_register_handler(uint64_t number, syscall_handler_fn handler, const char* name);

/**
 * @brief System call dispatcher (called from assembly stub)
 *
 * @param frame Pointer to syscall frame
 * @param arg5 Sixth argument (passed in R9, separate from frame)
 * @return int64_t Return value
 */
int64_t syscall_dispatch(syscall_frame_t* frame, uint64_t arg5);

/**
 * @brief System call dispatcher for int 0x80
 *
 * @param frame Pointer to syscall frame (with inline arg5)
 * @return int64_t Return value
 */
int64_t syscall_dispatch_int80(syscall_frame_int80_t* frame);

/**
 * @brief Get system call statistics
 *
 * @param stats Pointer to stats structure to fill
 */
void syscall_get_stats(syscall_stats_t* stats);

/**
 * @brief Dump system call statistics for debugging
 */
void syscall_dump_stats(void);

/**
 * @brief Check if syscall/sysret is available
 *
 * @return true if CPU supports syscall/sysret
 */
bool syscall_is_available(void);

/* ============================================================================
 * MSR Access Functions
 * ============================================================================ */

/**
 * @brief Read an MSR register
 *
 * @param msr MSR address
 * @return uint64_t MSR value
 */
static inline __attribute__((always_inline)) uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

/**
 * @brief Write an MSR register
 *
 * @param msr MSR address
 * @param value Value to write
 */
static inline __attribute__((always_inline)) void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high) : "memory");
}
