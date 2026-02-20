/* ==============================================================================
 * CCOS - Semaphore
 * ==============================================================================
 * Provides semaphore synchronization primitive for resource counting.
 * A semaphore maintains a counter that can be incremented (post) or decremented (wait).
 * When the counter is zero, wait operations will block.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"
#include "list/list.h"
#include "process/process.h"
#include "sync/atomic.h"
#include "sync/spinlock.h"

/* ==============================================================================
 * Semaphore Type
 * ==============================================================================
 */

/**
 * @brief Semaphore structure
 */
typedef struct {
    atomic_t count;      /* Resource counter */
    spinlock_t lock;     /* Protects state */
    list_head wait_list; /* List of waiting processes */
} semaphore_t;

/* ==============================================================================
 * Initialization Macros
 * ==============================================================================
 */

#define SEMAPHORE_INIT(val) \
    {.count = ATOMIC_INIT(val), .lock = SPIN_LOCK_INIT, .wait_list = LIST_HEAD_INIT(wait_list)}

#define SEMAPHORE_INITIALIZER(val) ((semaphore_t)SEMAPHORE_INIT(val))

/* ==============================================================================
 * Semaphore Operations
 * ==============================================================================
 */

void sem_init(semaphore_t* s, int value);
void sem_wait(semaphore_t* s);
bool sem_trywait(semaphore_t* s);
void sem_post(semaphore_t* s);
int sem_getvalue(semaphore_t* s);

/* ==============================================================================
 * Convenience Macros
 * ==============================================================================
 */

#define sem_init_binary(s) sem_init((s), 1)
#define sem_init_counting(s, count) sem_init((s), (count))
