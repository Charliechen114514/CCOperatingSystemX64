/* ==============================================================================
 * CCOS - Exception Handlers Implementation
 * ==============================================================================
 */

#include "interrupt/exception.h"
#include "interrupt/idt.h"
#include "interrupt/idt_constants.h"
#include "process/process.h"
#include "process/process_defines.h"
#include "klogs/kprintf.h"
#include "klogs/ksnprintf.h"

/* ============================================================================
 * Internal State
 * ============================================================================ */

static exception_stats_t s_stats = {0};

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void exception_init(void) {
    klog_info("[EXC] Registering exception handlers...\n");

    /* Register critical exception handlers */
    idt_register_handler(IDT_DF, double_fault_handler);
    idt_register_handler(IDT_SS, stack_fault_handler);
    idt_register_handler(IDT_GP, gp_fault_handler);

    klog_info("[EXC] Exception handlers registered\n");
}

void exception_get_stats(exception_stats_t* stats) {
    if (stats) {
        *stats = s_stats;
    }
}

/* ============================================================================
 * Double Fault Handler (Vector 8)
 * ============================================================================ */

void double_fault_handler(interrupt_frame_t* frame, uint64_t error_code) {
    s_stats.df_count++;

    klog_error("\n");
    klog_error("=================================================================\n");
    klog_error("=== DOUBLE FAULT (#DF) - CRITICAL ERROR =========================\n");
    klog_error("=================================================================\n");
    klog_error("System in critical state - nested exception detected!\n");
    klog_error("\n");
    klog_error("Error Code:     0x%016llX\n", error_code);
    klog_error("Instruction:    0x%016llX\n", frame->rip);
    klog_error("Stack Pointer:  0x%016llX\n", frame->rsp);
    klog_error("Code Segment:   0x%04llX\n", frame->cs);
    klog_error("RFLAGS:         0x%016llX\n", frame->rflags);

    /* Check error code meaning */
    if (error_code != 0) {
        klog_error("\n[DF] Non-zero error code indicates:\n");
        if (error_code & 0x01) {
            klog_error("[DF]   - Page fault while delivering double fault\n");
        }
        klog_error("[DF] This is a very serious error - possible stack corruption\n");
    } else {
        klog_error("\n[DF] Double fault with no error code\n");
        klog_error("[DF] This indicates an exception occurred during exception handling\n");
    }

    /* Try to determine if we're in user or kernel mode */
    bool user_mode = (frame->cs & 0x03) == 3;
    klog_error("[DF] Mode: %s\n", user_mode ? "User" : "Supervisor");

    klog_error("\n[DF] System halted - cannot recover from double fault\n");
    klog_error("[DF] This is a safety measure to prevent triple fault (system reset)\n");
    klog_error("=================================================================\n");

    /* Disable interrupts and halt */
    interrupt_disable();
    while (1) {
        __asm__ volatile("hlt");
    }
}

/* ============================================================================
 * Stack Fault Handler (Vector 12)
 * ============================================================================ */

const char* ss_parse_error_code(uint64_t error_code) {
    static char buffer[128] = {0};

    bool external = (error_code & 0x01) != 0;
    bool not_present = (error_code & 0x02) != 0;
    uint16_t selector = (error_code >> 3) & 0x1FFF;

    int i = 0;
    i += ksnprintf(buffer + i, sizeof(buffer) - i, "SS fault: ");
    if (external)
        i += ksnprintf(buffer + i, sizeof(buffer) - i, "[External] ");
    if (not_present)
        i += ksnprintf(buffer + i, sizeof(buffer) - i, "[Not-Present] ");
    i += ksnprintf(buffer + i, sizeof(buffer) - i, "Selector=0x%04X", selector);

    return buffer;
}

void stack_fault_handler(interrupt_frame_t* frame, uint64_t error_code) {
    s_stats.ss_count++;

    klog_error("\n");
    klog_error("=================================================================\n");
    klog_error("=== STACK FAULT (#SS) ==========================================\n");
    klog_error("=================================================================\n");
    klog_error("Error Code:     0x%02llX  %s\n", error_code, ss_parse_error_code(error_code));
    klog_error("Instruction:    0x%016llX\n", frame->rip);
    klog_error("Stack Pointer:  0x%016llX\n", frame->rsp);
    klog_error("Code Segment:   0x%04llX\n", frame->cs);
    klog_error("RFLAGS:         0x%016llX\n", frame->rflags);

    /* Check if user mode or kernel mode */
    bool user_mode = (frame->cs & 0x03) == 3;

    if (user_mode) {
        klog_error("\n[SS] User mode stack fault\n");
        klog_error("[SS] Possible causes:\n");
        klog_error("[SS]   - User stack overflow\n");
        klog_error("[SS]   - Invalid stack segment reference\n");

        /* Terminate user process */
        pcb_t* current = proc_current();
        if (current && current->is_user_mode) {
            klog_error("[SS] Terminating user process %d\n", current->pid);
            proc_exit(SIGSEGV);
            __builtin_unreachable();  /* proc_exit never returns */
        }

        klog_error("[SS] User stack fault with no valid process - halting\n");
    } else {
        klog_error("\n[SS] KERNEL stack fault - CRITICAL ERROR\n");
        klog_error("[SS] Possible causes:\n");
        klog_error("[SS]   - Kernel stack overflow\n");
        klog_error("[SS]   - Corrupted stack pointer\n");
        klog_error("[SS]   - Invalid TSS stack setup\n");
        klog_error("[SS] This is unrecoverable\n");
    }

    klog_error("=================================================================\n");

    /* For now, halt on stack fault */
    klog_error("[SS] System halted\n");
    interrupt_disable();
    while (1) {
        __asm__ volatile("hlt");
    }
}

