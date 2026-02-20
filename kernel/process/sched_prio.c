/* ==============================================================================
 * CCOS - Priority Scheduling Class Implementation
 * ==============================================================================
 */

#include "process/sched_prio.h"
#include "assert/assert.h"
#include "base/memory.h"
#include "klogs/kprintf.h"
#include "mm/heap/heap.h"
#include "process/process.h"
#include "process/sched.h"

/* ==============================================================================
 * Forward Declarations
 * ==============================================================================
 */

struct pcb;
struct sched_rq;

/* ==============================================================================
 * Priority Class Operations
 * ==============================================================================
 */

/**
 * @brief Enqueue a task on the priority run queue
 */
static void prio_enqueue_task(struct sched_rq* rq, struct pcb* pcb, bool head);

/**
 * @brief Dequeue a task from the priority run queue
 */
static void prio_dequeue_task(struct sched_rq* rq, struct pcb* pcb);

/**
 * @brief Pick the next task from priority queue
 * Selects highest priority task, FIFO within same priority
 */
static struct pcb* prio_pick_next_task(struct sched_rq* rq, struct pcb* prev);

/**
 * @brief Check if a task should preempt
 * Higher priority (lower number) always preempts lower priority
 */
static bool prio_should_preempt(struct pcb* p, struct pcb* curr);

/**
 * @brief Handle timer tick for priority task
 */
static void prio_task_tick(struct sched_rq* rq, struct pcb* pcb);

/**
 * @brief Initialize a new task for priority scheduling
 */
static void prio_task_fork(struct pcb* pcb, int nice);

/**
 * @brief Get time slice for priority task
 */
static uint32_t prio_get_time_slice(const struct pcb* pcb);

/* ==============================================================================
 * Priority Class Structure
 * ==============================================================================
 */

static sched_class_t prio_sched_class = {
    .name = "PRIO",
    .policy = SCHED_PRIORITY,
    .enqueue_task = prio_enqueue_task,
    .dequeue_task = prio_dequeue_task,
    .pick_next_task = prio_pick_next_task,
    .should_preempt = prio_should_preempt,
    .task_tick = prio_task_tick,
    .task_fork = prio_task_fork,
    .get_time_slice = prio_get_time_slice,
};

/* ==============================================================================
 * Helper Functions
 * ==============================================================================
 */

/**
 * @brief Get the run queue data for priority scheduler
 */
static inline prio_rq_data_t* prio_rq_data(struct sched_rq* rq) {
    return (prio_rq_data_t*)rq->class_data;
}

/**
 * @brief Find the highest priority with active tasks
 */
static int find_highest_prio(prio_rq_data_t* data) {
    for (int i = 0; i < PRIO_LEVELS; i++) {
        if (!list_is_empty(&data->active[i])) {
            return i;
        }
    }
    return PRIO_MIN;
}

/**
 * @brief Swap active and expired arrays
 */
static void prio_swap_active_expired(prio_rq_data_t* data) {
    for (int i = 0; i < PRIO_LEVELS; i++) {
        list_head temp;
        INIT_LIST_HEAD(&temp);

        /* Swap active[i] with expired[i] */
        list_splice_init(&data->active[i], &temp);
        list_splice_init(&data->expired[i], &data->active[i]);
        list_splice_init(&temp, &data->expired[i]);
    }

    /* Swap counters */
    uint32_t temp_count = data->nr_active;
    data->nr_active = data->nr_expired;
    data->nr_expired = temp_count;

    data->highest_prio = find_highest_prio(data);
    data->active_expired = false;
}

/* ==============================================================================
 * Priority Operations Implementation
 * ==============================================================================
 */

/**
 * @brief Enqueue a task on the priority run queue
 */
