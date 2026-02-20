/* ==============================================================================
 * CCOS - Thread Management Implementation
 * ==============================================================================
 * This module provides thread creation and management functionality.
 * Threads share the same address space as their parent process but have
 * their own kernel stack and CPU context.
 * ==============================================================================
 */

#include "base/memory.h"
#include "base/string.h"
#include "interrupt/tss.h"
#include "klogs/kprintf.h"
#include "klogs/ksnprintf.h"
#include "mm/heap/heap.h"
#include "mm/pframe/pframe.h"
#include "mm/vmm/page.h"
#include "mm/vmm/vmm.h"
#include "process/process.h"
#include "process/sched.h"
#include "sync/atomic.h"
#include "sync/spinlock.h"
#include "sync/waitqueue.h"

/* ==============================================================================
 * Thread Stack Allocator
 * ==============================================================================
 */

/**
 * @brief Next available thread stack address
 *
 * Thread stacks are allocated descending from THREAD_STACK_BASE.
 */
static virtual_addr_t s_next_thread_stack = THREAD_STACK_BASE;

/**
 * @brief Spinlock protecting thread stack allocation
 */
static spinlock_t s_thread_stack_lock = SPIN_LOCK_INIT;

/**
 * @brief Allocate a user stack for a new thread
 * @param parent Parent process (shared address space)
 * @param stack_size Desired stack size
 * @return Top of new stack, or 0 on failure
 */
