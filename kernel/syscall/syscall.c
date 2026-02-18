/**
 * @file syscall.c
 * @brief System call framework implementation
 */

#include "syscall.h"
#include "base/memory.h"
#include "interrupt/gdt.h"
#include "interrupt/idt.h"
#include "interrupt/idt_constants.h"
#include "klogs/kprintf.h"

/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief System call handler entry
 */
typedef struct {
    syscall_handler_fn handler; /* Handler function */
    const char* name;           /* Name for debugging */
    bool registered;            /* Whether handler is registered */
} syscall_entry_t;

/**
 * @brief System call table
 */
static syscall_entry_t s_syscall_table[SYS_MAX] = {0};

/**
 * @brief System call statistics
 */
static syscall_stats_t s_stats = {0};

/**
 * @brief Initialization flag
 */
static bool s_initialized = false;

/* ============================================================================
 * Default/Stub Handlers
 * ============================================================================ */

/**
 * @brief Default handler for unimplemented syscalls
 */
static int64_t syscall_not_impl(syscall_frame_t* frame) {
    (void)frame;
    s_stats.not_impl_count++;
    s_stats.errors++;
    klog_warn("[SYSCALL] Unimplemented syscall: %lu\n", frame->syscall_number);
    return SYS_ERR_NOTIMPL;
}

/**
 * @brief Debug log syscall (for testing)
 */
static int64_t syscall_debug_log(syscall_frame_t* frame) {
    const char* msg = (const char*)frame->arg0;
    uint64_t len = frame->arg1;
    klog_info("[USER LOG] %.*s\n", (int)len, msg);
    return SYS_OK;
}

/**
 * @brief Test syscall (returns input value)
 */
static int64_t syscall_test(syscall_frame_t* frame) {
    return (int64_t)frame->arg0; /* Echo back first argument */
}

/* ============================================================================
 * CPUID Feature Detection
 * ============================================================================ */

/**
 * @brief Check if CPU supports syscall/sysret
 */
bool syscall_is_available(void) {
    uint32_t eax, ebx, ecx, edx;
    /* CPUID leaf 0x80000001, EDX bit 11 = syscall/sysret support */
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000001));
    return (edx & (1 << 11)) != 0;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */
extern void syscall_enable();
extern void syscall_register_all(void);

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

void syscall_init(void) {
    if (s_initialized) {
        klog_warn("[SYSCALL] Already initialized\n");
        return;
    }

    klog_info("[SYSCALL] Initializing system call framework...\n");

    /* Check CPU support */
    if (!syscall_is_available()) {
        klog_error("[SYSCALL] CPU does not support syscall/sysret\n");
        return;
    }
    klog_info("[SYSCALL] syscall/sysret supported\n");

    /* Clear syscall table */
    memset(s_syscall_table, 0, sizeof(s_syscall_table));

    /* Register default handlers */
    syscall_register_handler(SYS_DEBUG_LOG, syscall_debug_log, "debug_log");
    syscall_register_handler(SYS_TEST, syscall_test, "test");

    /* Register all syscall handlers from syscall_table.c */
    syscall_register_all();

    klog_trace("[SYSCALL] About to enable SCE bit in CR4...\n");

    /* Enable SCE bit in CR4 */
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    klog_trace("[SYSCALL] Current CR4 = 0x%016llX\n", cr4);

    if (cr4 & (1 << 11)) {
        klog_trace("[SYSCALL] SCE bit already set in CR4\n");
    } else {
        cr4 |= (1 << 11);
        klog_trace("[SYSCALL] Setting CR4 to 0x%016llX (before write)\n", cr4);

        /* Direct CR4 write - very simple */
        __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));

        klog_trace("[SYSCALL] CR4 write completed\n");
    }

    klog_trace("[SYSCALL] SCE bit enabled in CR4\n");

    /* Configure MSR registers */
    extern void syscall_handler(void);
    uint64_t lstar = (uint64_t)syscall_handler;

    klog_trace("[SYSCALL] About to write IA32_LSTAR...\n");
    /* Configure IA32_LSTAR (0xC0000082) - syscall entry point */
    wrmsr(0xC0000082, lstar);
    klog_trace("[SYSCALL] IA32_LSTAR = 0x%016llX\n", lstar);

    /* Configure IA32_STAR (0xC0000081)
     * STAR[63:48] = SYSRET CS (user mode code) = GDT_USER_CODE | 3 = 0x18 | 3 = 0x1B
     * STAR[47:32] = syscall CS (kernel mode code) = GDT_KERNEL_CODE = 0x08
     */
    klog_trace("[SYSCALL] About to write IA32_STAR...\n");
    uint64_t star = ((uint64_t)(GDT_USER_CODE | 3) << 48) | ((uint64_t)GDT_KERNEL_CODE << 32);
    wrmsr(0xC0000081, star);
    klog_trace("[SYSCALL] IA32_STAR = 0x%016llX\n", star);

    klog_trace("[SYSCALL] About to write IA32_FMASK...\n");
    /* Configure IA32_FMASK (0xC0000084) - RFLAGS bits to clear on syscall */
    wrmsr(0xC0000084, SYSCALL_FMASK_DEFAULT);
    klog_trace("[SYSCALL] IA32_FMASK = 0x%08X\n", SYSCALL_FMASK_DEFAULT);

    /* Register int 0x80 as fallback (vector 128, after IRQ range) */
    extern void int0x80_handler(void);
    idt_set_gate(128, (uint64_t)int0x80_handler, IDT_USER_INTERRUPT_GATE, GDT_KERNEL_CODE);
    klog_trace("[SYSCALL] int 0x80 fallback registered at vector 128\n");

    s_initialized = true;
    klog_info("[SYSCALL] System call framework initialized\n");
    klog_info("[SYSCALL]   syscall/sysret: enabled\n");
    klog_info("[SYSCALL]   int 0x80 fallback: vector 128\n");
}

