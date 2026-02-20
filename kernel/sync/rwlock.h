/* ==============================================================================
 * CCOS - Read-Write Lock
 * ==============================================================================
 * Provides read-write lock synchronization primitive.
 * Multiple readers can hold the lock simultaneously, but writers have exclusive access.
 * Implements reader-preference policy.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"
#include "sync/atomic.h"
#include "sync/spinlock.h"

/* ==============================================================================
 * Read-Write Lock Type
 * ==============================================================================
 */

/**
 * @brief Read-write lock structure
 */
typedef struct {
    atomic_t readers; /* Number of active readers */
    atomic_t writers; /* Number of active writers (0 or 1) */
    spinlock_t lock;  /* Internal lock for state changes */
} rwlock_t;

/* ==============================================================================
 * Initialization Macros
 * ==============================================================================
 */

#define RWLOCK_INIT {.readers = ATOMIC_INIT(0), .writers = ATOMIC_INIT(0), .lock = SPIN_LOCK_INIT}

#define RWLOCK_INITIALIZER ((rwlock_t)RWLOCK_INIT)

/* ==============================================================================
 * RWLock Operations
 * ==============================================================================
 */

void rwlock_init(rwlock_t* rw);
void rwlock_read_lock(rwlock_t* rw);
bool rwlock_read_trylock(rwlock_t* rw);
void rwlock_read_unlock(rwlock_t* rw);
void rwlock_write_lock(rwlock_t* rw);
bool rwlock_write_trylock(rwlock_t* rw);
void rwlock_write_unlock(rwlock_t* rw);
void rwlock_write_downgrade(rwlock_t* rw);

/* ==============================================================================
 * Debugging Helpers
 * ==============================================================================
 */

bool rwlock_is_read_locked(const rwlock_t* rw);
bool rwlock_is_write_locked(const rwlock_t* rw);
bool rwlock_is_locked(const rwlock_t* rw);
