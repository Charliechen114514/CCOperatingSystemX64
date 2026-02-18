/* ==============================================================================
 * CCOS - Page Fault Handler Implementation
 * ==============================================================================
 */

#include "fault.h"
#include "mm/vmm/vmm.h"
#include "mm/vmm/page.h"
#include "mm/vmm/cow.h"
#include "process/process.h"
#include "process/process_defines.h"
#include "mm/pframe/pframe.h"
#include "klogs/kprintf.h"
#include "interrupt/idt.h"
#include "assert/assert.h"

/* ==============================================================================
 * Internal State
 * ============================================================================ */

static bool s_initialized = false;

/* Statistics */
static struct {
    uint64_t total_faults;
    uint64_t kernel_faults;
    uint64_t user_faults;
    uint64_t not_present_faults;
    uint64_t protection_faults;
    uint64_t write_faults;
    uint64_t handled_cow;
    uint64_t handled_demand;
} s_pf_stats = {0};

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void pf_init(void) {
    if (s_initialized) {
        klog_warn("[PF] Page fault handler already initialized\n");
        return;
    }

    /* Register the page fault handler with the IDT */
    idt_register_handler(IDT_PF, pf_handler);

    klog_info("[PF] Page fault handler registered (vector 14)\n");
    s_initialized = true;
}

void pf_parse_error_code(uint64_t error_code, page_fault_info_t* info) {
    info->error_code = error_code;
    info->present = (error_code & PF_ERR_PRESENT) != 0;
    info->write = (error_code & PF_ERR_WRITE) != 0;
    info->user = (error_code & PF_ERR_USER) != 0;
    info->reserved_bit = (error_code & PF_ERR_RESERVED) != 0;
    info->instruction_fetch = (error_code & PF_ERR_INSTR) != 0;
}

