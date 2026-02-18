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

/* ==============================================================================
 * Process Constants
 * ============================================================================== */

#define PID_MAX           32768
#define KERNEL_STACK_SIZE (16 * 1024)  /* 16KB */
#define USER_STACK        (USER_END - PAGE_SIZE)

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

    /* ===== Parent/Child Relations ===== */
    struct pcb*        parent;         /* Parent process */

    /* ===== Statistics ===== */
    uint64_t           start_time;     /* Process creation time */
    char               comm[16];       /* Command name */

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
