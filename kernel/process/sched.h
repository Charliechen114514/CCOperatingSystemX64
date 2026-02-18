/* ==============================================================================
 * CCOS - Scheduling Class Framework
 * ==============================================================================
 * This module provides an extensible scheduler class system inspired by Linux's
 * modular scheduling architecture. Multiple scheduling classes (Round-Robin,
 * Priority, etc.) can coexist and new algorithms can be easily added.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"
#include "list/list.h"

/* ==============================================================================
 * Forward Declarations
 * ==============================================================================
 */

struct pcb;
struct sched_class;
struct sched_rq;

/* ==============================================================================
 * Scheduling Policy Enumeration
 * ==============================================================================
 */

/**
 * @brief Scheduling policy types
 */
typedef enum sched_policy {
    SCHED_NORMAL   = 0,    /* Normal Round-Robin scheduling */
    SCHED_PRIORITY = 1,    /* Priority-based scheduling (system only) */
    SCHED_MAX      = 2,    /* Number of scheduling policies */
} sched_policy_t;

/* ==============================================================================
 * Scheduling Class Structure
 * ==============================================================================
 */

/**
 * @brief Scheduling class operations
 *
 * Each scheduling class implements these operations to define
 * its scheduling behavior.
 */
typedef struct sched_class {
    const char* name;              /* Class name for debugging */
    sched_policy_t policy;         /* Policy identifier */

    /**
     * @brief Enqueue a task on this class's run queue
     * @param rq The run queue
     * @param pcb The task to enqueue
     * @param head If true, add to head; otherwise add to tail
     */
    void (*enqueue_task)(struct sched_rq* rq, struct pcb* pcb, bool head);

    /**
     * @brief Dequeue a task from this class's run queue
     * @param rq The run queue
     * @param pcb The task to dequeue
     */
    void (*dequeue_task)(struct sched_rq* rq, struct pcb* pcb);

    /**
     * @brief Pick the next task to run
     * @param rq The run queue
     * @param prev The previously running task
     * @return The next task to run, or NULL if queue is empty
     */
    struct pcb* (*pick_next_task)(struct sched_rq* rq, struct pcb* prev);

    /**
     * @brief Check if a task can preempt another
     * @param p The preempting task
     * @param curr The currently running task
     * @return true if p should preempt curr
     */
    bool (*should_preempt)(struct pcb* p, struct pcb* curr);

    /**
     * @brief Called when a task's time slice expires
     * @param rq The run queue
     * @param pcb The task whose time slice expired
     */
    void (*task_tick)(struct sched_rq* rq, struct pcb* pcb);

    /**
     * @brief Initialize a new task for this class
     * @param pcb The task to initialize
     * @param nice Priority modifier (for future use)
     */
    void (*task_fork)(struct pcb* pcb, int nice);

    /**
     * @brief Get the default time slice for a task
     * @param pcb The task
     * @return Time slice in milliseconds
     */
    uint32_t (*get_time_slice)(const struct pcb* pcb);

} sched_class_t;

/* ==============================================================================
 * Per-Class Run Queue Structure
 * ==============================================================================
 */

/**
 * @brief Per-policy run queue
 *
 * Each scheduling class maintains its own run queue.
 */
typedef struct sched_rq {
    sched_class_t* sched_class;    /* Associated scheduling class */
    list_head        queue;         /* Run queue for this class */
    uint32_t         nr_running;    /* Number of tasks on this queue */

    /* Class-specific data */
    void*            class_data;    /* Opaque pointer to class-specific data */

} sched_rq_t;

/* ==============================================================================
 * Per-Task Scheduling Entity
 * ==============================================================================
 */

/**
 * @brief Per-task scheduling data
 */
typedef struct sched_entity {
    sched_policy_t    policy;         /* Scheduling policy for this task */
    sched_class_t*    sched_class;    /* Associated scheduling class */
    uint32_t          time_slice;     /* Remaining time slice (in ms) */
    uint32_t          time_slice_total; /* Full time slice (in ms) */
    int               priority;       /* Priority level (0-127, lower = higher) */
    int               nice;           /* Nice value for future extensions */
    uint64_t          last_ran;       /* Last time this task ran */
    list_head         run_list;       /* Node for class-specific run queue */

} sched_entity_t;

/* ==============================================================================
 * Extended Scheduler Structure
 * ==============================================================================
 */

/**
 * @brief Forward declaration for scheduler_t from process.h
 * This allows us to extend it without circular dependencies.
 */
typedef struct scheduler scheduler_t;

/* ==============================================================================
 * Time Slice Constants
 * ==============================================================================
 */

/**
 * @brief Default time slices (in milliseconds, at 1000Hz)
 */
#define DEF_TIMESLICE_MS     10      /* Default 10ms time slice */
#define MIN_TIMESLICE_MS     2       /* Minimum 2ms */
#define MAX_TIMESLICE_MS     100     /* Maximum 100ms */

/* ==============================================================================
 * Scheduling Class API
 * ==============================================================================
 */

/**
 * @brief Initialize the scheduler class framework
 * @return 0 on success, negative on error
 */
int sched_class_init(void);

/**
 * @brief Register a scheduling class
 * @param class The scheduling class to register
 * @param policy The policy identifier
 * @return 0 on success, negative on error
 */
int sched_class_register(sched_class_t* class, sched_policy_t policy);

/**
 * @brief Enqueue a task on its class's run queue
 * @param pcb The task to enqueue
 * @param head If true, add to head; otherwise add to tail
 */
void sched_enqueue_task(struct pcb* pcb, bool head);

/**
 * @brief Dequeue a task from its class's run queue
 * @param pcb The task to dequeue
 */
void sched_dequeue_task(struct pcb* pcb);

/**
 * @brief Pick the next task to run
 * @return The next task to run, or NULL if no tasks
 */
struct pcb* sched_pick_next_task(void);

/**
 * @brief Handle timer tick for current task
 * Called from timer interrupt handler
 */
void sched_tick(void);

/**
 * @brief Check if we need to reschedule
 * @return true if reschedule is needed
 */
bool sched_needs_reschedule(void);

/**
 * @brief Set the need_resched flag
 */
void sched_set_resched(void);

/**
 * @brief Clear the need_resched flag
 */
void sched_clear_resched(void);

/* ==============================================================================
 * Time Slice Management
 * ==============================================================================
 */

/**
 * @brief Initialize time slice for a task
 * @param pcb The task
 */
void sched_init_time_slice(struct pcb* pcb);

/**
 * @brief Reset time slice for a task
 * @param pcb The task
 */
void sched_reset_time_slice(struct pcb* pcb);

/* ==============================================================================
 * Policy Selection Helpers
 * ==============================================================================
 */

/**
 * @brief Set scheduling policy for a task
 * @param pcb The task
 * @param policy The policy to set
 * @param priority Priority level (for SCHED_PRIORITY)
 * @return 0 on success, negative on error
 */
int sched_set_policy(struct pcb* pcb, sched_policy_t policy, int priority);

/**
 * @brief Get scheduling policy for a task
 * @param pcb The task
 * @return The task's scheduling policy
 */
sched_policy_t sched_get_policy(const struct pcb* pcb);
