/* ==============================================================================
 * CCOS - Read-Write Lock Implementation
 * ==============================================================================
 */

#include "sync/rwlock.h"

void rwlock_init(rwlock_t *rw) {
    atomic_write(&rw->readers, 0);
    atomic_write(&rw->writers, 0);
    spin_lock_init(&rw->lock);
}

void rwlock_read_lock(rwlock_t *rw) {
    /* Spin until no writer is active */
    while (atomic_read(&rw->writers) > 0) {
        __asm__ volatile("pause" ::: "memory");
    }

    /* Increment reader count */
    spinlock_flags_t flags;
    spin_lock_irqsave(&rw->lock, &flags);
    atomic_inc(&rw->readers);
    spin_unlock_irqrestore(&rw->lock, flags);
}

bool rwlock_read_trylock(rwlock_t *rw) {
    if (atomic_read(&rw->writers) > 0) {
        return false;
    }

    spinlock_flags_t flags;
    spin_lock_irqsave(&rw->lock, &flags);
    if (atomic_read(&rw->writers) > 0) {
        spin_unlock_irqrestore(&rw->lock, flags);
        return false;
    }
    atomic_inc(&rw->readers);
    spin_unlock_irqrestore(&rw->lock, flags);
    return true;
}

void rwlock_read_unlock(rwlock_t *rw) {
    atomic_dec(&rw->readers);
}

void rwlock_write_lock(rwlock_t *rw) {
    spinlock_flags_t flags;

    /* First claim writer status */
    spin_lock_irqsave(&rw->lock, &flags);
    while (atomic_read(&rw->writers) > 0) {
        spin_unlock_irqrestore(&rw->lock, flags);
        __asm__ volatile("pause" ::: "memory");
        spin_lock_irqsave(&rw->lock, &flags);
    }
    atomic_inc(&rw->writers);
    spin_unlock_irqrestore(&rw->lock, flags);

    /* Now wait for all readers to finish */
    while (atomic_read(&rw->readers) > 0) {
        __asm__ volatile("pause" ::: "memory");
    }
}

bool rwlock_write_trylock(rwlock_t *rw) {
    spinlock_flags_t flags;

    spin_lock_irqsave(&rw->lock, &flags);

    /* Check if any writer or readers are active */
    if (atomic_read(&rw->writers) > 0 || atomic_read(&rw->readers) > 0) {
        spin_unlock_irqrestore(&rw->lock, flags);
        return false;
    }

    atomic_inc(&rw->writers);
    spin_unlock_irqrestore(&rw->lock, flags);
    return true;
}

void rwlock_write_unlock(rwlock_t *rw) {
    atomic_dec(&rw->writers);
}

void rwlock_write_downgrade(rwlock_t *rw) {
    atomic_inc(&rw->readers);
    atomic_dec(&rw->writers);
}

bool rwlock_is_read_locked(const rwlock_t *rw) {
    return atomic_read((atomic_t*)&rw->readers) > 0;
}

bool rwlock_is_write_locked(const rwlock_t *rw) {
    return atomic_read((atomic_t*)&rw->writers) > 0;
}

bool rwlock_is_locked(const rwlock_t *rw) {
    return rwlock_is_read_locked(rw) || rwlock_is_write_locked(rw);
}
