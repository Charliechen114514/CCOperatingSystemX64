/* ==============================================================================
 * CCOS - Process Management
 * ==============================================================================
 * This module provides process management including PCB structure,
 * process states, and process creation/destruction APIs.
 * ==============================================================================
 */

#pragma once

#include "process_defines.h"
#include "defines/types.h"
#include "list/list.h"
#include "mm/vmm/vmm_config.h"
#include "mm/page_config.h"  /* For PAGE_SIZE */
#include "process/sched.h"   /* Scheduling class framework */
#include "sync/atomic.h"     /* For atomic_t (mm_refcount) */

/* ==============================================================================
 * Process Constants
 * ============================================================================== */

#define PID_MAX           32768
#define KERNEL_STACK_SIZE (16 * 1024)  /* 16KB */
#define USER_STACK        (USER_END - PAGE_SIZE)

/* ==============================================================================
 * Thread Constants
 * ============================================================================== */

#define THREAD_STACK_BASE    (USER_END - 64 * 1024 * 1024)  /* 64MB region for thread stacks */
#define THREAD_STACK_SIZE    (256 * 1024)                    /* 256KB per thread */

/* ==============================================================================
 * Process State Enumeration
 * ============================================================================== */

/**
 * @brief Process states
 */
typedef enum process_state {
    PROC_READY    = 0,    /* Ready to run (on run queue) */
    PROC_RUNNING  = 1,    /* Currently executing */
    PROC_BLOCKED  = 2,    /* Waiting for I/O or event */
    PROC_ZOMBIE   = 3,    /* Terminated, waiting to be reaped */
} process_state_t;

/* ==============================================================================
 * CPU Context Structure
 * ============================================================================== */

/**
 * @brief CPU context saved during context switch
 *
 * This matches the layout expected by switch_context in switch.s.
 * Only callee-saved registers need to be saved (System V AMD64 ABI).
 */
typedef struct PACKED cpu_context {
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;        /* Stack pointer */
    uint64_t rip;        /* Return address */
} cpu_context_t;

/* ==============================================================================
 * Trap Frame Structure
 * ============================================================================== */

/**
 * @brief Trap frame saved when entering kernel from user mode
 *
 * This captures the full user state for return via iretq/sysret.
 */
typedef struct PACKED trap_frame {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t vector;      /* Interrupt vector */
    uint64_t error_code;  /* Error code */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} trap_frame_t;

/* ==============================================================================
 * Memory Context Structure
 * ============================================================================== */

/**
 * @brief Memory context for a process
 */
typedef struct memory_context {
    physical_addr_t pml4_phys;    /* Physical address of PML4 */
    virtual_addr_t  brk;          /* Program break (heap end) */
    virtual_addr_t  stack_start;  /* User stack base */
} memory_context_t;

/* ==============================================================================
 * Process Control Block (PCB)
 * ============================================================================== */

/**
 * @brief Process Control Block
 *
 * This structure contains all information about a process.
 */
typedef struct pcb {
    /* ===== Process Identification ===== */
    int32_t            pid;            /* Process ID */
    int32_t            ppid;           /* Parent process ID */

    /* ===== Process State ===== */
    process_state_t    state;          /* Current state */
    int32_t            exit_code;      /* Exit code (if zombie) */

    /* ===== Scheduling ===== */
    list_head          run_list;       /* Run queue list (legacy, for compatibility) */
    list_head          siblings;       /* Sibling list (parent's children) */
    list_head          children;       /* Children list */
    list_head          zombie_children;/* Reapable children */

    /* ===== Scheduler Entity ===== */
    sched_entity_t     sched_entity;   /* Scheduler class integration */

    /* ===== Memory Context ===== */
    memory_context_t   mm;             /* Address space info */

    /* ===== CPU Context ===== */
    cpu_context_t*     cpu_ctx;        /* Saved kernel context */
    trap_frame_t*      trap_frame;     /* User trap frame */
    virtual_addr_t     kernel_stack;   /* Kernel stack top (for TSS.rsp0) */
    virtual_addr_t     kernel_stack_base; /* Kernel stack base */

    /* ===== User Mode Specific ===== */
    bool               is_user_mode;   /* true = user process, false = kernel process */
    virtual_addr_t     user_stack;     /* User stack top */
    size_t             user_stack_size;/* User stack size */

    /* ===== Parent/Child Relations ===== */
    struct pcb*        parent;         /* Parent process */

    /* ===== Statistics ===== */
    uint64_t           start_time;     /* Process creation time */
    char               comm[16];       /* Command name */

    /* ===== Thread Identification ===== */
    int32_t            tgid;           /* Thread Group ID (PID of thread leader) */
    bool               is_thread;      /* true = this is a thread, not a process */
    list_head          thread_list;    /* List of threads in this thread group */
    list_head          thread_group;   /* Node in leader's thread_list */

    /* ===== Address Space Reference Counting ===== */
    atomic_t           mm_refcount;    /* Reference count for shared mm */

    /* ===== Thread Join Support ===== */
    void*              join_waiters;    /* Opaque pointer to wait_queue_head_t */
    bool               detached;       /* true = thread cannot be joined */
    void*              return_value;   /* Thread return value */

    /* ===== Thread Entry Point ===== */
    virtual_addr_t     thread_entry;   /* User space function pointer */
    virtual_addr_t     thread_arg;     /* Argument to thread function */

} pcb_t;

