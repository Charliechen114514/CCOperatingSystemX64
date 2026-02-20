/* ==============================================================================
 * CCOS - Scheduling Class Framework Implementation
 * ==============================================================================
 */

#include "process/sched.h"
#include "driver/timer/timer.h"
#include "klogs/kprintf.h"
#include "process/process.h"
#include "sync/spinlock.h"

/* ==============================================================================
 * Global Scheduler State
 * ==============================================================================
 */

/* Global scheduler instance */
extern scheduler_t scheduler;

/* Static arrays for scheduler class data */
static sched_rq_t s_run_queues[SCHED_MAX];
static sched_class_t* s_classes[SCHED_MAX];

/* Global scheduler lock - exported for use in process.c */
spinlock_t g_scheduler_lock = SPIN_LOCK_INIT;

/* ==============================================================================
 * Timer Tick Handler
 * ==============================================================================
 */

/**
 * @brief Timer tick callback for scheduling
 * Called from timer_irq_handler at 1000Hz
 *
 * NOTE: Runs in interrupt context, must use irqsave variants
 */
static void sched_timer_tick_handler(uint64_t ticks) {
    (void)ticks;

    spinlock_flags_t flags;
    spin_lock_irqsave(&g_scheduler_lock, &flags);

    /* Only tick if we have a running process with a scheduling class */
    if (scheduler.current && scheduler.rq != NULL) {
        if (scheduler.current->sched_entity.sched_class) {
            sched_rq_t* rq = &scheduler.rq[scheduler.current->sched_entity.policy];
            scheduler.current->sched_entity.sched_class->task_tick(rq, scheduler.current);
        }
    }

    spin_unlock_irqrestore(&g_scheduler_lock, flags);
}

/* ==============================================================================
 * Time Slice Management Functions
 * ==============================================================================
 */

/**
 * @brief Initialize time slice for a task
 */
void sched_init_time_slice(struct pcb* pcb) {
    if (!pcb || !pcb->sched_entity.sched_class) {
        return;
    }

    uint32_t slice = pcb->sched_entity.sched_class->get_time_slice(pcb);
    pcb->sched_entity.time_slice = slice;
    pcb->sched_entity.time_slice_total = slice;
}

/**
 * @brief Reset time slice for a task
 */
void sched_reset_time_slice(struct pcb* pcb) {
    if (!pcb || !pcb->sched_entity.sched_class) {
        return;
    }

    pcb->sched_entity.time_slice = pcb->sched_entity.time_slice_total;
}

/* ==============================================================================
 * Scheduler Class Initialization
 * ==============================================================================
 */

/**
 * @brief Initialize the scheduler class framework
 */
int sched_class_init(void) {
    klog_info("[SCHED] Initializing scheduler class framework\n");

    /* Initialize static run queues */
    for (int i = 0; i < SCHED_MAX; i++) {
        INIT_LIST_HEAD(&s_run_queues[i].queue);
        s_run_queues[i].sched_class = NULL;
        s_run_queues[i].nr_running = 0;
        s_run_queues[i].class_data = NULL;
        s_classes[i] = NULL;
    }

    /* Link to scheduler */
    scheduler.rq = s_run_queues;
    scheduler.classes = s_classes;

    /* Initialize legacy run queue for compatibility */
    INIT_LIST_HEAD(&scheduler.run_queue);

    /* Register timer callback */
    timer_set_callback(sched_timer_tick_handler);

    klog_info("[SCHED] Scheduler class framework initialized\n");
    return 0;
}

/* ==============================================================================
 * Class Registration
 * ==============================================================================
 */

/**
 * @brief Register a scheduling class
 */
int sched_class_register(sched_class_t* class, sched_policy_t policy) {
    if (!class || policy >= SCHED_MAX) {
        return -1;
    }

    if (s_classes[policy] != NULL) {
        klog_warn("[SCHED] Class already registered for policy %d\n", policy);
        return -1;
    }

    s_classes[policy] = class;
    s_run_queues[policy].sched_class = class;

    klog_info("[SCHED] Registered class '%s' for policy %d\n", class->name, policy);
    return 0;
}

/* ==============================================================================
 * Task Queue Operations
 * ==============================================================================
 */

/**
 * @brief Enqueue a task on its class's run queue
 */
void sched_enqueue_task(struct pcb* pcb, bool head) {
    if (!pcb || !pcb->sched_entity.sched_class) {
        return;
    }

    spinlock_flags_t flags;
    spin_lock_irqsave(&g_scheduler_lock, &flags);

    sched_rq_t* rq = &scheduler.rq[pcb->sched_entity.policy];

    pcb->sched_entity.sched_class->enqueue_task(rq, pcb, head);

    /* Update legacy nr_running */
    scheduler.nr_running++;

    /* Also add to legacy run_queue for compatibility */
    list_add_tail(&pcb->run_list, &scheduler.run_queue);

    spin_unlock_irqrestore(&g_scheduler_lock, flags);
}

