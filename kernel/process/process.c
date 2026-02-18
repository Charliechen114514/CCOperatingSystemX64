/* ==============================================================================
 * CCOS - Process Management Implementation
 * ==============================================================================
 */

#include "process/process.h"
#include "process/sched.h"
#include "process/sched_rr.h"
#include "process/sched_prio.h"
#include "mm/heap/heap.h"
#include "mm/vmm/vmm.h"
#include "mm/vmm/page.h"
#include "mm/pframe/pframe.h"
#include "mm/vmm/cow.h"
#include "interrupt/tss.h"
#include "klogs/kprintf.h"
#include "base/memory.h"
#include "assert/assert.h"
#include "bitmap/bitmap.h"

/* ==============================================================================
 * Global Scheduler State
 * ============================================================================== */

scheduler_t scheduler = {
    .current = NULL,
    .idle = NULL,
    .nr_running = 0,
    .need_resched = false,
};

/* ==============================================================================
 * PID Allocator
 * ============================================================================== */

#define PID_BITMAP_SIZE ((PID_MAX + 7) / 8)

static byte_t s_pid_bitmap_buffer[PID_BITMAP_SIZE];
static struct bitmap s_pid_bitmap;

/**
 * @brief Initialize PID allocator
 */
void pid_alloc_init(void) {
    bitmap_init(&s_pid_bitmap, s_pid_bitmap_buffer, PID_MAX);
    bitmap_set(&s_pid_bitmap, 0);  /* Reserve PID 0 */
}

/**
 * @brief Allocate a new PID
 * @return Allocated PID, or negative on error
 */
int32_t pid_alloc(void) {
    int pid = bitmap_find_first_zero(&s_pid_bitmap);
    if (pid < 0 || pid >= PID_MAX) {
        return -1;
    }
    bitmap_set(&s_pid_bitmap, pid);
    return pid;
}

/**
 * @brief Free a PID
 * @param pid PID to free
 */
void pid_free(int32_t pid) {
    if (pid > 0 && pid < PID_MAX) {
        bitmap_clear(&s_pid_bitmap, pid);
    }
}

/* ==============================================================================
 * PCB Management
 * ============================================================================ */

/**
 * @brief Allocate a new PCB
 * @return Pointer to new PCB, or NULL on failure
 */
static pcb_t* proc_alloc_pcb(void) {
    pcb_t* pcb = (pcb_t*)kmalloc(sizeof(pcb_t));
    if (!pcb) {
        return NULL;
    }

    /* Initialize to zero */
    memset(pcb, 0, sizeof(pcb_t));

    /* Allocate CPU context */
    pcb->cpu_ctx = (cpu_context_t*)kmalloc(sizeof(cpu_context_t));
    if (!pcb->cpu_ctx) {
        kfree(pcb);
        return NULL;
    }
    memset(pcb->cpu_ctx, 0, sizeof(cpu_context_t));

    /* Allocate kernel stack */
    pcb->kernel_stack_base = (virtual_addr_t)kmalloc(KERNEL_STACK_SIZE);
    if (!pcb->kernel_stack_base) {
        kfree(pcb->cpu_ctx);
        kfree(pcb);
        return NULL;
    }
    /* Stack grows down, so top is base + size */
    pcb->kernel_stack = pcb->kernel_stack_base + KERNEL_STACK_SIZE;

    /* Initialize lists */
    INIT_LIST_HEAD(&pcb->run_list);
    INIT_LIST_HEAD(&pcb->siblings);
    INIT_LIST_HEAD(&pcb->children);
    INIT_LIST_HEAD(&pcb->zombie_children);
    INIT_LIST_HEAD(&pcb->sched_entity.run_list);

    /* Initialize sched_entity with default values */
    pcb->sched_entity.policy = SCHED_NORMAL;
    pcb->sched_entity.sched_class = NULL;  /* Will be set during enqueue */
    pcb->sched_entity.time_slice = DEF_TIMESLICE_MS;
    pcb->sched_entity.time_slice_total = DEF_TIMESLICE_MS;
    pcb->sched_entity.priority = PRIO_DEFAULT;
    pcb->sched_entity.nice = 0;
    pcb->sched_entity.last_ran = 0;

    /* Initialize user mode fields */
    pcb->is_user_mode = false;
    pcb->user_stack = 0;
    pcb->user_stack_size = 0;

    return pcb;
}

/**
 * @brief Free a PCB
 * @param pcb PCB to free
 */
