/* ==============================================================================
 * CCOS - Task State Segment (TSS) Implementation
 * ==============================================================================
 */

#include "interrupt/tss.h"
#include "mm/vmm/vmm.h"
#include "mm/heap/heap.h"
#include "klogs/kprintf.h"

/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief Single TSS for the entire system
 *
 * Since we don't support hardware task switching, we only need one TSS.
 * For SMP systems in the future, each CPU will have its own TSS.
 */
static tss_t s_tss = {0};

/**
 * @brief IST stack storage
 *
 * We allocate these from the kernel heap during initialization.
 */
static struct {
    uint8_t* df_stack;    /* Double Fault stack */
    uint8_t* nmi_stack;   /* NMI stack */
    uint8_t* debug_stack; /* Debug stack */
    uint8_t* ss_stack;    /* Stack Fault stack */
} s_ist_stacks = {
    .df_stack = NULL,
    .nmi_stack = NULL,
    .debug_stack = NULL,
    .ss_stack = NULL
};

/* Flag to track if TSS has been initialized */
static bool s_initialized = false;

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void tss_init(void) {
    if (s_initialized) {
        klog_warn("[TSS] Already initialized\n");
        return;
    }

    klog_info("[TSS] Initializing Task State Segment...\n");

    /* Clear TSS structure */
    for (size_t i = 0; i < sizeof(tss_t); i++) {
        ((uint8_t*)&s_tss)[i] = 0;
    }

    /* Allocate IST stacks */
    s_ist_stacks.df_stack = (uint8_t*)kmalloc(IST_STACK_SIZE);
    s_ist_stacks.nmi_stack = (uint8_t*)kmalloc(IST_STACK_SIZE);
    s_ist_stacks.debug_stack = (uint8_t*)kmalloc(IST_STACK_SIZE);
    s_ist_stacks.ss_stack = (uint8_t*)kmalloc(IST_STACK_SIZE);

    if (!s_ist_stacks.df_stack || !s_ist_stacks.nmi_stack ||
        !s_ist_stacks.debug_stack || !s_ist_stacks.ss_stack) {
        klog_error("[TSS] Failed to allocate IST stacks\n");
        /* Free any allocated stacks */
        if (s_ist_stacks.df_stack) kfree(s_ist_stacks.df_stack);
        if (s_ist_stacks.nmi_stack) kfree(s_ist_stacks.nmi_stack);
        if (s_ist_stacks.debug_stack) kfree(s_ist_stacks.debug_stack);
        if (s_ist_stacks.ss_stack) kfree(s_ist_stacks.ss_stack);
        return;
    }

    /* Set up IST stack pointers (stack grows down, so point to top) */
    s_tss.ist1 = (uint64_t)(s_ist_stacks.df_stack + IST_STACK_SIZE);
    s_tss.ist2 = (uint64_t)(s_ist_stacks.nmi_stack + IST_STACK_SIZE);
    s_tss.ist3 = (uint64_t)(s_ist_stacks.debug_stack + IST_STACK_SIZE);
    s_tss.ist4 = (uint64_t)(s_ist_stacks.ss_stack + IST_STACK_SIZE);

    /* Set kernel RSP0 for user->kernel transitions
     * Use the Double Fault stack as the default kernel stack
     */
    s_tss.rsp0 = s_tss.ist1;

    /* Set I/O bitmap base to indicate no bitmap (0xFFFF) */
    s_tss.iomap_base = 0xFFFF;

    klog_info("[TSS] IST1 (DF)  stack: 0x%016llX\n", s_tss.ist1);
    klog_info("[TSS] IST2 (NMI) stack: 0x%016llX\n", s_tss.ist2);
    klog_info("[TSS] IST3 (Debug) stack: 0x%016llX\n", s_tss.ist3);
    klog_info("[TSS] IST4 (SS)  stack: 0x%016llX\n", s_tss.ist4);
    klog_info("[TSS] RSP0 (kernel) : 0x%016llX\n", s_tss.rsp0);

    s_initialized = true;
}

tss_t* tss_get(void) {
    return &s_tss;
}

void tss_set_ist_stack(uint8_t ist_index, virtual_addr_t stack_top) {
    if (!s_initialized) {
        klog_warn("[TSS] Not initialized\n");
        return;
    }

    if (ist_index < 1 || ist_index > 7) {
        klog_error("[TSS] Invalid IST index: %u\n", ist_index);
        return;
    }

    /* Set the appropriate IST entry */
    switch (ist_index) {
        case 1: s_tss.ist1 = stack_top; break;
        case 2: s_tss.ist2 = stack_top; break;
        case 3: s_tss.ist3 = stack_top; break;
        case 4: s_tss.ist4 = stack_top; break;
        case 5: s_tss.ist5 = stack_top; break;
        case 6: s_tss.ist6 = stack_top; break;
        case 7: s_tss.ist7 = stack_top; break;
    }

    klog_debug("[TSS] IST%u stack set to 0x%016llX\n", ist_index, stack_top);
}

virtual_addr_t tss_get_ist_stack(uint8_t ist_index) {
    if (!s_initialized) {
        return 0;
    }

    if (ist_index < 1 || ist_index > 7) {
        return 0;
    }

    /* Get the appropriate IST entry */
    switch (ist_index) {
        case 1: return s_tss.ist1;
        case 2: return s_tss.ist2;
        case 3: return s_tss.ist3;
        case 4: return s_tss.ist4;
        case 5: return s_tss.ist5;
        case 6: return s_tss.ist6;
        case 7: return s_tss.ist7;
        default: return 0;
    }
}

void tss_dump(void) {
    if (!s_initialized) {
        klog_error("[TSS] Not initialized\n");
        return;
    }

    klog_info("[TSS] Task State Segment dump:\n");
    klog_info("[TSS]   RSP0:      0x%016llX\n", s_tss.rsp0);
    klog_info("[TSS]   IST1 (DF): 0x%016llX\n", s_tss.ist1);
    klog_info("[TSS]   IST2 (NMI):0x%016llX\n", s_tss.ist2);
    klog_info("[TSS]   IST3 (DBG):0x%016llX\n", s_tss.ist3);
    klog_info("[TSS]   IST4 (SS): 0x%016llX\n", s_tss.ist4);
    klog_info("[TSS]   IST5:      0x%016llX\n", s_tss.ist5);
    klog_info("[TSS]   IST6:      0x%016llX\n", s_tss.ist6);
    klog_info("[TSS]   IST7:      0x%016llX\n", s_tss.ist7);
    klog_info("[TSS]   IOMP base: 0x%04X\n", s_tss.iomap_base);
}