/**
 * @brief Dequeue a task from its class's run queue
 */
void sched_dequeue_task(struct pcb* pcb) {
    if (!pcb || !pcb->sched_entity.sched_class) {
        return;
    }

    spinlock_flags_t flags;
    spin_lock_irqsave(&g_scheduler_lock, &flags);

    sched_rq_t* rq = &scheduler.rq[pcb->sched_entity.policy];

    pcb->sched_entity.sched_class->dequeue_task(rq, pcb);

    /* Update legacy nr_running */
    if (scheduler.nr_running > 0) {
        scheduler.nr_running--;
    }

    /* Also remove from legacy run_queue */
    list_del_init(&pcb->run_list);

    spin_unlock_irqrestore(&g_scheduler_lock, flags);
}

/* ==============================================================================
 * Task Selection
 * ==============================================================================
 */

/**
 * @brief Pick the next task to run
 *
 * Selection order:
 * 1. If current is still runnable and has time slice, keep it
 * 2. Otherwise, pick from priority queues first
 * 3. Then pick from normal RR queue
 *
 * NOTE: Caller must hold g_scheduler_lock
 */
struct pcb* sched_pick_next_task(void) {
    struct pcb* prev = scheduler.current;

    /* If current is still runnable, check if we should keep it */
    if (prev && prev->state == PROC_RUNNING) {
        if (prev->sched_entity.time_slice > 0) {
            /* Check if any higher priority task wants to run */
            sched_rq_t* prio_rq = &scheduler.rq[SCHED_PRIORITY];
            if (prio_rq->nr_running > 0 && prio_rq->sched_class) {
                struct pcb* prio_task = prio_rq->sched_class->pick_next_task(prio_rq, prev);
                if (prio_task &&
                    prio_task->sched_entity.sched_class->should_preempt(prio_task, prev)) {
                    /* Re-enqueue current and return priority task */
                    return prio_task;
                }
            }
            /* Keep current */
            return prev;
        }
    }

    /* Check priority queue first */
    sched_rq_t* prio_rq = &scheduler.rq[SCHED_PRIORITY];
    if (prio_rq->nr_running > 0 && prio_rq->sched_class) {
        struct pcb* next = prio_rq->sched_class->pick_next_task(prio_rq, prev);
        if (next) {
            return next;
        }
    }

    /* Fall back to normal RR queue */
    sched_rq_t* rr_rq = &scheduler.rq[SCHED_NORMAL];
    if (rr_rq->nr_running > 0 && rr_rq->sched_class) {
        struct pcb* next = rr_rq->sched_class->pick_next_task(rr_rq, prev);
        if (next) {
            return next;
        }
    }

    /* No tasks available */
    return NULL;
}

/* ==============================================================================
 * Reschedule Management
 * ==============================================================================
 */

/**
 * @brief Check if we need to reschedule
 */
bool sched_needs_reschedule(void) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&g_scheduler_lock, &flags);
    bool needs = scheduler.need_resched;
    spin_unlock_irqrestore(&g_scheduler_lock, flags);
    return needs;
}

/**
 * @brief Set the need_resched flag
 */
void sched_set_resched(void) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&g_scheduler_lock, &flags);
    scheduler.need_resched = true;
    spin_unlock_irqrestore(&g_scheduler_lock, flags);
}

/**
 * @brief Clear the need_resched flag
 */
void sched_clear_resched(void) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&g_scheduler_lock, &flags);
    scheduler.need_resched = false;
    spin_unlock_irqrestore(&g_scheduler_lock, flags);
}

/* ==============================================================================
 * Policy Management
 * ==============================================================================
 */

/**
 * @brief Set scheduling policy for a task
 */
int sched_set_policy(struct pcb* pcb, sched_policy_t policy, int priority) {
    if (!pcb || policy >= SCHED_MAX) {
        return -1;
    }

    if (s_classes[policy] == NULL) {
        klog_error("[SCHED] No class registered for policy %d\n", policy);
        return -1;
    }

    pcb->sched_entity.policy = policy;
    pcb->sched_entity.sched_class = s_classes[policy];

    if (pcb->sched_entity.sched_class) {
        pcb->sched_entity.sched_class->task_fork(pcb, priority);
    }

    return 0;
}

/**
 * @brief Get scheduling policy for a task
 */
sched_policy_t sched_get_policy(const struct pcb* pcb) {
    if (!pcb) {
        return SCHED_NORMAL;
    }
    return pcb->sched_entity.policy;
}