static void proc_free_pcb(pcb_t* pcb) {
    if (!pcb) {
        return;
    }

    /* Free CPU context */
    if (pcb->cpu_ctx) {
        kfree(pcb->cpu_ctx);
    }

    /* Free kernel stack */
    if (pcb->kernel_stack_base) {
        kfree((void*)pcb->kernel_stack_base);
    }

    /* Free trap frame if exists */
    if (pcb->trap_frame) {
        kfree(pcb->trap_frame);
    }

    /* Free user stack if this is a user process */
    if (pcb->is_user_mode && pcb->user_stack != 0 && pcb->user_stack_size > 0) {
        /* Unmap and free user stack pages */
        size_t stack_pages = pcb->user_stack_size / PAGE_SIZE;
        for (size_t i = 0; i < stack_pages; i++) {
            virtual_addr_t vaddr = pcb->user_stack + (i * PAGE_SIZE);
            physical_addr_t paddr;
            if (page_virt_to_phys(pcb->mm.pml4_phys, vaddr, &paddr) == PAGE_OK) {
                pframe_free(paddr);
            }
            page_unmap_page(pcb->mm.pml4_phys, vaddr, false);
        }
    }

    /* Destroy address space if it's a user process */
    if (pcb->mm.pml4_phys != 0) {
        vmm_destroy_user_space(pcb->mm.pml4_phys);
    }

    kfree(pcb);
}

/* ==============================================================================
 * Process Find
 * ============================================================================ */

/**
 * @brief Find process by PID
 * @param pid Process ID to find
 * @return Pointer to PCB, or NULL if not found
 */
pcb_t* proc_find(int32_t pid) {
    /* For now, we only search the run queue */
    /* TODO: Implement a process table for O(1) lookup */

    if (scheduler.current && scheduler.current->pid == pid) {
        return scheduler.current;
    }

    pcb_t* proc;
    list_for_each_entry(proc, &scheduler.run_queue, run_list) {
        if (proc->pid == pid) {
            return proc;
        }
    }

    return NULL;
}

/* ==============================================================================
 * Context Switch
 * ============================================================================ */

/**
 * @brief Switch to a new process
 * @param prev Pointer to store current process
 * @param next Process to switch to
 */
extern void switch_context(pcb_t** prev, pcb_t* next);

/**
 * @brief Switch from kernel to first process
 * @param first First process to run
 */
extern void switch_to_first(pcb_t* first);

/* ==============================================================================
 * Scheduler
 * ============================================================================ */

/**
 * @brief Schedule next process
 */
void schedule(void) {
    pcb_t* prev = scheduler.current;
    pcb_t* next = NULL;

    /* Clear reschedule flag */
    sched_clear_resched();

    /* Pick next task using scheduler classes */
    next = sched_pick_next_task();

    /* If no task, run idle */
    if (!next) {
        if (scheduler.idle) {
            next = scheduler.idle;
        } else {
            /* No idle process, can't schedule */
            klog_warn("[PROC] No process to run!\n");
            return;
        }
    }

    if (next == prev) {
        /* Same process, no need to switch */
        return;
    }

    /* Dequeue previous task if needed */
    if (prev && prev != scheduler.idle) {
        if (prev->state == PROC_RUNNING) {
            prev->state = PROC_READY;
        }
        /* Re-enqueue if ready */
        if (prev->state == PROC_READY) {
            sched_enqueue_task(prev, false);
        }
    }

    /* Dequeue next from its run queue */
    if (next != scheduler.idle) {
        sched_dequeue_task(next);
    }

    /* Update state */
    next->state = PROC_RUNNING;
    scheduler.current = next;

    /* Reset time slice for new task */
    sched_reset_time_slice(next);

    /* Perform context switch */
    if (prev == NULL) {
        /* First time switching from kernel */
        switch_to_first(next);
    } else {
        switch_context(&prev, next);
    }
}

/* ==============================================================================
 * Process Creation (Fork)
 * ============================================================================ */

/**
 * @brief Internal function to copy parent's address space to child
 * @param parent Parent PCB
 * @param child Child PCB
 * @return 0 on success, negative on error
 */
static int proc_copy_address_space(pcb_t* parent, pcb_t* child) {
    /* Create new user address space */
    vmm_result_t result = vmm_create_user_space(&child->mm.pml4_phys);
    if (result != VMM_OK) {
        klog_error("[PROC] Failed to create user address space for child\n");
        return -1;
    }

    /* For now, we just create an empty address space */
    /* TODO: Implement proper COW fork by:
     * 1. Copying parent's page table entries to child
     * 2. Registering pages with COW subsystem
     * 3. Marking pages as read-only with COW flag
     */

    /* Copy memory context settings */
    child->mm.brk = parent->mm.brk;
    child->mm.stack_start = parent->mm.stack_start;

    return 0;
}

/**
 * @brief Fork - create a new process
 * @return Child PID to parent, 0 to child, negative on error
 */