/* ==============================================================================
 * Scheduler Structure
 * ============================================================================== */

/**
 * @brief Global scheduler state
 */
typedef struct scheduler {
    list_head          run_queue;      /* Run queue */
    pcb_t*             current;        /* Currently running process */
    pcb_t*             idle;           /* Idle process */
    uint32_t           nr_running;     /* Number of running processes */
    bool               need_resched;   /* Reschedule flag */
    sched_rq_t*        rq;             /* Per-policy run queues array (allocated) */
    sched_class_t**    classes;        /* Registered scheduling classes array (allocated) */
} scheduler_t;

/* ==============================================================================
 * Process Management API
 * ============================================================================== */

/**
 * @brief Initialize process subsystem
 * @return 0 on success, negative on error
 */
int proc_init(void);

/**
 * @brief Create a new process (fork)
 * @return Child PID to parent, 0 to child, negative on error
 */
int32_t proc_fork(void);

/**
 * @brief Exit current process
 * @param exit_code Exit code
 */
void proc_exit(int exit_code) __attribute__((noreturn));

/**
 * @brief Wait for a child process to exit
 * @param pid PID to wait for (-1 for any)
 * @param wstatus Pointer to store exit status
 * @param options Wait options (WNOHANG, etc.)
 * @return PID of exited child, or negative on error
 */
int32_t proc_wait4(int32_t pid, int* wstatus, int options);

/**
 * @brief Allocate a new PCB
 * @return Pointer to new PCB, or NULL on failure
 */
pcb_t* proc_alloc_pcb(void);

/**
 * @brief Free a PCB
 * @param pcb PCB to free
 */
void proc_free_pcb(pcb_t* pcb);

/**
 * @brief Get current process
 * @return Pointer to current PCB, or NULL if no current process
 */
static inline pcb_t* proc_current(void) {
    extern scheduler_t scheduler;
    return scheduler.current;
}

/**
 * @brief Find process by PID
 * @param pid Process ID to find
 * @return Pointer to PCB, or NULL if not found
 */
pcb_t* proc_find(int32_t pid);

/**
 * @brief Schedule next process
 */
void schedule(void);

/* ==============================================================================
 * PID Allocator API
 * ============================================================================== */

/**
 * @brief Initialize PID allocator
 */
void pid_alloc_init(void);

/**
 * @brief Allocate a new PID
 * @return Allocated PID, or negative on error
 */
int32_t pid_alloc(void);

/**
 * @brief Free a PID
 * @param pid PID to free
 */
void pid_free(int32_t pid);

/* ==============================================================================
 * Thread Management API
 * ==============================================================================
 */

/**
 * @brief Thread function type
 */
typedef void (*thread_fn_t)(void* arg);

/**
 * @brief Create a new kernel thread
 * @param fn Thread entry point function
 * @param arg Argument to pass to thread function
 * @param name Thread name for debugging
 * @return Pointer to new PCB, or NULL on failure
 */
pcb_t* proc_create_kernel_thread(thread_fn_t fn, void* arg, const char* name);

/**
 * @brief Create a new user thread
 * @param parent Parent thread (current process)
 * @param entry User-space function pointer
 * @param arg User-space argument
 * @param stack_addr User stack address (0 to auto-allocate)
 * @param stack_size Stack size in bytes
 * @return TID on success, negative on error
 */
int32_t proc_create_user_thread(pcb_t* parent, virtual_addr_t entry,
                                virtual_addr_t arg,
                                virtual_addr_t stack_addr,
                                size_t stack_size);

/**
 * @brief Exit current thread
 * @param retval Return value
 */
void proc_thread_exit(void* retval) __attribute__((noreturn));

/**
 * @brief Wait for a thread to exit
 * @param tid Thread ID to wait for
 * @param retval Pointer to store return value
 * @return 0 on success, negative on error
 */
int32_t proc_thread_join(int32_t tid, void** retval);

/**
 * @brief Detach a thread (cannot be joined)
 * @param tid Thread ID to detach
 * @return 0 on success, negative on error
 */
int32_t proc_thread_detach(int32_t tid);

/**
 * @brief Find thread by TID
 * @param tid Thread ID to find
 * @return Pointer to PCB, or NULL if not found
 */
pcb_t* proc_find_by_tid(int32_t tid);