static virtual_addr_t thread_alloc_user_stack(pcb_t* parent, size_t stack_size) {
    if (!parent || stack_size == 0) {
        return 0;
    }

    /* Align stack size to page boundary */
    stack_size = (stack_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Limit stack size */
    if (stack_size > THREAD_STACK_SIZE) {
        stack_size = THREAD_STACK_SIZE;
    }

    spinlock_flags_t flags;
    spin_lock_irqsave(&s_thread_stack_lock, &flags);

    /* Allocate stack region */
    virtual_addr_t stack_top = s_next_thread_stack;
    virtual_addr_t stack_base = stack_top - stack_size;

    /* Check if we have space */
    if (stack_base < THREAD_STACK_BASE - (64 * 1024 * 1024)) {
        spin_unlock_irqrestore(&s_thread_stack_lock, flags);
        klog_error("[THREAD] Thread stack region exhausted\n");
        return 0;
    }

    s_next_thread_stack = stack_base;

    spin_unlock_irqrestore(&s_thread_stack_lock, flags);

    /* Map pages into parent's address space */
    size_t num_pages = stack_size / PAGE_SIZE;
    for (size_t i = 0; i < num_pages; i++) {
        virtual_addr_t vaddr = stack_base + (i * PAGE_SIZE);
        physical_addr_t paddr;

        if (pframe_alloc(&paddr) != PFRAME_OK) {
            /* Roll back on failure */
            for (size_t j = 0; j < i; j++) {
                vaddr = stack_base + (j * PAGE_SIZE);
                physical_addr_t paddr_tmp;
                if (page_virt_to_phys(parent->mm.pml4_phys, vaddr, &paddr_tmp) == PAGE_OK) {
                    pframe_free(paddr_tmp);
                }
                page_unmap_page(parent->mm.pml4_phys, vaddr, false);
            }
            klog_error("[THREAD] Failed to allocate physical frame for stack\n");
            return 0;
        }

        vmm_result_t result = vmm_map_to_user(parent->mm.pml4_phys, vaddr, paddr, 1,
                                              VMAP_FLAG_WRITE | VMAP_FLAG_USER);

        if (result != VMM_OK) {
            pframe_free(paddr);
            /* Roll back */
            for (size_t j = 0; j < i; j++) {
                vaddr = stack_base + (j * PAGE_SIZE);
                physical_addr_t paddr_tmp;
                if (page_virt_to_phys(parent->mm.pml4_phys, vaddr, &paddr_tmp) == PAGE_OK) {
                    pframe_free(paddr_tmp);
                }
                page_unmap_page(parent->mm.pml4_phys, vaddr, false);
            }
            klog_error("[THREAD] Failed to map stack page\n");
            return 0;
        }
    }

    return stack_top;
}

/**
 * @brief Free a thread's user stack
 * @param thread Thread whose stack to free
 */
static void thread_free_user_stack(pcb_t* thread) {
    if (!thread || !thread->is_user_mode || thread->user_stack == 0) {
        return;
    }

    size_t stack_size = thread->user_stack_size;
    if (stack_size == 0) {
        stack_size = THREAD_STACK_SIZE;
    }

    /* Align to page boundary */
    stack_size = (stack_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    virtual_addr_t stack_base = thread->user_stack - stack_size;

    size_t num_pages = stack_size / PAGE_SIZE;
    for (size_t i = 0; i < num_pages; i++) {
        virtual_addr_t vaddr = stack_base + (i * PAGE_SIZE);
        physical_addr_t paddr;

        if (page_virt_to_phys(thread->mm.pml4_phys, vaddr, &paddr) == PAGE_OK) {
            pframe_free(paddr);
        }
        page_unmap_page(thread->mm.pml4_phys, vaddr, false);
    }
}

/* ==============================================================================
 * Thread Entry Point Wrapper
 * ==============================================================================
 */

/**
 * @brief Kernel-side wrapper for user thread entry
 *
 * This function is called when a new thread starts execution.
 * It sets up the user mode context and jumps to the thread's entry point.
 */
static void thread_entry_wrapper(void) {
    pcb_t* current = proc_current();

    if (!current) {
        klog_error("[THREAD] No current process in thread entry wrapper\n");
        while (1) {
            __asm__ volatile("hlt");
        }
    }

    klog_info("[THREAD] Thread %d starting, entry=%p, arg=%p\n", current->pid,
              (void*)current->thread_entry, (void*)current->thread_arg);

    /* Check if this is a user mode thread or kernel thread */
    if (current->is_user_mode) {
        /* === User thread: set up trap_frame and jump to user mode === */

        /* Initialize trap_frame for first entry to user mode */
        /* Call assembly function to restore and iretq to user mode */
        extern void restore_from_trap_frame_and_iret(void);
        restore_from_trap_frame_and_iret();

        /* Should never reach here */
        __builtin_unreachable();
    } else {
        /* === Kernel thread: just call the function directly === */
        thread_fn_t fn = (thread_fn_t)current->thread_entry;
        void* arg = (void*)current->thread_arg;

        if (fn) {
            fn(arg);
        }

        /* Thread function returned - exit */
        klog_info("[THREAD] Thread %d exited normally\n", current->pid);
        proc_thread_exit(NULL);
    }
}

/* ==============================================================================
 * Thread Find by TID
 * ==============================================================================
 */

/**
 * @brief Find a thread by TID
 * @param tid Thread ID to find
 * @return Pointer to PCB, or NULL if not found
 */
pcb_t* proc_find_by_tid(int32_t tid) {
    extern scheduler_t scheduler;

    /* Check current process */
    if (scheduler.current && scheduler.current->pid == tid) {
        return scheduler.current;
    }

    /* Search run queue */
    pcb_t* proc;
    list_for_each_entry(proc, &scheduler.run_queue, run_list) {
        if (proc->pid == tid) {
            return proc;
        }
    }

    /* Search thread lists */
    list_for_each_entry(proc, &scheduler.run_queue, run_list) {
        pcb_t* thread;
        list_for_each_entry(thread, &proc->thread_list, thread_group) {
            if (thread->pid == tid) {
                return thread;
            }
        }
    }

    return NULL;
}

/* ==============================================================================
 * Kernel Thread Creation
 * ==============================================================================
 */

/**
 * @brief Create a new kernel thread
 * @param fn Thread entry point function
 * @param arg Argument to pass to thread function
 * @param name Thread name for debugging
 * @return Pointer to new PCB, or NULL on failure
 */
pcb_t* proc_create_kernel_thread(thread_fn_t fn, void* arg, const char* name) {
    if (!fn) {
        klog_error("[THREAD] NULL thread function\n");
        return NULL;
    }

    /* Allocate PCB */
    pcb_t* thread = proc_alloc_pcb();
    if (!thread) {
        klog_error("[THREAD] Failed to allocate PCB for kernel thread\n");
        return NULL;
    }

    /* Allocate PID */
    thread->pid = pid_alloc();
    if (thread->pid < 0) {
        klog_error("[THREAD] Failed to allocate PID for kernel thread\n");
        proc_free_pcb(thread);
        return NULL;
    }

    /* Set up thread info */
    thread->ppid = 0; /* Kernel threads have no parent */
    thread->parent = NULL;
    thread->tgid = thread->pid; /* Kernel thread is its own thread group */
    thread->is_thread = true;
    thread->is_user_mode = false;
    thread->state = PROC_READY;
    thread->thread_entry = (virtual_addr_t)fn;
    thread->thread_arg = (virtual_addr_t)arg;

    /* Kernel threads share the kernel's address space
     * Get the PML4 from the current process (main thread) */
    pcb_t* current = proc_current();
    if (current) {
        klog_info("[THREAD] Current pml4_phys before read: 0x%llx\n", current->mm.pml4_phys);
        /* Use a local variable to avoid potential optimization issues */
        physical_addr_t current_pml4 = current->mm.pml4_phys;
        thread->mm.pml4_phys = current_pml4;
        klog_info("[THREAD] Setting pml4_phys from current: 0x%llx (copied to 0x%llx)\n",
                  current_pml4, thread->mm.pml4_phys);
    } else {
        /* Fallback: use kernel's PML4 directly */
        extern physical_addr_t vmm_get_kernel_pml4(void);
        thread->mm.pml4_phys = vmm_get_kernel_pml4();
        klog_info("[THREAD] Setting pml4_phys from vmm_get_kernel_pml4: 0x%llx\n",
                  thread->mm.pml4_phys);
    }
    klog_info("[THREAD] Final pml4_phys for thread: 0x%llx (offset in pcb: %lu)\n",
              thread->mm.pml4_phys, __builtin_offsetof(pcb_t, mm.pml4_phys));

    /* Set command name */
    if (name) {
        strncpy(thread->comm, name, sizeof(thread->comm) - 1);
        thread->comm[sizeof(thread->comm) - 1] = '\0';
    } else {
        strcpy(thread->comm, "kthread");
    }

    /* Allocate join waiters */
    wait_queue_head_t* wq = (wait_queue_head_t*)kmalloc(sizeof(wait_queue_head_t));
    if (!wq) {
        klog_error("[THREAD] Failed to allocate join waiters\n");
        pid_free(thread->pid);
        proc_free_pcb(thread);
        return NULL;
    }
    init_waitqueue_head(wq);
    thread->join_waiters = wq;

    /* Set up CPU context to start at thread_entry_wrapper
     * NOTE: Function pointers are already in the KERNEL_TEXT_BASE range due to
     * the bootloader's page table setup mapping KERNEL_TEXT_BASE to KERNEL_PHYS_BASE. */
    thread->cpu_ctx->rip = (virtual_addr_t)thread_entry_wrapper;
    /* Set RSP to top of kernel stack */
    thread->cpu_ctx->rsp = thread->kernel_stack;

    klog_info("[THREAD] Setup context: pcb=%p, rip=%p, rsp=%p, cpu_ctx=%p, pml4=0x%llx\n",
              (void*)thread, (void*)thread->cpu_ctx->rip, (void*)thread->cpu_ctx->rsp,
              (void*)thread->cpu_ctx, (unsigned long long)thread->mm.pml4_phys);

    /* Initialize scheduling entity */
    sched_set_policy(thread, SCHED_NORMAL, 0);

    /* Add to scheduler run queue */
    sched_enqueue_task(thread, false);

    klog_info("[THREAD] Created kernel thread: PID=%d, name=%s\n", thread->pid, thread->comm);

    return thread;
}

/* ==============================================================================
 * User Thread Creation
 * ==============================================================================
 */

/**
 * @brief Create a new user thread
 * @param parent Parent thread (current process)
 * @param entry User-space function pointer
 * @param arg User-space argument
 * @param stack_addr User stack address (0 to auto-allocate)
 * @param stack_size Stack size in bytes
 * @return TID on success, negative on error
 */
int32_t proc_create_user_thread(pcb_t* parent, virtual_addr_t entry, virtual_addr_t arg,
                                virtual_addr_t stack_addr, size_t stack_size) {
    if (!parent) {
        klog_error("[THREAD] NULL parent in proc_create_user_thread\n");
        return -1;
    }

    if (entry == 0) {
        klog_error("[THREAD] NULL thread entry point\n");
        return -1;
    }

    /* Allocate PCB */
    pcb_t* thread = proc_alloc_pcb();
    if (!thread) {
        klog_error("[THREAD] Failed to allocate PCB for user thread\n");
        return -1;
    }

    /* Allocate PID */
    thread->pid = pid_alloc();
    if (thread->pid < 0) {
        klog_error("[THREAD] Failed to allocate PID for user thread\n");
        proc_free_pcb(thread);
        return -1;
    }

    /* Set up thread info */
    thread->ppid = parent->pid;
    thread->parent = parent;

    /* Thread group ID - same as parent's tgid */
    if (parent->tgid != 0) {
        thread->tgid = parent->tgid;
    } else {
        thread->tgid = parent->pid;
    }

    thread->is_thread = true;
    thread->is_user_mode = true;
    thread->state = PROC_READY;
    thread->thread_entry = entry;
    thread->thread_arg = arg;

    /* Copy command name with suffix */
    ksnprintf(thread->comm, sizeof(thread->comm), "%s:t%d", parent->comm, thread->pid);

    /* Share address space with parent */
    thread->mm.pml4_phys = parent->mm.pml4_phys;
    thread->mm.brk = parent->mm.brk;
    thread->mm.stack_start = parent->mm.stack_start;

    /* Increment address space reference count */
    atomic_inc(&parent->mm_refcount);
    atomic_write(&thread->mm_refcount, atomic_read(&parent->mm_refcount));

    /* Allocate or use provided user stack */
    if (stack_addr == 0) {
        if (stack_size == 0) {
            stack_size = THREAD_STACK_SIZE;
        }
        thread->user_stack = thread_alloc_user_stack(parent, stack_size);
        if (thread->user_stack == 0) {
            klog_error("[THREAD] Failed to allocate user stack\n");
            pid_free(thread->pid);
            proc_free_pcb(thread);
            return -1;
        }
        thread->user_stack_size = stack_size;
    } else {
        thread->user_stack = stack_addr;
        thread->user_stack_size = stack_size;
    }

    /* Allocate join waiters */
    wait_queue_head_t* wq = (wait_queue_head_t*)kmalloc(sizeof(wait_queue_head_t));
    if (!wq) {
        klog_error("[THREAD] Failed to allocate join waiters\n");
        thread_free_user_stack(thread);
        pid_free(thread->pid);
        proc_free_pcb(thread);
        return -1;
    }
    init_waitqueue_head(wq);
    thread->join_waiters = wq;

    /* Set up CPU context to start at thread_entry_wrapper
     * NOTE: Function pointers are already in the KERNEL_TEXT_BASE range due to
     * the bootloader's page table setup mapping KERNEL_TEXT_BASE to KERNEL_PHYS_BASE. */
    thread->cpu_ctx->rip = (virtual_addr_t)thread_entry_wrapper;
    /* Set RSP to top of kernel stack */
    thread->cpu_ctx->rsp = thread->kernel_stack;

    klog_info("[THREAD] Setup context: pcb=%p, rip=%p, rsp=%p, cpu_ctx=%p, pml4=0x%llx\n",
              (void*)thread, (void*)thread->cpu_ctx->rip, (void*)thread->cpu_ctx->rsp,
              (void*)thread->cpu_ctx, (unsigned long long)thread->mm.pml4_phys);

    /* Add to thread group list */
    spinlock_flags_t flags;
    spin_lock_irqsave(&s_thread_stack_lock, &flags); /* Reuse stack lock for simplicity */
    list_add_tail(&thread->thread_group, &parent->thread_list);
    spin_unlock_irqrestore(&s_thread_stack_lock, flags);

    /* Initialize scheduling entity */
    sched_set_policy(thread, SCHED_NORMAL, 0);

    /* Add to scheduler run queue */
    sched_enqueue_task(thread, false);

    klog_info("[THREAD] Created user thread: TID=%d, TGID=%d, entry=%p\n", thread->pid,
              thread->tgid, entry);

    return thread->pid;
}

/* ==============================================================================
 * Thread Exit
 * ==============================================================================
 */

/**
 * @brief Exit current thread
 * @param retval Return value
 */
void proc_thread_exit(void* retval) {
    pcb_t* current = proc_current();
    if (!current) {
        klog_error("[THREAD] No current process to exit\n");
        while (1) {
            __asm__ volatile("hlt");
        }
    }

    klog_info("[THREAD] Thread %d exiting\n", current->pid);

    /* Store return value for join */
    current->return_value = retval;

    /* Remove from thread group list */
    if (current->parent) {
        list_del(&current->thread_group);
    }

    if (current->detached) {
        /* Detached thread - clean up immediately */
        current->state = PROC_ZOMBIE;
        thread_free_user_stack(current);
        proc_free_pcb(current);
    } else {
        /* Joinable thread - become zombie */
        current->state = PROC_ZOMBIE;

        /* Wake up any join waiters */
        if (current->join_waiters) {
            wake_up_all((wait_queue_head_t*)current->join_waiters);
        }
    }

    /* Schedule next process */
    schedule();

    /* Should never reach here */
    __builtin_unreachable();
}

/* ==============================================================================
 * Thread Join
 * ==============================================================================
 */

/**
 * @brief Wait for a thread to exit
 * @param tid Thread ID to wait for
 * @param retval Pointer to store return value
 * @return 0 on success, negative on error
 */
int32_t proc_thread_join(int32_t tid, void** retval) {
    pcb_t* current = proc_current();
    if (!current) {
        return -1;
    }

    /* Find target thread */
    pcb_t* target = proc_find_by_tid(tid);
    if (!target) {
        klog_error("[THREAD] Thread %d not found\n", tid);
        return -1;
    }

    /* Check if target is in same thread group */
    if (target->tgid != current->tgid) {
        klog_error("[THREAD] Cannot join thread from different group\n");
        return -1;
    }

    /* Cannot join detached thread */
    if (target->detached) {
        klog_error("[THREAD] Cannot join detached thread\n");
        return -1;
    }

    /* Check if already exited (zombie) */
    if (target->state == PROC_ZOMBIE) {
        /* Collect return value */
        if (retval) {
            *retval = target->return_value;
        }

        /* Free the thread's resources */
        thread_free_user_stack(target);
        proc_free_pcb(target);

        return 0;
    }

    /* Wait for thread to exit */
    if (target->join_waiters) {
        wait_event(*(wait_queue_head_t*)target->join_waiters, target->state == PROC_ZOMBIE);
    }

    /* Collect return value */
    if (retval) {
        *retval = target->return_value;
    }

    /* Free the thread's resources */
    thread_free_user_stack(target);
    proc_free_pcb(target);

    return 0;
}

/* ==============================================================================
 * Thread Detach
 * ==============================================================================
 */

/**
 * @brief Detach a thread (cannot be joined)
 * @param tid Thread ID to detach
 * @return 0 on success, negative on error
 */
int32_t proc_thread_detach(int32_t tid) {
    pcb_t* current = proc_current();
    if (!current) {
        return -1;
    }

    /* Find target thread */
    pcb_t* target = proc_find_by_tid(tid);
    if (!target) {
        klog_error("[THREAD] Thread %d not found\n", tid);
        return -1;
    }

    /* Check if target is in same thread group */
    if (target->tgid != current->tgid) {
        klog_error("[THREAD] Cannot detach thread from different group\n");
        return -1;
    }

    /* Mark as detached */
    target->detached = true;

    /* If already exited, free resources */
    if (target->state == PROC_ZOMBIE) {
        thread_free_user_stack(target);
        proc_free_pcb(target);
    }

    klog_info("[THREAD] Thread %d detached\n", tid);

    return 0;
}