static void prio_enqueue_task(struct sched_rq* rq, struct pcb* pcb, bool head) {
    prio_rq_data_t* data = prio_rq_data(rq);
    int prio = pcb->sched_entity.priority;

    if (prio < PRIO_MAX) {
        prio = PRIO_MAX;
    } else if (prio > PRIO_MIN) {
        prio = PRIO_MIN;
    }

    /* If time slice is exhausted, enqueue to expired queue instead of active.
     * This handles the case where a task's time slice expired while it was running. */
    bool to_expired = (pcb->sched_entity.time_slice == 0);

    klog_info("[PRIO] Enqueue PID=%d: prio=%d, time_slice=%d/%d, to_expired=%d, head=%d\n",
              pcb->pid, prio, pcb->sched_entity.time_slice, pcb->sched_entity.time_slice_total,
              to_expired, head);

    if (to_expired) {
        /* Enqueue to expired queue */
        if (head) {
            list_add(&pcb->sched_entity.run_list, &data->expired[prio]);
        } else {
            list_add_tail(&pcb->sched_entity.run_list, &data->expired[prio]);
        }
        data->nr_expired++;
        klog_info("[PRIO] Enqueued PID=%d to EXPIRED queue (prio=%d), nr_expired=%d\n", pcb->pid,
                  prio, data->nr_expired);
    } else {
        /* Enqueue to active queue */
        if (head) {
            list_add(&pcb->sched_entity.run_list, &data->active[prio]);
        } else {
            list_add_tail(&pcb->sched_entity.run_list, &data->active[prio]);
        }
        data->nr_active++;
        klog_info("[PRIO] Enqueued PID=%d to ACTIVE queue (prio=%d), nr_active=%d\n", pcb->pid,
                  prio, data->nr_active);
        /* Update highest priority if needed */
        if (prio < data->highest_prio) {
            data->highest_prio = prio;
        }
    }

    rq->nr_running++; /* FIX: Update run queue counter */
    klog_info("[PRIO] After enqueue: rq->nr_running=%d, nr_active=%d, nr_expired=%d\n",
              rq->nr_running, data->nr_active, data->nr_expired);
}

/**
 * @brief Dequeue a task from the priority run queue
 */
static void prio_dequeue_task(struct sched_rq* rq, struct pcb* pcb) {
    prio_rq_data_t* data = prio_rq_data(rq);

    /* Remove from whichever queue it's on */
    list_del_init(&pcb->sched_entity.run_list);

    /* We need to determine which queue the task was on.
     * Since we can't tell after removing it, we use a heuristic:
     * If time_slice is 0, it was likely on expired queue (just finished).
     * Otherwise, it was likely on active queue. */
    if (pcb->sched_entity.time_slice == 0 && data->nr_expired > 0) {
        data->nr_expired--;
    } else if (data->nr_active > 0) {
        data->nr_active--;
    }

    rq->nr_running--; /* FIX: Update run queue counter */

    /* Update highest priority */
    data->highest_prio = find_highest_prio(data);
}

/**
 * @brief Pick the next task from priority queue
 */
static struct pcb* prio_pick_next_task(struct sched_rq* rq, struct pcb* prev) {
    (void)prev; /* Not used for priority selection */
    prio_rq_data_t* data = prio_rq_data(rq);

    klog_info(
        "[PRIO] pick_next_task: nr_active=%d, nr_expired=%d, highest_prio=%d, rq->nr_running=%d\n",
        data->nr_active, data->nr_expired, data->highest_prio, rq->nr_running);

    /* If no active tasks, try to swap arrays */
    if (data->nr_active == 0) {
        if (data->nr_expired > 0) {
            klog_info("[PRIO] Swapping active/expired queues (nr_expired=%d)\n", data->nr_expired);
            prio_swap_active_expired(data);
            klog_info("[PRIO] After swap: nr_active=%d, nr_expired=%d, highest_prio=%d\n",
                      data->nr_active, data->nr_expired, data->highest_prio);
        } else {
            klog_info("[PRIO] No tasks available (nr_active=0, nr_expired=0), returning NULL\n");
            return NULL; /* No tasks at all */
        }
    }

    /* Get highest priority queue */
    int highest = find_highest_prio(data);
    klog_info("[PRIO] Highest priority: %d (PRIO_LEVELS=%d)\n", highest, PRIO_LEVELS);
    if (highest >= PRIO_LEVELS) {
        klog_info("[PRIO] Invalid highest priority %d >= %d\n", highest, PRIO_LEVELS);
        return NULL;
    }

