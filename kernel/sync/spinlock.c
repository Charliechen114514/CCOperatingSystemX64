/* ==============================================================================
 * CCOS - Spinlock Implementation
 * ==============================================================================
 * Provides spinlock synchronization primitive for short critical sections.
 * A spinlock busy-waits in a loop until the lock is acquired.
 * ==============================================================================
 */

#include "sync/spinlock.h"
#include "assert/assert.h"
#include "interrupt/idt.h"
#include "klogs/kprintf.h"
/* ==============================================================================
 * Spinlock Operations
 * ==============================================================================
 */

void spin_lock_init(spinlock_t* lock) {
    lock->locked = 0;
}

void spin_lock(spinlock_t* lock) {
    /* Check for potential recursive lock deadlock */
    CCOS_IF_PANIC(spin_is_locked(lock));

    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause" ::: "memory");
    }
}

bool spin_trylock(spinlock_t* lock) {
    return !__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE);
}

void spin_unlock(spinlock_t* lock) {
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}

void spin_lock_irq(spinlock_t* lock) {
    interrupt_disable();
    spin_lock(lock);
}

void spin_unlock_irq(spinlock_t* lock) {
    spin_unlock(lock);
    interrupt_enable();
}

void spin_lock_irqsave(spinlock_t* lock, spinlock_flags_t* flags) {
    uint64_t rflags;
    __asm__ volatile("pushfq; popq %0" : "=r"(rflags));
    *flags = rflags;

    interrupt_disable();
    spin_lock(lock);
}

void spin_unlock_irqrestore(spinlock_t* lock, spinlock_flags_t flags) {
    spin_unlock(lock);

    if (flags & 0x200) {
        interrupt_enable();
    }
}

bool spin_is_locked(const spinlock_t* lock) {
    return lock->locked != 0;
}
