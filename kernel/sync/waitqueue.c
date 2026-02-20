/* ==============================================================================
 * CCOS - Wait Queue Implementation
 * ==============================================================================
 */

#include "sync/waitqueue.h"
#include "process/process.h"
#include "klogs/kprintf.h"

/* ==============================================================================
 * Sleep Operations
 * ==============================================================================
 */

/**
 * @brief Internal helper to add task to wait queue and block
 */
static void add_to_waitqueue_and_block(wait_queue_head_t *wq, bool exclusive) {
    wait_queue_entry_t wq_entry;
    pcb_t *current = proc_current();

    /* Initialize wait queue entry */
    init_waitqueue_entry(&wq_entry, exclusive);

    /* Add to wait queue */
    add_wait_queue(wq, &wq_entry);

    /* Set process state to blocked */
    current->state = PROC_BLOCKED;

    /* Trigger reschedule */
    schedule();
}

void sleep_on(wait_queue_head_t *wq) {
    if (wq == NULL) {
        return;
    }

    add_to_waitqueue_and_block(wq, false);  /* Non-exclusive sleep */
}

int interruptible_sleep_on(wait_queue_head_t *wq) {
    if (wq == NULL) {
        return -1;
    }

    add_to_waitqueue_and_block(wq, false);  /* Non-exclusive sleep */

    /* TODO: Return -ERESTARTSYS if interrupted by signal */
    return 0;
}

/* ==============================================================================
 * Wake Operations
 * ==============================================================================
 */

/**
 * @brief Internal helper to wake up processes from wait queue
 * @param wq Wait queue head
 * @param exclusive_only If true, only wake exclusive waiters
 * @param nr_to_wake Number of processes to wake (0 = all)
 * @return Number of processes actually woken
 */
static int __wake_up_common(wait_queue_head_t *wq, bool exclusive_only, int nr_to_wake) {
    wait_queue_entry_t *wq_entry, *tmp;
    int woken = 0;

    if (wq == NULL || !waitqueue_active(wq)) {
        return 0;
    }

    spinlock_flags_t flags;
    spin_lock_irqsave(&wq->lock, &flags);

    /* Try to wake exclusive waiters first if any */
    list_for_each_entry_safe(wq_entry, tmp, &wq->task_list, list) {
        if (nr_to_wake > 0 && woken >= nr_to_wake) {
            break;
        }

        /* Skip non-exclusive if we only want exclusive */
        if (exclusive_only && !wq_entry->exclusive) {
            continue;
        }

        /* Prefer exclusive waiters */
        if (wq_entry->exclusive) {
            /* Remove from wait queue */
            list_del_init(&wq_entry->list);

            /* Wake the process */
            if (wq_entry->task != NULL) {
                wq_entry->task->state = PROC_READY;
                /* Add to run queue */
                extern void sched_enqueue_task(struct pcb *pcb, bool head);
                sched_enqueue_task(wq_entry->task, false);
                woken++;
            }

            /* If this was exclusive, we're done */
            if (!exclusive_only) {
                break;
            }
        }
    }

    /* If no exclusive waiters or we need to wake more, wake normal waiters */
    if (woken < nr_to_wake || nr_to_wake == 0) {
        list_for_each_entry_safe(wq_entry, tmp, &wq->task_list, list) {
            if (nr_to_wake > 0 && woken >= nr_to_wake) {
                break;
            }

            if (wq_entry->exclusive) {
                continue;  /* Already handled exclusive waiters */
            }

            /* Remove from wait queue */
            list_del_init(&wq_entry->list);

            /* Wake the process */
            if (wq_entry->task != NULL) {
                wq_entry->task->state = PROC_READY;
                /* Add to run queue */
                extern void sched_enqueue_task(struct pcb *pcb, bool head);
                sched_enqueue_task(wq_entry->task, false);
                woken++;
            }
        }
    }

    spin_unlock_irqrestore(&wq->lock, flags);

    /* Trigger reschedule if we woke someone */
    if (woken > 0) {
        extern void sched_set_resched(void);
        sched_set_resched();
    }

    return woken;
}

void wake_up(wait_queue_head_t *wq) {
    __wake_up_common(wq, false, 1);  /* Wake one, prefer exclusive */
}

void wake_up_all(wait_queue_head_t *wq) {
    __wake_up_common(wq, false, 0);  /* Wake all */
}

void wake_up_exclusive(wait_queue_head_t *wq) {
    __wake_up_common(wq, true, 1);  /* Wake one exclusive only */
}
