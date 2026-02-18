/**
 * @file user_demo.c
 * @brief User Mode Demo - uname syscall test
 *
 * This demo demonstrates:
 * 1. Creating a user mode process
 * 2. Loading a simple user program that calls uname
 * 3. Printing the uname structure from user mode
 * 4. Returning to kernel mode
 */

#include "user_demo.h"
#include "user/user.h"
#include "process/process.h"
#include "mm/vmm/vmm.h"
#include "mm/vmm/page.h"
#include "mm/heap/heap.h"
#include "mm/page_config.h"
#include "klogs/kprintf.h"
#include "base/string.h"
#include "base/memory.h"
#include "ccos_config.h"

/* ============================================================================
 * External user program (compiled separately)
 * ============================================================================ */

/* Reference the compiled user program */
extern const uint8_t _binary_user_programs_demo_uname_test_start[];
extern const uint8_t _binary_user_programs_demo_uname_test_size[];

/* ============================================================================
 * User Mode Process Creation and Execution
 * ============================================================================ */

/**
 * @brief Load a user program into memory and execute it
 */
static int load_and_execute_user_program(const uint8_t* program, size_t program_size) {
    virtual_addr_t user_code_base = 0x400000;  /* Typical user code start */

    klog_trace("[USER_DEMO] load_and_execute_user_program: entry\n");
    klog_info("[USER_DEMO] Creating user mode process...\n");
    klog_info("[USER_DEMO] Program size: %zu bytes\n", program_size);

    klog_trace("[USER_DEMO] About to allocate PCB...\n");
    /* Create a new PCB for the user process */
    pcb_t* user_pcb = (pcb_t*)kmalloc(sizeof(pcb_t));
    if (!user_pcb) {
        klog_error("[USER_DEMO] Failed to allocate PCB\n");
        return -1;
    }
    memset(user_pcb, 0, sizeof(pcb_t));

    /* Initialize basic PCB fields */
    user_pcb->pid = pid_alloc();  /* Use the correct function */
    user_pcb->ppid = proc_current() ? proc_current()->pid : 0;
    user_pcb->state = PROC_READY;
    user_pcb->is_user_mode = true;
    INIT_LIST_HEAD(&user_pcb->run_list);
    INIT_LIST_HEAD(&user_pcb->siblings);
    INIT_LIST_HEAD(&user_pcb->children);
    INIT_LIST_HEAD(&user_pcb->zombie_children);
    user_pcb->sched_entity.policy = SCHED_NORMAL;
    user_pcb->sched_entity.sched_class = NULL;

    /* Allocate kernel stack for the process */
    user_pcb->kernel_stack_base = (virtual_addr_t)kmalloc(KERNEL_STACK_SIZE);
    if (!user_pcb->kernel_stack_base) {
        klog_error("[USER_DEMO] Failed to allocate kernel stack\n");
        kfree(user_pcb);
        return -1;
    }
    user_pcb->kernel_stack = user_pcb->kernel_stack_base + KERNEL_STACK_SIZE;

    /* Allocate CPU context */
    user_pcb->cpu_ctx = (cpu_context_t*)kmalloc(sizeof(cpu_context_t));
    if (!user_pcb->cpu_ctx) {
        klog_error("[USER_DEMO] Failed to allocate CPU context\n");
        kfree((void*)user_pcb->kernel_stack_base);
        kfree(user_pcb);
        return -1;
    }
    memset(user_pcb->cpu_ctx, 0, sizeof(cpu_context_t));

    /* Create user address space */
    vmm_result_t result = vmm_create_user_space(&user_pcb->mm.pml4_phys);
    if (result != VMM_OK) {
        klog_error("[USER_DEMO] Failed to create user address space\n");
        kfree(user_pcb->cpu_ctx);
        kfree((void*)user_pcb->kernel_stack_base);
        kfree(user_pcb);
        return -1;
    }

    /* Map the user program code into user space */
    size_t code_pages = (program_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < code_pages; i++) {
        virtual_addr_t vaddr = user_code_base + (i * PAGE_SIZE);
        virtual_addr_t program_vaddr = (virtual_addr_t)program + (i * PAGE_SIZE);

        /* Get physical address of the program code (it's in kernel virtual space) */
        physical_addr_t paddr;
        page_result_t lookup_result = page_virt_to_phys(
            page_get_pml4(),  /* Use current PML4 since program is in kernel space */
            program_vaddr,
            &paddr
        );
        if (lookup_result != PAGE_OK) {
            klog_error("[USER_DEMO] Failed to get physical address for program at 0x%llX\n",
                      program_vaddr);
            vmm_destroy_user_space(user_pcb->mm.pml4_phys);
            kfree(user_pcb->cpu_ctx);
            kfree((void*)user_pcb->kernel_stack_base);
            kfree(user_pcb);
            return -1;
        }

        /* Map the code page as user-readable and executable */
        /* VMAP_FLAG_USER implies readable by default */
        page_result_t page_result = page_map_page(user_pcb->mm.pml4_phys, vaddr, paddr,
                                                     VMAP_FLAG_USER,
                                                     true);
        if (page_result != PAGE_OK) {
            klog_error("[USER_DEMO] Failed to map code page at 0x%llX\n", vaddr);
            vmm_destroy_user_space(user_pcb->mm.pml4_phys);
            kfree(user_pcb->cpu_ctx);
            kfree((void*)user_pcb->kernel_stack_base);
            kfree(user_pcb);
            return -1;
        }
    }

    /* Create user mode process structure */
    int create_result = user_create_process(user_code_base, user_pcb);
    if (create_result != 0) {
        klog_error("[USER_DEMO] Failed to create user process\n");
        vmm_destroy_user_space(user_pcb->mm.pml4_phys);
        kfree(user_pcb->cpu_ctx);
        kfree((void*)user_pcb->kernel_stack_base);
        kfree(user_pcb);
        return -1;
    }

    klog_info("[USER_DEMO] User process created:\n");
    klog_info("[USER_DEMO]   PID: %d\n", user_pcb->pid);
    klog_info("[USER_DEMO]   Entry: 0x%llX\n", user_code_base);
    klog_info("[USER_DEMO]   User Stack: 0x%llX - 0x%llX\n",
              user_pcb->user_stack, user_pcb->user_stack + user_pcb->user_stack_size);
    klog_info("[USER_DEMO]   PML4: 0x%X\n\n", user_pcb->mm.pml4_phys);

    klog_info("[USER_DEMO] ========================================\n");
    klog_info("[USER_DEMO] Executing user mode program...\n");
    klog_info("[USER_DEMO] ========================================\n\n");

    /* TODO: In a real system, we would:
     * 1. Add the process to the scheduler
     * 2. Return to the scheduler
     * 3. The scheduler would switch to the user process
     *
     * For this demo, we're just demonstrating that we CAN create
     * a user mode process. The actual execution would require
     * proper context switching support.
     */

    /* Clean up for now (since we can't actually execute in Ring 3 yet) */
    klog_trace("[USER_DEMO] About to call user_destroy_process...\n");
    user_destroy_process(user_pcb);
    klog_trace("[USER_DEMO] user_destroy_process returned\n");
    kfree(user_pcb->cpu_ctx);
    kfree((void*)user_pcb->kernel_stack_base);
    kfree(user_pcb);

    klog_info("\n[USER_DEMO] ========================================\n");
    klog_info("[USER_DEMO] User process creation demo completed!\n");
    klog_info("[USER_DEMO] ========================================\n");

    klog_trace("[USER_DEMO] load_and_execute_user_program: returning 0\n");
    return 0;
}

