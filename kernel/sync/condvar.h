/* ==============================================================================
 * CCOS - Condition Variable
 * ==============================================================================
 * Provides condition variable synchronization primitive.
 * Condition variables allow threads to wait for a condition to become true.
 * Must be used with a mutex to protect the shared condition.
 * ==============================================================================
 */

#pragma once
#include "assert/assert.h"
#include "defines/types.h"
#include "list/list.h"
#include "sync/spinlock.h"

/* Forward declaration - incomplete type */
typedef struct mutex mutex_t;

/* ==============================================================================
 * Condition Variable Type
 * ==============================================================================
 */

/**
 * @brief Condition variable structure
 */
typedef struct {
    spinlock_t lock;   /* Protects waiters */
    list_head waiters; /* List of waiting processes */
} condvar_t;

/* ==============================================================================
 * Initialization Macros
 * ==============================================================================
 */

#define CONDVAR_INIT {.lock = SPIN_LOCK_INIT, .waiters = LIST_HEAD_INIT(waiters)}

#define CONDVAR_INITIALIZER ((condvar_t)CONDVAR_INIT)

/* ==============================================================================
 * Condition Variable Operations
 * ==============================================================================
 */

void condvar_init(condvar_t* cv);
void condvar_wait(condvar_t* cv, mutex_t* m);
void condvar_signal(condvar_t* cv);
void condvar_broadcast(condvar_t* cv);

#define CCOS_ASRT_NO_WAITER(cv) CCOS_ASSERT(!list_is_empty(&(cv)->waiters))
