/* ==============================================================================
 * CCOS - Spinlock
 * ==============================================================================
 * Provides spinlock synchronization primitive for short critical sections.
 * A spinlock busy-waits in a loop until the lock is acquired.
 * ==============================================================================
 */

#pragma once
#include "defines/types.h"

/* ==============================================================================
 * Spinlock Type
 * ==============================================================================
 */

/**
 * @brief Spinlock structure
 *
 * Uses an atomic integer to track lock state:
 * - 0 = unlocked
 * - 1 = locked
 */
typedef struct {
    volatile int locked;
} spinlock_t;

/* ==============================================================================
 * Flags type for saving interrupt state
 * ==============================================================================
 */
typedef uint64_t spinlock_flags_t;

/* ==============================================================================
 * Initialization Macros
 * ==============================================================================
 */

/**
 * @brief Static initializer for spinlock_t
 */
#define SPIN_LOCK_INIT {.locked = 0}

/**
 * @brief Spinlock initializer at runtime
 */
#define SPIN_LOCK_INITIALIZER ((spinlock_t)SPIN_LOCK_INIT)

/* ==============================================================================
 * Spinlock Operations
 * ==============================================================================
 */

/**
 * @brief Initialize a spinlock
 * @param lock Pointer to spinlock_t
 */
void spin_lock_init(spinlock_t* lock);

/**
 * @brief Acquire a spinlock (busy-wait until acquired)
 * @param lock Pointer to spinlock_t
 */
void spin_lock(spinlock_t* lock);

/**
 * @brief Try to acquire a spinlock without blocking
 * @param lock Pointer to spinlock_t
 * @return true if lock was acquired, false otherwise
 */
bool spin_trylock(spinlock_t* lock);

/**
 * @brief Release a spinlock
 * @param lock Pointer to spinlock_t
 */
void spin_unlock(spinlock_t* lock);

/**
 * @brief Acquire a spinlock with interrupts disabled
 * @param lock Pointer to spinlock_t
 */
void spin_lock_irq(spinlock_t* lock);

/**
 * @brief Release a spinlock and re-enable interrupts
 * @param lock Pointer to spinlock_t
 */
void spin_unlock_irq(spinlock_t* lock);

/**
 * @brief Acquire a spinlock with interrupt state saving
 * @param lock Pointer to spinlock_t
 * @param flags Pointer to store interrupt flags
 */
void spin_lock_irqsave(spinlock_t* lock, spinlock_flags_t* flags);

/**
 * @brief Release a spinlock and restore interrupt state
 * @param lock Pointer to spinlock_t
 * @param flags Saved interrupt flags from spin_lock_irqsave()
 */
void spin_unlock_irqrestore(spinlock_t* lock, spinlock_flags_t flags);

/**
 * @brief Check if a spinlock is currently locked
 * @param lock Pointer to spinlock_t
 * @return true if locked, false otherwise
 */
bool spin_is_locked(const spinlock_t* lock);

/* ==============================================================================
 * Raw Spinlock Operations (No preempt_count modification)
 * ==============================================================================
 * These are "raw" spinlocks that DON'T modify preempt_count.
 * They should ONLY be used in special cases where:
 * 1. The lock is taken in interrupt context with no current process
 * 2. The lock is used by logging/debugging infrastructure that runs before
 *    the scheduler is fully initialized
 * 3. You absolutely know what you're doing!
 * ==============================================================================
 */

/**
 * @brief Raw spin lock - doesn't call preempt_disable()
 * @param lock Pointer to spinlock_t
 */
static inline void spin_lock_raw(spinlock_t* lock) {
    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause" ::: "memory");
    }
}

/**
 * @brief Raw spin unlock - doesn't call preempt_enable()
 * @param lock Pointer to spinlock_t
 */
static inline void spin_unlock_raw(spinlock_t* lock) {
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}

/**
 * @brief Raw spin lock with IRQ save - doesn't modify preempt_count
 * @param lock Pointer to spinlock_t
 * @param flags Pointer to store interrupt flags
 */
static inline void spin_lock_irqsave_raw(spinlock_t* lock, spinlock_flags_t* flags) {
    __asm__ volatile("pushfq; popq %0" : "=r"(*flags));
    __asm__ volatile("cli" ::: "memory");
    spin_lock_raw(lock);
}

/**
 * @brief Raw spin unlock with IRQ restore - doesn't modify preempt_count
 * @param lock Pointer to spinlock_t
 * @param flags Saved interrupt flags
 */
static inline void spin_unlock_irqrestore_raw(spinlock_t* lock, spinlock_flags_t flags) {
    spin_unlock_raw(lock);
    if (flags & 0x200) {
        __asm__ volatile("sti" ::: "memory");
    }
}