    /* Return first task from highest priority queue */
    struct pcb* next = list_first_entry(&data->active[highest], struct pcb, sched_entity.run_list);
    klog_info(
        "[PRIO] Returning task PID=%d (time_slice=%d/%d, time_slice_total=%d, sched_class=%p)\n",
        next ? next->pid : -1, next ? next->sched_entity.time_slice : 0,
        next ? next->sched_entity.time_slice_total : 0,
        next ? next->sched_entity.time_slice_total : 0,
        next ? (void*)next->sched_entity.sched_class : NULL);
    return next;
}

/**
 * @brief Check if a task should preempt
 * Higher priority (lower number) always preempts lower priority
 */
static bool prio_should_preempt(struct pcb* p, struct pcb* curr) {
    /* Higher priority (lower number) preempts */
    if (p->sched_entity.priority < curr->sched_entity.priority) {
        return true;
    }

    /* Same priority: only if current exhausted time slice */
    if (p->sched_entity.priority == curr->sched_entity.priority) {
        return (curr->sched_entity.time_slice == 0);
    }

    return false;
}

/**
 * @brief Handle timer tick for priority task
 */
static void prio_task_tick(struct sched_rq* rq, struct pcb* pcb) {
    (void)rq; /* Not used for priority scheduling */

    /* Decrement time slice */
    if (pcb->sched_entity.time_slice > 0) {
        pcb->sched_entity.time_slice--;
    }

    /* If time slice expired, move to expired queue */
}

/**
 * @brief Initialize a new task for priority scheduling
 */
static void prio_task_fork(struct pcb* pcb, int nice) {
    pcb->sched_entity.sched_class = &prio_sched_class;
    pcb->sched_entity.policy = SCHED_PRIORITY;

    /* Set priority from nice value or use default */
    if (nice < 0) {
        pcb->sched_entity.priority = PRIO_DEFAULT + nice;
        if (pcb->sched_entity.priority < PRIO_MAX) {
            pcb->sched_entity.priority = PRIO_MAX;
        }
    } else {
        pcb->sched_entity.priority = PRIO_DEFAULT;
    }

    /* Clamp priority range */
    if (pcb->sched_entity.priority < PRIO_MAX) {
        pcb->sched_entity.priority = PRIO_MAX;
    } else if (pcb->sched_entity.priority > PRIO_MIN) {
        pcb->sched_entity.priority = PRIO_MIN;
    }

    /* Set time slice based on priority */
    pcb->sched_entity.time_slice = PRIO_TIMESLICE(pcb->sched_entity.priority);
    pcb->sched_entity.time_slice_total = pcb->sched_entity.time_slice;
    pcb->sched_entity.nice = nice;
}

/**
 * @brief Get time slice for priority task
 */
static uint32_t prio_get_time_slice(const struct pcb* pcb) {
    return PRIO_TIMESLICE(pcb->sched_entity.priority);
}

/* ==============================================================================
 * Priority Class Registration
 * ==============================================================================
 */

/**
 * @brief Initialize the Priority scheduling class
 */
int sched_prio_init(void) {
    klog_info("[SCHED] Initializing Priority scheduling class\n");

    /* Allocate per-policy run queue data */
    prio_rq_data_t* data = (prio_rq_data_t*)kmalloc(sizeof(prio_rq_data_t));
    if (!data) {
        klog_error("[SCHED] Failed to allocate priority queue data\n");
        return -1;
    }

    /* Initialize queues */
    for (int i = 0; i < PRIO_LEVELS; i++) {
        INIT_LIST_HEAD(&data->active[i]);
        INIT_LIST_HEAD(&data->expired[i]);
    }
    data->nr_active = 0;
    data->nr_expired = 0;
    data->highest_prio = PRIO_MIN;
    data->active_expired = false;

    extern scheduler_t scheduler;

    /* Get the run queue for priority scheduling and set class_data */
    /* The run queue array is already initialized by sched_class_init */
    sched_rq_t* rq = &scheduler.rq[SCHED_PRIORITY];

    rq->class_data = data;

    /* Register the class */
    int ret = sched_class_register(&prio_sched_class, SCHED_PRIORITY);
    if (ret != 0) {
        klog_error("[SCHED] Failed to register Priority class\n");
        kfree(data);
        return ret;
    }

    klog_info("[SCHED] Priority class initialized (levels=%d)\n", PRIO_LEVELS);
    return 0;
}

/**
 * @brief Get the Priority class structure
 */
sched_class_t* sched_prio_get_class(void) {
    return &prio_sched_class;
}