/* ============================================================================
 * General Protection Fault Handler (Vector 13)
 * ============================================================================ */

void gp_parse_error_code(uint64_t error_code, gpf_error_info_t* info) {
    info->external = (error_code & 0x01) != 0;
    info->idt_descriptor = (error_code & 0x02) != 0;
    info->gdt_table = (error_code & 0x04) != 0;
    info->ti = (error_code & 0x08) != 0;
    info->selector_index = (error_code >> 3) & 0x1FFF;
}

void gp_fault_handler(interrupt_frame_t* frame, uint64_t error_code) {
    s_stats.gp_count++;

    /* Parse error code */
    gpf_error_info_t info;
    gp_parse_error_code(error_code, &info);

    /* Check if user mode or kernel mode */
    bool user_mode = (frame->cs & 0x03) == 3;

    if (user_mode) {
        s_stats.gp_user_count++;
    } else {
        s_stats.gp_kernel_count++;
    }

    klog_error("\n");
    klog_error("=================================================================\n");
    klog_error("=== GENERAL PROTECTION FAULT (#GP) =============================\n");
    klog_error("=================================================================\n");
    klog_error("Error Code:     0x%016llX\n", error_code);
    klog_error("  External:     %s\n", info.external ? "yes" : "no");
    klog_error("  IDT desc:     %s\n", info.idt_descriptor ? "yes" : "no");
    klog_error("  Table:        %s\n", info.gdt_table ? (info.ti ? "LDT" : "GDT") : "N/A");
    if (info.gdt_table) {
        klog_error("  Selector:     0x%04X\n", info.selector_index);
    }
    klog_error("Instruction:    0x%016llX\n", frame->rip);
    klog_error("Stack Pointer:  0x%016llX\n", frame->rsp);
    klog_error("Code Segment:   0x%04llX\n", frame->cs);
    klog_error("Stack Segment:  0x%04llX\n", frame->ss);
    klog_error("RFLAGS:         0x%016llX\n", frame->rflags);

    /* Decode instruction at RIP for debugging */
    klog_error("\n[GP] Instruction bytes at RIP:\n");
    uint8_t* ip = (uint8_t*)frame->rip;
    klog_error("[GP]   ");
    for (int i = 0; i < 16; i++) {
        klog_error("%02X ", ip[i]);
    }
    klog_error("\n");

    if (user_mode) {
        klog_error("\n[GP] User mode GPF\n");
        klog_error("[GP] Possible causes:\n");
        klog_error("[GP]   - I/O port access without permission\n");
        klog_error("[GP]   - Privileged instruction in user mode\n");
        klog_error("[GP]   - Writing to read-only memory\n");
        klog_error("[GP]   - Invalid segment reference\n");

        /* Terminate user process */
        pcb_t* current = proc_current();
        if (current && current->is_user_mode) {
            klog_error("[GP] Terminating user process %d\n", current->pid);
            proc_exit(SIGSEGV);
            __builtin_unreachable();  /* proc_exit never returns */
        }

        klog_error("[GP] User GPF with no valid process - halting\n");
    } else {
        klog_error("\n[GP] KERNEL GPF - KERNEL BUG DETECTED\n");
        klog_error("[GP] This indicates a serious kernel error\n");
        klog_error("[GP] Possible causes:\n");
        klog_error("[GP]   - Null pointer dereference\n");
        klog_error("[GP]   - Invalid memory access\n");
        klog_error("[GP]   - Segment register corruption\n");
        klog_error("[GP]   - Privilege violation\n");
    }

    klog_error("=================================================================\n");

    /* For now, halt on GPF */
    klog_error("[GP] System halted\n");
    interrupt_disable();
    while (1) {
        __asm__ volatile("hlt");
    }
}