void pf_handler(interrupt_frame_t* frame, uint64_t error_code) {
    CCOS_ASSERT(s_initialized);

    /* Update statistics */
    s_pf_stats.total_faults++;

    /* Get fault address from CR2 */
    virtual_addr_t fault_addr = pf_get_cr2();

    /* Parse the error code */
    page_fault_info_t info;
    pf_parse_error_code(error_code, &info);
    info.fault_addr = fault_addr;

    /* Update statistics */
    if (info.user) {
        s_pf_stats.user_faults++;
    } else {
        s_pf_stats.kernel_faults++;
    }

    if (info.present) {
        s_pf_stats.protection_faults++;
    } else {
        s_pf_stats.not_present_faults++;
    }

    if (info.write) {
        s_pf_stats.write_faults++;
    }

    /* Log the page fault */
    klog_error("\n");
    klog_error("=== PAGE FAULT ===\n");
    klog_error("Fault address: 0x%016llX\n", fault_addr);
    klog_error("Instruction:  0x%016llX\n", frame->rip);
    klog_error("Error code:   0x%02llX\n", error_code);
    klog_error("  P=%d (page present)\n", info.present);
    klog_error("  W=%d (write access)\n", info.write);
    klog_error("  U=%d (user mode)\n", info.user);
    klog_error("  R=%d (reserved bit)\n", info.reserved_bit);
    klog_error("  I=%d (instruction fetch)\n", info.instruction_fetch);
    klog_error("Mode: %s\n", info.user ? "User" : "Supervisor");
    klog_error("==================\n");

    /* Check if address is canonical */
    if (!vmm_is_canonical(fault_addr)) {
        klog_error("[PF] Non-canonical address - system halted\n");
        goto kernel_panic;
    }

    /* Check for reserved bit violations (could be x86_64 specific issues) */
    if (info.reserved_bit) {
        klog_error("[PF] Reserved bit set in page table entry - possible corruption\n");
        goto kernel_panic;
    }

    /* Handle kernel vs user page faults differently */
    if (!info.user) {
        /* Kernel page fault - this is a kernel bug */
        klog_error("[PF] KERNEL PAGE FAULT - This is a kernel bug!\n");

        /* Print available register dump from interrupt frame */
        klog_error("[PF] Register dump:\n");
        klog_error("[PF]   RIP: 0x%016llX  RSP: 0x%016llX\n",
                   frame->rip, frame->rsp);
        klog_error("[PF]   RFLAGS: 0x%016llX\n", frame->rflags);
        klog_error("[PF]   CS:  0x%04llX  SS:  0x%04llX\n",
                   frame->cs, frame->ss);

        goto kernel_panic;
    } else {
        /* User page fault - try COW handling first */
        klog_debug("[PF] User page fault at 0x%016llX\n", fault_addr);

        /* Check for COW fault: present=1, write=1, user=1 */
        if (info.present && info.write && !info.reserved_bit) {
            physical_addr_t current_pml4 = vmm_get_current_pml4();
            pf_result_t cow_result = pf_handle_cow(current_pml4, fault_addr);

            if (cow_result == PF_SUCCESS) {
                klog_debug("[PF] COW fault handled successfully\n");
                return;  /* Resume execution */
            }
        }

        /* Try demand paging for non-present pages */
        if (!info.present && vmm_is_user_addr(fault_addr)) {
            pf_result_t demand_result = pf_handle_demand_page(fault_addr, true);
            if (demand_result == PF_SUCCESS) {
                klog_debug("[PF] Demand page handled successfully\n");
                return;  /* Resume execution */
            }
        }

        /* Unhandleable user fault - terminate user process */
        pcb_t* current = proc_current();
        if (current && current->is_user_mode) {
            klog_error("[PF] Terminating user process %d due to page fault at 0x%llX\n",
                      current->pid, fault_addr);
            proc_exit(SIGSEGV);
            __builtin_unreachable();  /* proc_exit never returns */
        }

        /* No current process or not a user process - halt */
        klog_error("[PF] User page fault with no valid process - system halted\n");
        klog_error("[PF] TODO: Implement process management and signal delivery\n");
        goto kernel_panic;
    }

kernel_panic:
    klog_error("\n[PF] System halted due to page fault\n");
    klog_error("[PF] Page fault statistics:\n");
    klog_error("[PF]   Total faults:     %llu\n", s_pf_stats.total_faults);
    klog_error("[PF]   Kernel faults:    %llu\n", s_pf_stats.kernel_faults);
    klog_error("[PF]   User faults:      %llu\n", s_pf_stats.user_faults);
    klog_error("[PF]   Not present:      %llu\n", s_pf_stats.not_present_faults);
    klog_error("[PF]   Protection:       %llu\n", s_pf_stats.protection_faults);
    klog_error("[PF]   Write faults:     %llu\n", s_pf_stats.write_faults);

    /* Disable interrupts and halt */
    interrupt_disable();
    while (1) {
        __asm__ volatile("hlt");
    }
}

/* ============================================================================
 * Copy-on-Write Implementation
 * ============================================================================ */

pf_result_t pf_handle_cow(physical_addr_t pml4, virtual_addr_t fault_addr) {
    /* Delegate to the COW module */
    cow_result_t result = cow_handle_fault(pml4, fault_addr);

    if (result == COW_OK) {
        s_pf_stats.handled_cow++;
        return PF_SUCCESS;
    }

    return PF_NOT_OUR_FAULT;
}

vmm_result_t pf_register_cow_region(physical_addr_t pml4, virtual_addr_t base, size_t size) {
    /* Delegate to the COW module */
    cow_result_t result = cow_register_region(pml4, base, size);
    return (result == COW_OK) ? VMM_OK : VMM_ERR_INVALID;
}

/* ============================================================================
 * Demand Paging Implementation
 * ============================================================================ */

pf_result_t pf_handle_demand_page(virtual_addr_t fault_addr, bool user) {
    /* TODO: Implement demand paging
     * 1. Check if fault_addr is in a valid region
     * 2. Allocate a new physical page
     * 3. Map it to the fault address
     * 4. Clear the new page (for security)
     */

    (void)fault_addr;
    (void)user;
    s_pf_stats.handled_demand++;
    return PF_NOT_OUR_FAULT;
}
