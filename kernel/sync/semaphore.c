/* ==============================================================================
 * CCOS - Semaphore Implementation
 * ==============================================================================
 */

#include "sync/semaphore.h"
#include "sync/waitqueue.h"
#include "process/process.h"
#include "process/sched.h"
#include "klogs/kprintf.h"

void sem_init(semaphore_t *s, int value) {
    atomic_write(&s->count, value);
    spin_lock_init(&s->lock);
    INIT_LIST_HEAD(&s->wait_list);
}

void sem_wait(semaphore_t *s) {
    /* Fast path: try to decrement immediately */
    while (true) {
        int old_val = atomic_read(&s->count);

        if (old_val > 0) {
            if (atomic_compare_and_swap(&s->count, old_val, old_val - 1)) {
                return;  /* Successfully acquired */
            }
            /* CAS failed, retry */
        } else {
            break;  /* Counter is zero, need to block */
        }
    }

    /* Slow path: counter is zero, block */
    spinlock_flags_t flags;
    spin_lock_irqsave(&s->lock, &flags);

    /* Double-check after acquiring lock */
    int old_val = atomic_read(&s->count);
    if (old_val > 0 && atomic_compare_and_swap(&s->count, old_val, old_val - 1)) {
        spin_unlock_irqrestore(&s->lock, flags);
        return;
    }

    /* Create wait queue */
    wait_queue_head_t wq;
    init_waitqueue_head(&wq);

    /* Create waiter entry */
    struct sem_waiter {
        list_head list;
        pcb_t* task;
        wait_queue_head_t* wq;
    } waiter;

    waiter.task = proc_current();
    waiter.wq = &wq;
    list_add_tail(&waiter.list, &s->wait_list);

    spin_unlock_irqrestore(&s->lock, flags);

    /* Sleep until woken */
    while (atomic_read(&s->count) == 0) {
        sleep_on(&wq);
    }

    /* Try to acquire again */
    while (!atomic_compare_and_swap(&s->count, 1, 0)) {
        /* Wait a bit and retry */
        __asm__ volatile("pause" ::: "memory");
    }

    /* Remove from wait list */
    spin_lock_irqsave(&s->lock, &flags);
    list_del_init(&waiter.list);
    spin_unlock_irqrestore(&s->lock, flags);
}

bool sem_trywait(semaphore_t *s) {
    int old_val = atomic_read(&s->count);

    if (old_val > 0) {
        return atomic_compare_and_swap(&s->count, old_val, old_val - 1);
    }
    return false;
}

void sem_post(semaphore_t *s) {
    atomic_inc(&s->count);

    /* Wake up one waiting process */
    spinlock_flags_t flags;
    spin_lock_irqsave(&s->lock, &flags);

    if (!list_is_empty(&s->wait_list)) {
        struct sem_waiter {
            list_head list;
            pcb_t* task;
            wait_queue_head_t* wq;
        };

        list_head* first = s->wait_list.next;
        struct sem_waiter* waiter = list_entry(first, struct sem_waiter, list);

        /* Remove from wait list */
        list_del_init(first);

        /* Wake up the waiter */
        if (waiter->task != NULL && waiter->wq != NULL) {
            waiter->task->state = PROC_READY;
            sched_enqueue_task(waiter->task, false);

            /* Trigger reschedule */
            sched_set_resched();
        }
    }

    spin_unlock_irqrestore(&s->lock, flags);
}

int sem_getvalue(semaphore_t *s) {
    return atomic_read(&s->count);
}