int32_t proc_fork(void) {
    pcb_t* parent = proc_current();
    if (!parent) {
        klog_error("[PROC] No current process to fork from\n");
        return -1;
    }

    /* Allocate child PCB */
    pcb_t* child = proc_alloc_pcb();
    if (!child) {
        klog_error("[PROC] Failed to allocate child PCB\n");
        return -1;
    }

    /* Allocate PID */
    child->pid = pid_alloc();
    if (child->pid < 0) {
        klog_error("[PROC] Failed to allocate PID\n");
        proc_free_pcb(child);
        return -1;
    }

    /* Set up child's basic info */
    child->ppid = parent->pid;
    child->parent = parent;
    child->state = PROC_READY;
    child->start_time = 0;  /* TODO: Get actual time */

    /* Copy command name */
    for (int i = 0; i < 16; i++) {
        child->comm[i] = parent->comm[i];
    }

    /* Copy address space */
    if (proc_copy_address_space(parent, child) != 0) {
        pid_free(child->pid);
        proc_free_pcb(child);
        return -1;
    }

    /* Add to parent's children list */
    list_add_tail(&child->siblings, &parent->children);

    /* Initialize scheduling entity with RR class */
    sched_set_policy(child, SCHED_NORMAL, 0);

    /* Add to scheduler run queue */
    sched_enqueue_task(child, false);  /* false = add to tail */

    klog_info("[PROC] Forked: parent PID=%d, child PID=%d\n",
              parent->pid, child->pid);

    /* If this is the child process, return 0 */
    if (proc_current() == child) {
        return 0;
    }

    /* Parent returns child's PID */
    return child->pid;
}

/* ==============================================================================
 * Process Exit
 * ============================================================================ */

/**
 * @brief Exit current process
 * @param exit_code Exit code
 */
void proc_exit(int exit_code) {
    pcb_t* current = proc_current();
    if (!current) {
        klog_error("[PROC] No current process to exit\n");
        /* Halt if no current process */
        while (1) {
            __asm__ volatile("hlt");
        }
    }

    klog_info("[PROC] Process %d exiting with code %d\n", current->pid, exit_code);

    /* Set zombie state */
    current->state = PROC_ZOMBIE;
    current->exit_code = exit_code;

    /* Move to parent's zombie list */
    list_del(&current->run_list);
    if (current->parent) {
        list_add_tail(&current->zombie_children, &current->parent->zombie_children);
    }

    scheduler.nr_running--;

    /* Schedule next process */
    schedule();

    /* Should never reach here */
    __builtin_unreachable();
}

/* ==============================================================================
 * Process Wait
 * ============================================================================ */

/**
 * @brief Wait for a child process to exit
 * @param pid PID to wait for (-1 for any)
 * @param wstatus Pointer to store exit status
 * @param options Wait options (WNOHANG, etc.)
 * @return PID of exited child, or negative on error
 */
int32_t proc_wait4(int32_t pid, int* wstatus, int options) {
    (void)options;  /* TODO: Implement options */

    pcb_t* current = proc_current();
    if (!current) {
        return -1;
    }

    /* Check for zombie children */
    pcb_t* child;
    pcb_t* target = NULL;
    list_for_each_entry(child, &current->zombie_children, zombie_children) {
        if (pid == -1 || child->pid == pid) {
            target = child;
            break;
        }
    }

    if (target) {
        /* Found a zombie child */
        if (wstatus) {
            *wstatus = target->exit_code;
        }
        int32_t result = target->pid;

        /* Remove from zombie list and free */
        list_del(&target->zombie_children);
        proc_free_pcb(target);

        return result;
    }

    /* No zombie children - block current */
    /* TODO: Implement proper blocking with wakeup */
    current->state = PROC_BLOCKED;
    schedule();

    return -1;  /* Will be resumed when child exits */
}

/* ==============================================================================
 * Process Initialization
 * ============================================================================ */

/**
 * @brief Initialize process subsystem
 * @return 0 on success, negative on error
 */
int proc_init(void) {
    klog_info("[PROC] Initializing process subsystem...\n");

    /* Initialize scheduler class framework first */
    int ret = sched_class_init();
    if (ret != 0) {
        klog_error("[PROC] Failed to initialize scheduler classes\n");
        return ret;
    }

    /* Initialize Round-Robin scheduling class */
    ret = sched_rr_init();
    if (ret != 0) {
        klog_error("[PROC] Failed to initialize RR scheduler class\n");
        return ret;
    }

    /* Initialize Priority scheduling class */
    ret = sched_prio_init();
    if (ret != 0) {
        klog_warn("[PROC] Failed to initialize Priority scheduler class (continuing without it)\n");
        /* Continue without priority class */
    }

    /* Initialize scheduler */
    INIT_LIST_HEAD(&scheduler.run_queue);
    scheduler.current = NULL;
    scheduler.idle = NULL;
    scheduler.nr_running = 0;
    scheduler.need_resched = false;

    /* Initialize PID allocator */
    pid_alloc_init();

    klog_info("[PROC] Process subsystem initialized\n");
    return 0;
}
