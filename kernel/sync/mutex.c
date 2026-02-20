/* ==============================================================================
 * CCOS - Mutex Implementation
 * ==============================================================================
 */

#include "sync/mutex.h"
#include "sync/waitqueue.h"
#include "process/process.h"
#include "process/sched.h"
#include "klogs/kprintf.h"

void mutex_init(mutex_t* m) {
    atomic_write(&m->locked, 0);
    m->owner = NULL;
    m->count = 0;
    spin_lock_init(&m->wait_lock);
    INIT_LIST_HEAD(&m->wait_list);
}

void mutex_lock(mutex_t* m) {
    pcb_t* current = proc_current();

    /* Fast path: try to acquire the lock atomically */
    if (atomic_compare_and_swap(&m->locked, 0, 1)) {
        m->owner = current;
        m->count = 1;
        return;
    }

    /* Slow path: lock is held, need to wait */
    spinlock_flags_t flags;
    spin_lock_irqsave(&m->wait_lock, &flags);

    /* Double-check after acquiring wait_lock */
    if (atomic_compare_and_swap(&m->locked, 0, 1)) {
        m->owner = current;
        m->count = 1;
        spin_unlock_irqrestore(&m->wait_lock, flags);
        return;
    }

    /* Add ourselves to wait list */
    /* Create a wait queue entry on stack */
    wait_queue_head_t wq;
    init_waitqueue_head(&wq);

    /* Add current process to mutex wait list */
    struct mutex_waiter {
        list_head list;
        pcb_t* task;
        wait_queue_head_t* wq;
    } waiter;

    waiter.task = current;
    waiter.wq = &wq;
    list_add_tail(&waiter.list, &m->wait_list);

    spin_unlock_irqrestore(&m->wait_lock, flags);

    /* Sleep until woken */
    while (atomic_read(&m->locked) != 0 || m->owner != NULL) {
        /* Sleep on our private wait queue */
        sleep_on(&wq);
    }

    /* Acquire the lock */
    atomic_write(&m->locked, 1);
    m->owner = current;
    m->count = 1;

    /* Remove from wait list */
    spin_lock_irqsave(&m->wait_lock, &flags);
    list_del_init(&waiter.list);
    spin_unlock_irqrestore(&m->wait_lock, flags);
}

bool mutex_trylock(mutex_t* m) {
    pcb_t* current = proc_current();

    if (!atomic_compare_and_swap(&m->locked, 0, 1)) {
        return false;
    }

    m->owner = current;
    m->count = 1;
    return true;
}

void mutex_unlock(mutex_t* m) {
    pcb_t* current = proc_current();

    /* Verify we own the lock */
    if (m->owner != current) {
        /* Attempting to unlock a mutex we don't own */
        return;
    }

    m->owner = NULL;
    m->count = 0;
    atomic_write(&m->locked, 0);

    /* Wake up one waiting process */
    spinlock_flags_t flags;
    spin_lock_irqsave(&m->wait_lock, &flags);

    if (!list_is_empty(&m->wait_list)) {
        struct mutex_waiter {
            list_head list;
            pcb_t* task;
            wait_queue_head_t* wq;
        };

        list_head* first = m->wait_list.next;
        struct mutex_waiter* waiter = list_entry(first, struct mutex_waiter, list);

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

    spin_unlock_irqrestore(&m->wait_lock, flags);
}

bool mutex_is_locked(const mutex_t* m) {
    return atomic_read((atomic_t*)&m->locked) != 0;
}

pcb_t* mutex_owner(const mutex_t* m) {
    return m->owner;
}
