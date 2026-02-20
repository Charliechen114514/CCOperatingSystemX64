/* ==============================================================================
 * CCOS - Mutex (Mutual Exclusion Lock)
 * ==============================================================================
 * Provides mutex synchronization primitive for protecting critical sections.
 * Unlike spinlocks, mutexes can cause the calling thread to sleep while waiting.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"
#include "list/list.h"
#include "process/process.h"
#include "sync/atomic.h"
#include "sync/spinlock.h"

/* ==============================================================================
 * Mutex Type
 * ==============================================================================
 */

/**
 * @brief Mutex structure
 */
typedef struct mutex {
    atomic_t locked;      /* Lock state: 0 = unlocked, 1 = locked */
    pcb_t* owner;         /* Current owner process */
    uint32_t count;       /* Recursion count */
    spinlock_t wait_lock; /* Protects wait_list */
    list_head wait_list;  /* List of waiting processes */
} mutex_t;

/* ==============================================================================
 * Initialization Macros
 * ==============================================================================
 */

#define MUTEX_INIT                \
    {.locked = ATOMIC_INIT(0),    \
     .owner = NULL,               \
     .count = 0,                  \
     .wait_lock = SPIN_LOCK_INIT, \
     .wait_list = LIST_HEAD_INIT(wait_list)}

#define MUTEX_INITIALIZER ((mutex_t)MUTEX_INIT)

/* ==============================================================================
 * Mutex Operations
 * ==============================================================================
 */

void mutex_init(mutex_t* m);
void mutex_lock(mutex_t* m);
bool mutex_trylock(mutex_t* m);
void mutex_unlock(mutex_t* m);

/* ==============================================================================
 * Extended Operations
 * ==============================================================================
 */

bool mutex_is_locked(const mutex_t* m);
pcb_t* mutex_owner(const mutex_t* m);