/* ============================================================================
 * Demo: Kernel-mode uname test (for comparison)
 * ============================================================================ */

/**
 * @brief Test uname from kernel mode (for comparison)
 */
static void demo_kernel_uname(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Kernel-mode uname (for reference)    ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    /* These are the values that uname syscall returns */
    klog_info("[KERNEL] sysname:    %s\n", "CCOS");
    klog_info("[KERNEL] nodename:   %s\n", "localhost");
    klog_info("[KERNEL] release:    %s\n", CCOS_VERSION);
    klog_info("[KERNEL] version:    %s\n", "CCOS x86_64 v" CCOS_VERSION);
    klog_info("[KERNEL] machine:    %s\n", "x86_64");
    klog_info("[KERNEL] domainname: %s\n", "(none current)");
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int user_run_demo(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   User Mode uname Demo                  ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    klog_trace("[USER_DEMO] About to call user_init...\n");
    /* Initialize user mode subsystem */
    int init_result = user_init();
    klog_trace("[USER_DEMO] user_init returned: %d\n", init_result);
    if (init_result != 0) {
        klog_error("[USER_DEMO] Failed to initialize user mode subsystem\n");
        return -1;
    }

    /* Show kernel-mode uname for reference */
    klog_trace("[USER_DEMO] About to call demo_kernel_uname...\n");
    demo_kernel_uname();

    /* Create and execute user mode program */
    klog_trace("[USER_DEMO] About to load user program...\n");
    size_t program_size = (size_t)_binary_user_programs_demo_uname_test_size;
    klog_trace("[USER_DEMO] Program size: %zu bytes, start addr: %p\n", program_size, _binary_user_programs_demo_uname_test_start);
    int exec_result = load_and_execute_user_program(
        _binary_user_programs_demo_uname_test_start,
        program_size
    );

    if (exec_result != 0) {
        klog_error("[USER_DEMO] Failed to execute user program\n");
        return -1;
    }

    klog_info("\n[USER_DEMO] Demo completed successfully!\n");
    klog_info("[USER_DEMO] Note: Full Ring 3 execution requires scheduler support.\n");

    return 0;
}

void user_stop_demo(void) {
    klog_info("[USER_DEMO] Stopping user mode demo...\n");
}