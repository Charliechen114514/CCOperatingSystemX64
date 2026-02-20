/* ==============================================================================
 * CCOS - Wait Queue
 * ==============================================================================
 * Provides wait queue synchronization primitive for process blocking/waking.
 * Wait queues allow processes to sleep waiting for an event and be woken up
 * when that event occurs.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"
#include "list/list.h"
#include "process/process.h"
#include "sync/spinlock.h"

/* ==============================================================================
 * Wait Queue Entry
 * ==============================================================================
 */

/**
 * @brief Wait queue entry - represents a single waiting process
 */
typedef struct wait_queue_entry {
    list_head list;            /* List node for wait queue */
    pcb_t* task;               /* Waiting process */
    process_state_t old_state; /* Process state before blocking */
    bool exclusive;            /* Exclusive wait (only one waker needed) */
} wait_queue_entry_t;

/* ==============================================================================
 * Wait Queue Head
 * ==============================================================================
 */

/**
 * @brief Wait queue head - manages a queue of waiting processes
 */
typedef struct {
    list_head task_list; /* List of wait_queue_entry_t */
    spinlock_t lock;     /* Protects the wait queue */
} wait_queue_head_t;

/* ==============================================================================
 * Initialization Macros
 * ==============================================================================
 */

/**
 * @brief Static initializer for wait_queue_head_t
 */
#define WAIT_QUEUE_HEAD_INIT {.task_list = LIST_HEAD_INIT(task_list), .lock = SPIN_LOCK_INIT}

/**
 * @brief Wait queue head initializer at runtime
 */
#define WAIT_QUEUE_HEAD_INITIALIZER ((wait_queue_head_t)WAIT_QUEUE_HEAD_INIT)

/**
 * @brief Declare and initialize a static wait queue head
 */
#define DECLARE_WAIT_QUEUE_HEAD(name) wait_queue_head_t name = WAIT_QUEUE_HEAD_INIT

/* ==============================================================================
 * Wait Queue Operations
 * ==============================================================================
 */

/**
 * @brief Initialize a wait queue head
 * @param wq Pointer to wait_queue_head_t
 */
static inline void init_waitqueue_head(wait_queue_head_t* wq) {
    INIT_LIST_HEAD(&wq->task_list);
    spin_lock_init(&wq->lock);
}

/**
 * @brief Initialize a wait queue entry
 * @param wq_entry Pointer to wait_queue_entry_t
 * @param exclusive Whether this is an exclusive wait
 */
static inline void init_waitqueue_entry(wait_queue_entry_t* wq_entry, bool exclusive) {
    INIT_LIST_HEAD(&wq_entry->list);
    wq_entry->task = proc_current();
    wq_entry->old_state = PROC_READY; /* Will be set when actually blocking */
    wq_entry->exclusive = exclusive;
}

/* ==============================================================================
 * Sleep Operations
 * ==============================================================================
 */

/**
 * @brief Add current process to wait queue and block
 * @param wq Pointer to wait_queue_head_t
 * @param exclusive Whether this is an exclusive wait
 *
 * Adds the current process to the wait queue and blocks until woken.
 * This is a simplified implementation - proper implementation would
 * require more sophisticated scheduler integration.
 */
void sleep_on(wait_queue_head_t* wq);

/**
 * @brief Interruptible sleep on a wait queue
 * @param wq Pointer to wait_queue_head_t
 * @param exclusive Whether this is an exclusive wait
 * @return 0 if woken normally, negative if interrupted
 *
 * Similar to sleep_on but can be interrupted by signals (when implemented).
 */
int interruptible_sleep_on(wait_queue_head_t* wq);

/**
 * @brief Sleep on a wait queue with a condition check
 * @param wq Pointer to wait_queue_head_t
 * @param condition Boolean condition to check
 *
 * Will sleep until the condition is true. Uses pattern:
 *   wait_event(wq, condition);
 */
#define wait_event(wq, condition) \
    do {                          \
        while (!(condition)) {    \
            sleep_on(&(wq));      \
        }                         \
    } while (0)

/**
 * @brief Interruptible sleep with condition check
 * @param wq Pointer to wait_queue_head_t
 * @param condition Boolean condition to check
 * @return 0 if condition became true, negative if interrupted
 */
#define wait_event_interruptible(wq, condition)    \
    ({                                             \
        int __ret = 0;                             \
        while (!(condition)) {                     \
            __ret = interruptible_sleep_on(&(wq)); \
            if (__ret != 0) {                      \
                break;                             \
            }                                      \
        }                                          \
        __ret;                                     \
    })

/* ==============================================================================
 * Wake Operations
 * ==============================================================================
 */

/**
 * @brief Wake up one process waiting on the queue
 * @param wq Pointer to wait_queue_head_t
 *
 * Wakes up one waiting process. Prefers exclusive waiters if any.
 */
void wake_up(wait_queue_head_t* wq);

/**
 * @brief Wake up all processes waiting on the queue
 * @param wq Pointer to wait_queue_head_t
 *
 * Wakes up all waiting processes.
 */
void wake_up_all(wait_queue_head_t* wq);

/**
 * @brief Wake up exclusive waiters only
 * @param wq Pointer to wait_queue_head_t
 *
 * Only wakes up processes that registered as exclusive waiters.
 */
void wake_up_exclusive(wait_queue_head_t* wq);

/* ==============================================================================
 * Wait Queue Entry Management
 * ==============================================================================
 */

/**
 * @brief Add a wait queue entry to a wait queue
 * @param wq Pointer to wait_queue_head_t
 * @param wq_entry Pointer to wait_queue_entry_t
 */
static inline void add_wait_queue(wait_queue_head_t* wq, wait_queue_entry_t* wq_entry) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&wq->lock, &flags);
    list_add_tail(&wq_entry->list, &wq->task_list);
    spin_unlock_irqrestore(&wq->lock, flags);
}

/**
 * @brief Remove a wait queue entry from a wait queue
 * @param wq Pointer to wait_queue_head_t
 * @param wq_entry Pointer to wait_queue_entry_t
 */
static inline void remove_wait_queue(wait_queue_head_t* wq, wait_queue_entry_t* wq_entry) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&wq->lock, &flags);
    list_del_init(&wq_entry->list);
    spin_unlock_irqrestore(&wq->lock, flags);
}

/**
 * @brief Check if wait queue is empty
 * @param wq Pointer to wait_queue_head_t
 * @return true if empty, false otherwise
 */
static inline bool waitqueue_active(const wait_queue_head_t* wq) {
    return !list_is_empty(&wq->task_list);
}
