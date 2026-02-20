/* ==============================================================================
 * CCOS - Round-Robin Scheduling Class Implementation
 * ==============================================================================
 */

#include "process/sched_rr.h"
#include "assert/assert.h"
#include "klogs/kprintf.h"
#include "process/process.h"
#include "process/sched.h"

/* ==============================================================================
 * Forward Declarations
 * ==============================================================================
 */

struct pcb;
struct sched_rq;

/* ==============================================================================
 * Round-Robin Class Operations
 * ==============================================================================
 */

/**
 * @brief Enqueue a task on the RR run queue
 * Simple FIFO enqueue
 */
static void rr_enqueue_task(struct sched_rq* rq, struct pcb* pcb, bool head);

/**
 * @brief Dequeue a task from the RR run queue
 */
static void rr_dequeue_task(struct sched_rq* rq, struct pcb* pcb);

/**
 * @brief Pick the next task from RR queue
 * Returns the first task in the queue (FIFO)
 */
static struct pcb* rr_pick_next_task(struct sched_rq* rq, struct pcb* prev);

/**
 * @brief Check if a task should preempt
 * In RR, tasks don't preempt based on priority
 * Only preempt if current task exhausted its time slice
 */
static bool rr_should_preempt(struct pcb* p, struct pcb* curr);

/**
 * @brief Handle timer tick for RR task
 */
static void rr_task_tick(struct sched_rq* rq, struct pcb* pcb);

/**
 * @brief Initialize a new task for RR
 */
static void rr_task_fork(struct pcb* pcb, int nice);

/**
 * @brief Get time slice for RR task
 */
static uint32_t rr_get_time_slice(const struct pcb* pcb);

/* ==============================================================================
 * Round-Robin Class Structure
 * ==============================================================================
 */

static sched_class_t rr_sched_class = {
    .name = "RR",
    .policy = SCHED_NORMAL,
    .enqueue_task = rr_enqueue_task,
    .dequeue_task = rr_dequeue_task,
    .pick_next_task = rr_pick_next_task,
    .should_preempt = rr_should_preempt,
    .task_tick = rr_task_tick,
    .task_fork = rr_task_fork,
    .get_time_slice = rr_get_time_slice,
};

/* ==============================================================================
 * Round-Robin Operations Implementation
 * ==============================================================================
 */

/**
 * @brief Enqueue a task on the RR run queue
 */
static void rr_enqueue_task(struct sched_rq* rq, struct pcb* pcb, bool head) {
    if (head) {
        list_add(&pcb->sched_entity.run_list, &rq->queue);
    } else {
        list_add_tail(&pcb->sched_entity.run_list, &rq->queue);
    }

    rq->nr_running++;
}

/**
 * @brief Dequeue a task from the RR run queue
 */
static void rr_dequeue_task(struct sched_rq* rq, struct pcb* pcb) {
    list_del_init(&pcb->sched_entity.run_list);
    rq->nr_running--;
}

/**
 * @brief Pick the next task from RR queue
 */
static struct pcb* rr_pick_next_task(struct sched_rq* rq, struct pcb* prev) {
    (void)prev; /* RR doesn't care about previous task */

    if (list_is_empty(&rq->queue)) {
        return NULL;
    }

    struct pcb* next = list_first_entry(&rq->queue, struct pcb, sched_entity.run_list);
    return next;
}

/**
 * @brief Check if a task should preempt
 * In RR, tasks don't preempt based on priority
 * Only preempt if current task exhausted its time slice
 */
static bool rr_should_preempt(struct pcb* p, struct pcb* curr) {
    (void)p; /* Not used in RR */

    /* Only preempt if current has no time left */
    return (curr->sched_entity.time_slice == 0);
}

/**
 * @brief Handle timer tick for RR task
 */
static void rr_task_tick(struct sched_rq* rq, struct pcb* pcb) {
    (void)rq; /* Not needed for RR */

    /* Decrement time slice */
    if (pcb->sched_entity.time_slice > 0) {
        pcb->sched_entity.time_slice--;
    }
}

/**
 * @brief Initialize a new task for RR
 */
static void rr_task_fork(struct pcb* pcb, int nice) {
    (void)nice; /* Not used yet */

    pcb->sched_entity.sched_class = &rr_sched_class;
    pcb->sched_entity.policy = SCHED_NORMAL;
    pcb->sched_entity.priority = 0; /* Default priority */
    pcb->sched_entity.time_slice = RR_TIMESLICE_DEFAULT;
    pcb->sched_entity.time_slice_total = RR_TIMESLICE_DEFAULT;
    pcb->sched_entity.nice = 0;
}

/**
 * @brief Get time slice for RR task
 */
static uint32_t rr_get_time_slice(const struct pcb* pcb) {
    (void)pcb; /* RR uses fixed time slice */
    return RR_TIMESLICE_DEFAULT;
}

/* ==============================================================================
 * Round-Robin Class Registration
 * ==============================================================================
 */

/**
 * @brief Initialize the RR scheduling class
 */
int sched_rr_init(void) {
    klog_info("[SCHED] Initializing Round-Robin scheduling class\n");

    extern scheduler_t scheduler;

    /* Run queues are initialized in sched_class_init, just register the class */
    /* Register the class */
    int ret = sched_class_register(&rr_sched_class, SCHED_NORMAL);
    if (ret != 0) {
        klog_error("[SCHED] Failed to register RR class\n");
        return ret;
    }

    klog_info("[SCHED] Round-Robin class initialized (timeslice=%d ms)\n", RR_TIMESLICE_DEFAULT);
    return 0;
}

/**
 * @brief Get the RR class structure
 */
sched_class_t* sched_rr_get_class(void) {
    return &rr_sched_class;
}
