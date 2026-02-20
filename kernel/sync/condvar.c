/* ==============================================================================
 * CCOS - Condition Variable Implementation
 * ==============================================================================
 */

#include "sync/condvar.h"
#include "sync/mutex.h"
#include "sync/waitqueue.h"
#include "process/process.h"

void condvar_init(condvar_t *cv) {
    spin_lock_init(&cv->lock);
    INIT_LIST_HEAD(&cv->waiters);
}

void condvar_wait(condvar_t *cv, mutex_t *m) {
    pcb_t* current = proc_current();

    /* Create wait queue for this thread */
    wait_queue_head_t wq;
    init_waitqueue_head(&wq);

    /* Create waiter entry */
    struct condvar_waiter {
        list_head list;
        pcb_t* task;
        mutex_t* m;
        wait_queue_head_t* wq;
    } waiter;

    waiter.task = current;
    waiter.m = m;
    waiter.wq = &wq;

    /* Add to condition variable wait list */
    spinlock_flags_t flags;
    spin_lock_irqsave(&cv->lock, &flags);
    list_add_tail(&waiter.list, &cv->waiters);
    spin_unlock_irqrestore(&cv->lock, flags);

    /* Release the mutex and sleep */
    mutex_unlock(m);
    sleep_on(&wq);

    /* Re-acquire the mutex after being woken */
    mutex_lock(m);

    /* Remove from wait list */
    spin_lock_irqsave(&cv->lock, &flags);
    list_del_init(&waiter.list);
    spin_unlock_irqrestore(&cv->lock, flags);
}

void condvar_signal(condvar_t *cv) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&cv->lock, &flags);

    if (!list_is_empty(&cv->waiters)) {
        struct condvar_waiter {
            list_head list;
            pcb_t* task;
            mutex_t* m;
            wait_queue_head_t* wq;
        };

        /* Wake up first waiter */
        list_head* first = cv->waiters.next;
        struct condvar_waiter* waiter = list_entry(first, struct condvar_waiter, list);

        /* Remove from wait list */
        list_del_init(first);

        /* Wake up the waiter by waking its wait queue */
        if (waiter->wq != NULL) {
            spin_unlock_irqrestore(&cv->lock, flags);
            wake_up(waiter->wq);
            return;
        }
    }

    spin_unlock_irqrestore(&cv->lock, flags);
}

void condvar_broadcast(condvar_t *cv) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&cv->lock, &flags);

    struct condvar_waiter {
        list_head list;
        pcb_t* task;
        mutex_t* m;
        wait_queue_head_t* wq;
    };

    /* Collect all wait queues to wake before releasing the lock */
    wait_queue_head_t *wq_list[32];
    int count = 0;

    list_head *pos, *next;
    list_for_each_safe(pos, next, &cv->waiters) {
        struct condvar_waiter* waiter = list_entry(pos, struct condvar_waiter, list);

        /* Remove from wait list */
        list_del_init(pos);

        /* Collect wait queue to wake later */
        if (waiter->wq != NULL && count < 32) {
            wq_list[count++] = waiter->wq;
        }
    }

    spin_unlock_irqrestore(&cv->lock, flags);

    /* Wake all waiters outside the lock */
    for (int i = 0; i < count; i++) {
        wake_up(wq_list[i]);
    }
}
