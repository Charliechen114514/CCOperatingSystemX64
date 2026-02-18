/* ==============================================================================
 * CCOS - Priority Scheduling Class Implementation
 * ==============================================================================
 */

#include "process/sched_prio.h"
#include "process/process.h"
#include "process/sched.h"
#include "mm/heap/heap.h"
#include "klogs/kprintf.h"
#include "assert/assert.h"
#include "base/memory.h"

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
    .name            = "PRIO",
    .policy          = SCHED_PRIORITY,
    .enqueue_task    = prio_enqueue_task,
    .dequeue_task    = prio_dequeue_task,
    .pick_next_task  = prio_pick_next_task,
    .should_preempt  = prio_should_preempt,
    .task_tick       = prio_task_tick,
    .task_fork       = prio_task_fork,
    .get_time_slice  = prio_get_time_slice,
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

    /* Enqueue to active queue */
    if (head) {
        list_add(&pcb->sched_entity.run_list, &data->active[prio]);
    } else {
        list_add_tail(&pcb->sched_entity.run_list, &data->active[prio]);
    }

    data->nr_active++;

    /* Update highest priority if needed */
    if (prio < data->highest_prio) {
        data->highest_prio = prio;
    }
}

/**
 * @brief Dequeue a task from the priority run queue
 */
static void prio_dequeue_task(struct sched_rq* rq, struct pcb* pcb) {
    prio_rq_data_t* data = prio_rq_data(rq);

    /* Remove from whichever queue it's on */
    list_del_init(&pcb->sched_entity.run_list);

    /* Check which queue the task was on by checking if list is empty */
    /* We already removed it, so just decrement the appropriate counter */
    /* For simplicity, we decrement from active if it was likely there */
    /* This is a simplified approach - in a full implementation, we'd track better */
    if (data->nr_active > 0) {
        data->nr_active--;
    } else if (data->nr_expired > 0) {
        data->nr_expired--;
    }

    /* Update highest priority */
    data->highest_prio = find_highest_prio(data);
}

/**
 * @brief Pick the next task from priority queue
 */
static struct pcb* prio_pick_next_task(struct sched_rq* rq, struct pcb* prev) {
    (void)prev;  /* Not used for priority selection */
    prio_rq_data_t* data = prio_rq_data(rq);

    /* If no active tasks, try to swap arrays */
    if (data->nr_active == 0) {
        if (data->nr_expired > 0) {
            prio_swap_active_expired(data);
        } else {
            return NULL;  /* No tasks at all */
        }
    }

    /* Get highest priority queue */
    int highest = find_highest_prio(data);
    if (highest >= PRIO_LEVELS) {
        return NULL;
    }

    /* Return first task from highest priority queue */
    struct pcb* next = list_first_entry(&data->active[highest],
                                       struct pcb, sched_entity.run_list);
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
    prio_rq_data_t* data = prio_rq_data(rq);

    /* Decrement time slice */
    if (pcb->sched_entity.time_slice > 0) {
        pcb->sched_entity.time_slice--;
    }

    /* If time slice expired, move to expired queue */
    if (pcb->sched_entity.time_slice == 0) {
        int prio = pcb->sched_entity.priority;

        /* Move from active to expired */
        list_del_init(&pcb->sched_entity.run_list);
        list_add_tail(&pcb->sched_entity.run_list, &data->expired[prio]);

        data->nr_active--;
        data->nr_expired++;

        /* Update highest priority */
        data->highest_prio = find_highest_prio(data);

        /* Trigger reschedule */
        sched_set_resched();
    }
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