int syscall_register_handler(uint64_t number, syscall_handler_fn handler, const char* name) {
    if (number >= SYS_MAX) {
        klog_error("[SYSCALL] Invalid syscall number: %lu\n", number);
        return -1;
    }

    if (handler == NULL) {
        klog_error("[SYSCALL] NULL handler for syscall %lu\n", number);
        return -2;
    }

    syscall_entry_t* entry = &s_syscall_table[number];
    entry->handler = handler;
    entry->name = name;
    entry->registered = true;

    klog_trace("[SYSCALL] Registered syscall %lu: %s\n", number, name ? name : "unnamed");
    return 0;
}

int64_t syscall_dispatch(syscall_frame_t* frame, uint64_t arg5) {
    /* Update statistics */
    s_stats.total_calls++;

    uint64_t number = frame->syscall_number;

    if (number >= SYS_MAX) {
        s_stats.errors++;
        return SYS_ERR_INVAL;
    }

    /* Store arg5 in frame for handler access */
    frame->arg5 = arg5;

    /* Update per-syscall counter */
    if (number < 256) {
        s_stats.syscall_calls[number]++;
    }

    /* Get handler */
    syscall_entry_t* entry = &s_syscall_table[number];

    if (!entry->registered || entry->handler == NULL) {
        return syscall_not_impl(frame);
    }

    /* Call handler */
    return entry->handler(frame);
}

int64_t syscall_dispatch_int80(syscall_frame_int80_t* frame) {
    /* Convert to regular frame and dispatch */
    syscall_frame_t regular_frame;
    regular_frame.syscall_number = frame->syscall_number;
    regular_frame.arg0 = frame->arg0;
    regular_frame.arg1 = frame->arg1;
    regular_frame.arg2 = frame->arg2;
    regular_frame.arg3 = frame->arg3;
    regular_frame.arg4 = frame->arg4;

    return syscall_dispatch(&regular_frame, frame->arg5);
}

void syscall_get_stats(syscall_stats_t* stats) {
    if (stats) {
        *stats = s_stats;
    }
}

void syscall_dump_stats(void) {
    klog_info("[SYSCALL] Statistics:\n");
    klog_info("[SYSCALL]   Total calls: %lu\n", s_stats.total_calls);
    klog_info("[SYSCALL]   Errors:      %lu\n", s_stats.errors);
    klog_info("[SYSCALL]   Not impl:    %lu\n", s_stats.not_impl_count);

    /* Show top 5 most called syscalls */
    klog_info("[SYSCALL]   Top syscalls:\n");
    uint64_t top_counts[5] = {0};
    uint64_t top_numbers[5] = {0};

    for (uint64_t i = 0; i < 256; i++) {
        if (s_stats.syscall_calls[i] > top_counts[0]) {
            top_counts[4] = top_counts[3];
            top_numbers[4] = top_numbers[3];
            top_counts[3] = top_counts[2];
            top_numbers[3] = top_numbers[2];
            top_counts[2] = top_counts[1];
            top_numbers[2] = top_numbers[1];
            top_counts[1] = top_counts[0];
            top_numbers[1] = top_numbers[0];
            top_counts[0] = s_stats.syscall_calls[i];
            top_numbers[0] = i;
        } else if (s_stats.syscall_calls[i] > top_counts[1]) {
            top_counts[4] = top_counts[3];
            top_numbers[4] = top_numbers[3];
            top_counts[3] = top_counts[2];
            top_numbers[3] = top_numbers[2];
            top_counts[2] = top_counts[1];
            top_numbers[2] = top_numbers[1];
            top_counts[1] = s_stats.syscall_calls[i];
            top_numbers[1] = i;
        } else if (s_stats.syscall_calls[i] > top_counts[2]) {
            top_counts[4] = top_counts[3];
            top_numbers[4] = top_numbers[3];
            top_counts[3] = top_counts[2];
            top_numbers[3] = top_numbers[2];
            top_counts[2] = s_stats.syscall_calls[i];
            top_numbers[2] = i;
        } else if (s_stats.syscall_calls[i] > top_counts[3]) {
            top_counts[4] = top_counts[3];
            top_numbers[4] = top_numbers[3];
            top_counts[3] = s_stats.syscall_calls[i];
            top_numbers[3] = i;
        } else if (s_stats.syscall_calls[i] > top_counts[4]) {
            top_counts[4] = s_stats.syscall_calls[i];
            top_numbers[4] = i;
        }
    }

    for (int i = 0; i < 5; i++) {
        if (top_counts[i] > 0) {
            const char* name = s_syscall_table[top_numbers[i]].name;
            klog_info("[SYSCALL]     %lu: %lu calls (%s)\n", top_numbers[i], top_counts[i],
                      name ? name : "unknown");
        }
    }
}
