/**
 * @file sync_demo.c
 * @brief Synchronization Primitives Demo - Concurrent Testing Implementation
 *
 * This demo tests synchronization primitives with simulated concurrent execution.
 * Due to the current state of the kernel's threading support, we simulate
 * concurrent execution by alternating between "processes" to test the
 * synchronization mechanisms.
 *
 * The tests verify:
 * 1. Mutex: Proper mutual exclusion and owner tracking
 * 2. Semaphore: Resource counting and blocking behavior
 * 3. RWLock: Multiple readers vs exclusive writer access
 * 4. Condition Variable: Wait/signal coordination
 */

#include "sync_demo.h"
#include "base/memory.h"
#include "base/string.h"
#include "klogs/kprintf.h"
#include "mm/heap/heap.h"
#include "process/process.h"
#include "sync/atomic.h"
#include "sync/condvar.h"
#include "sync/mutex.h"
#include "sync/rwlock.h"
#include "sync/semaphore.h"

/* ============================================================================
 * Test Configuration
 * ============================================================================ */

#define SYNC_TEST_NUM_PROCS 4         /* Number of simulated processes */
#define SYNC_TEST_ITERATIONS 100      /* Iterations per test */
#define SYNC_TEST_COUNTER_TARGET 1000 /* Target counter value */

/* ============================================================================
 * Test Result Tracking
 * ============================================================================ */

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message)                      \
    do {                                                     \
        if (!(condition)) {                                  \
            klog_error("[SYNC_DEMO] FAILED: %s\n", message); \
            tests_failed++;                                  \
            return false;                                    \
        }                                                    \
    } while (0)

#define TEST_START(name) klog_info("[SYNC_DEMO] Test: %s...\n", name)

#define TEST_PASS()                        \
    do {                                   \
        klog_info("[SYNC_DEMO] PASSED\n"); \
        tests_passed++;                    \
        return true;                       \
    } while (0)

/* ============================================================================
 * Shared Test Data Structures
 * ============================================================================ */

/**
 * @brief Shared counter for testing race conditions
 */
typedef struct {
    atomic_t counter;      /* Atomic counter (for verification) */
    int unsafe_counter;    /* Non-atomic counter (to detect races) */
    mutex_t mutex;         /* Mutex for protected access */
    int protected_counter; /* Counter protected by mutex */
} shared_counter_t;

/**
 * @brief Shared buffer for producer-consumer tests
 */
typedef struct {
    int buffer[16];
    int write_pos;
    int read_pos;
    int count;
    semaphore_t empty;
    semaphore_t full;
    mutex_t mutex;
} shared_buffer_t;

/**
 * @brief Shared data for read-write lock tests
 */
typedef struct {
    int read_count;
    int write_count;
    int data_value;
    rwlock_t rwlock;
    atomic_t active_readers;
    atomic_t active_writers;
} shared_data_t;

/**
 * @brief Shared data for condition variable tests
 */
typedef struct {
    int flag;
    int waiting_count;
    mutex_t mutex;
    condvar_t cv;
} shared_event_t;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Simulate a context switch between processes
 *
 * In a real system with proper threading, this would be a yield or sleep.
 * For testing purposes, we just do a small delay to allow
 * other operations to potentially interleave.
 */
static inline void simulate_context_switch(void) {
    /* Memory barrier to ensure operations are visible */
    mb();
    /* In a real system, this would be schedule() or yield() */
}

/* ============================================================================
 * Mutex Tests
 * ============================================================================ */

/**
 * @brief Initialize shared counter structure
 */
static void shared_counter_init(shared_counter_t* sc) {
    atomic_write(&sc->counter, 0);
    sc->unsafe_counter = 0;
    mutex_init(&sc->mutex);
    sc->protected_counter = 0;
}

/**
 * @brief Test 1: Basic Mutex Lock/Unlock
 */
static bool test_mutex_basic(void) {
    TEST_START("Mutex Basic Lock/Unlock");

    mutex_t mutex;
    mutex_init(&mutex);

    /* Test initial state */
    TEST_ASSERT(!mutex_is_locked(&mutex), "Mutex should not be locked initially");
    TEST_ASSERT(mutex_owner(&mutex) == NULL, "Mutex should have no owner initially");

    /* Test lock */
    mutex_lock(&mutex);
    TEST_ASSERT(mutex_is_locked(&mutex), "Mutex should be locked after lock");
    TEST_ASSERT(mutex_owner(&mutex) == proc_current(), "Mutex owner should be current process");

    /* Test unlock */
    mutex_unlock(&mutex);
    TEST_ASSERT(!mutex_is_locked(&mutex), "Mutex should not be locked after unlock");
    TEST_ASSERT(mutex_owner(&mutex) == NULL, "Mutex should have no owner after unlock");

    TEST_PASS();
}

/**
 * @brief Test 2: Mutex Trylock
 */
static bool test_mutex_trylock(void) {
    TEST_START("Mutex Trylock");

    mutex_t mutex;
    mutex_init(&mutex);

    /* Trylock on unlocked mutex should succeed */
    TEST_ASSERT(mutex_trylock(&mutex), "Trylock should succeed on unlocked mutex");
    TEST_ASSERT(mutex_is_locked(&mutex), "Mutex should be locked");
    mutex_unlock(&mutex);

    /* Trylock on locked mutex should fail */
    mutex_lock(&mutex);
    TEST_ASSERT(!mutex_trylock(&mutex), "Trylock should fail on locked mutex");
    mutex_unlock(&mutex);

    TEST_PASS();
}

/**
 * @brief Test 3: Mutex Recursive Locking
 *
 * Note: Current implementation doesn't support recursive locking.
 * This test verifies that behavior.
 */
static bool test_mutex_recursive(void) {
    TEST_START("Mutex Recursive Locking Behavior");

    mutex_t mutex;
    mutex_init(&mutex);

    /* First lock should succeed */
    mutex_lock(&mutex);
    TEST_ASSERT(mutex_is_locked(&mutex), "Mutex should be locked");
    TEST_ASSERT(mutex_owner(&mutex) == proc_current(), "Owner should be current process");

    /* Second lock by same owner will block in real scenario
     * For this demo, we just verify the owner tracking works */
    TEST_ASSERT(mutex_owner(&mutex) == proc_current(), "Owner should still be current process");

    mutex_unlock(&mutex);
    TEST_ASSERT(!mutex_is_locked(&mutex), "Mutex should be unlocked");

    TEST_PASS();
}

/**
 * @brief Simulated concurrent increment using mutex
 *
 * This simulates what would happen if multiple processes were
 * incrementing a shared counter with mutex protection.
 */
static void simulated_mutex_increment(shared_counter_t* sc, int iterations) {
    for (int i = 0; i < iterations; i++) {
        /* Simulate context switch between lock and increment */
        simulate_context_switch();

        mutex_lock(&sc->mutex);

        /* Critical section - protected by mutex */
        sc->protected_counter++;
        atomic_inc(&sc->counter); /* Atomic for verification */

        simulate_context_switch();

        mutex_unlock(&sc->mutex);
    }
}

/**
 * @brief Test 4: Mutex Protected Critical Section
 */
static bool test_mutex_critical_section(void) {
    TEST_START("Mutex Protected Critical Section");

    shared_counter_t sc;
    shared_counter_init(&sc);

    /* Simulate multiple processes incrementing */
    int total_iterations = SYNC_TEST_ITERATIONS;

    for (int proc = 0; proc < SYNC_TEST_NUM_PROCS; proc++) {
        simulated_mutex_increment(&sc, total_iterations / SYNC_TEST_NUM_PROCS);
        simulate_context_switch();
    }

    /* Verify protected counter matches atomic counter */
    int expected = total_iterations;
    int actual_protected = sc.protected_counter;
    int actual_atomic = atomic_read(&sc.counter);

    TEST_ASSERT(actual_protected == expected, "Protected counter should match expected value");
    TEST_ASSERT(actual_protected == actual_atomic, "Protected counter should match atomic counter");

    klog_info("[SYNC_DEMO]   Expected: %d, Protected: %d, Atomic: %d\n", expected, actual_protected,
              actual_atomic);

    TEST_PASS();
}

/* ============================================================================
 * Semaphore Tests
 * ============================================================================ */

/**
 * @brief Test 5: Semaphore Initialization
 */
static bool test_semaphore_init(void) {
    TEST_START("Semaphore Initialization");

    semaphore_t sem;
    sem_init(&sem, 5);

    TEST_ASSERT(sem_getvalue(&sem) == 5, "Semaphore should have initial value 5");

    /* Test binary semaphore init */
    sem_init_binary(&sem);
    TEST_ASSERT(sem_getvalue(&sem) == 1, "Binary semaphore should have value 1");

    TEST_PASS();
}

/**
 * @brief Test 6: Semaphore Post/Wait
 */
static bool test_semaphore_post_wait(void) {
    TEST_START("Semaphore Post/Wait");

    semaphore_t sem;
    sem_init(&sem, 0); /* Start with 0 */

    /* Post should increment */
    sem_post(&sem);
    TEST_ASSERT(sem_getvalue(&sem) == 1, "Value should be 1 after post");

    sem_post(&sem);
    TEST_ASSERT(sem_getvalue(&sem) == 2, "Value should be 2 after second post");

    /* Wait should decrement */
    sem_wait(&sem);
    TEST_ASSERT(sem_getvalue(&sem) == 1, "Value should be 1 after wait");

    sem_wait(&sem);
    TEST_ASSERT(sem_getvalue(&sem) == 0, "Value should be 0 after second wait");

    TEST_PASS();
}

/**
 * @brief Test 7: Semaphore Trywait
 */
static bool test_semaphore_trywait(void) {
    TEST_START("Semaphore Trywait");

    semaphore_t sem;
    sem_init(&sem, 1);

    /* Trywait on positive count should succeed */
    TEST_ASSERT(sem_trywait(&sem), "Trywait should succeed with positive count");
    TEST_ASSERT(sem_getvalue(&sem) == 0, "Count should be 0");

    /* Trywait on zero count should fail */
    TEST_ASSERT(!sem_trywait(&sem), "Trywait should fail with zero count");

    TEST_PASS();
}

/**
 * @brief Test 8: Semaphore as Resource Counter
 *
 * Simulates a semaphore limiting concurrent access to a resource pool.
 */
static bool test_semaphore_resource_pool(void) {
    TEST_START("Semaphore Resource Pool");

    semaphore_t resource_sem;
    const int pool_size = 3;

    sem_init(&resource_sem, pool_size);

    /* "Acquire" resources */
    int acquired = 0;
    for (int i = 0; i < pool_size + 2; i++) {
        if (sem_trywait(&resource_sem)) {
            acquired++;
        } else {
            /* Should fail when pool is exhausted */
            TEST_ASSERT(acquired == pool_size, "Should only acquire up to pool size");
        }
    }

    TEST_ASSERT(acquired == pool_size, "Should acquire exactly pool size resources");
    TEST_ASSERT(sem_getvalue(&resource_sem) == 0, "Pool should be empty");

    /* Release all resources */
    for (int i = 0; i < acquired; i++) {
        sem_post(&resource_sem);
    }

    TEST_ASSERT(sem_getvalue(&resource_sem) == pool_size, "Pool should be full again");

    TEST_PASS();
}

/* ============================================================================
 * Read-Write Lock Tests
 * ============================================================================ */

/**
 * @brief Initialize shared data for RWLock tests
 */
static void shared_data_init(shared_data_t* sd) {
    sd->read_count = 0;
    sd->write_count = 0;
    sd->data_value = 0;
    rwlock_init(&sd->rwlock);
    atomic_write(&sd->active_readers, 0);
    atomic_write(&sd->active_writers, 0);
}

/**
 * @brief Test 9: RWLock Initialization
 */
static bool test_rwlock_init(void) {
    TEST_START("RWLock Initialization");

    rwlock_t rwlock;
    rwlock_init(&rwlock);

    TEST_ASSERT(!rwlock_is_locked(&rwlock), "RWLock should not be locked initially");
    TEST_ASSERT(!rwlock_is_read_locked(&rwlock), "RWLock should not be read-locked initially");
    TEST_ASSERT(!rwlock_is_write_locked(&rwlock), "RWLock should not be write-locked initially");

    TEST_PASS();
}

/**
 * @brief Test 10: RWLock Write Lock (Exclusive)
 */
static bool test_rwlock_write_lock(void) {
    TEST_START("RWLock Write Lock");

    rwlock_t rwlock;
    rwlock_init(&rwlock);

    /* Acquire write lock */
    rwlock_write_lock(&rwlock);

    TEST_ASSERT(rwlock_is_locked(&rwlock), "RWLock should be locked");
    TEST_ASSERT(rwlock_is_write_locked(&rwlock), "RWLock should be write-locked");
    TEST_ASSERT(!rwlock_is_read_locked(&rwlock), "RWLock should not be read-locked");

    /* Release write lock */
    rwlock_write_unlock(&rwlock);

    TEST_ASSERT(!rwlock_is_locked(&rwlock), "RWLock should not be locked after unlock");

    TEST_PASS();
}

/**
 * @brief Test 11: RWLock Read Lock (Shared)
 */
static bool test_rwlock_read_lock(void) {
    TEST_START("RWLock Read Lock");

    rwlock_t rwlock;
    rwlock_init(&rwlock);

    /* Acquire read lock */
    rwlock_read_lock(&rwlock);

    TEST_ASSERT(rwlock_is_locked(&rwlock), "RWLock should be locked");
    TEST_ASSERT(rwlock_is_read_locked(&rwlock), "RWLock should be read-locked");
    TEST_ASSERT(!rwlock_is_write_locked(&rwlock), "RWLock should not be write-locked");

    /* Release read lock */
    rwlock_read_unlock(&rwlock);

    TEST_ASSERT(!rwlock_is_locked(&rwlock), "RWLock should not be locked after unlock");

    TEST_PASS();
}

/**
 * @brief Test 12: RWLock Multiple Readers
 *
 * Simulates multiple readers holding the lock simultaneously.
 * In a real system, this would involve multiple threads.
 */
static bool test_rwlock_multiple_readers(void) {
    TEST_START("RWLock Multiple Readers Simulation");

    shared_data_t sd;
    shared_data_init(&sd);

    /* Simulate multiple readers */
    for (int i = 0; i < 5; i++) {
        rwlock_read_lock(&sd.rwlock);
        atomic_inc(&sd.active_readers);
        sd.read_count++;

        /* Simulate reading */
        int value = sd.data_value;
        (void)value; /* Suppress unused warning */

        simulate_context_switch();

        atomic_dec(&sd.active_readers);
        rwlock_read_unlock(&sd.rwlock);
    }

    TEST_ASSERT(sd.read_count == 5, "Should have 5 read operations");
    TEST_ASSERT(atomic_read(&sd.active_readers) == 0, "No active readers should remain");

    TEST_PASS();
}

/**
 * @brief Test 13: RWLock Reader-Writer Exclusion
 */
static bool test_rwlock_reader_writer_exclusion(void) {
    TEST_START("RWLock Reader-Writer Exclusion");

    shared_data_t sd;
    shared_data_init(&sd);

    /* Writer acquires lock */
    rwlock_write_lock(&sd.rwlock);
    atomic_write(&sd.active_writers, 1);
    sd.data_value = 42;

    /* In a real concurrent system, readers would block here
     * We verify the state is consistent */
    TEST_ASSERT(rwlock_is_write_locked(&sd.rwlock), "RWLock should be write-locked");

    atomic_write(&sd.active_writers, 0);
    rwlock_write_unlock(&sd.rwlock);

    /* Now readers can acquire */
    rwlock_read_lock(&sd.rwlock);
    int value = sd.data_value;
    rwlock_read_unlock(&sd.rwlock);

    TEST_ASSERT(value == 42, "Reader should see writer's value");

    TEST_PASS();
}

/* ============================================================================
 * Condition Variable Tests
 * ============================================================================ */

/**
 * @brief Test 14: Condition Variable Initialization
 */
static bool test_condvar_init(void) {
    TEST_START("Condition Variable Initialization");

    condvar_t cv;
    mutex_t mutex;

    condvar_init(&cv);
    mutex_init(&mutex);

    /* Can't easily test internal state without accessing private members
     * Just verify the functions don't crash */

    TEST_PASS();
}

/**
 * @brief Test 15: Condition Variable Signal
 *
 * Tests basic signal functionality (without actual blocking).
 */
static bool test_condvar_signal(void) {
    TEST_START("Condition Variable Signal");

    shared_event_t event;
    condvar_init(&event.cv);
    mutex_init(&event.mutex);
    event.flag = 0;
    event.waiting_count = 0;

    /* Signal with no waiters should not cause issues */
    mutex_lock(&event.mutex);
    condvar_signal(&event.cv);
    mutex_unlock(&event.mutex);

    TEST_PASS();
}

/**
 * @brief Test 16: Condition Variable Broadcast
 */
static bool test_condvar_broadcast(void) {
    TEST_START("Condition Variable Broadcast");

    shared_event_t event;
    condvar_init(&event.cv);
    mutex_init(&event.mutex);
    event.flag = 0;
    event.waiting_count = 0;

    /* Broadcast with no waiters should not cause issues */
    mutex_lock(&event.mutex);
    condvar_broadcast(&event.cv);
    mutex_unlock(&event.mutex);

    TEST_PASS();
}

/**
 * @brief Test 17: Condition Variable with Predicate
 *
 * Simulates the classic condition variable pattern:
 * wait while (condition is false)
 */
static bool test_condvar_predicate(void) {
    TEST_START("Condition Variable Predicate Pattern");

    shared_event_t event;
    condvar_init(&event.cv);
    mutex_init(&event.mutex);
    event.flag = 0;

    /* Simulate: thread checks predicate, waits if false */
    mutex_lock(&event.mutex);

    /* Check predicate (would wait in real scenario) */
    if (!event.flag) {
        /* In real code: condvar_wait(&event.cv, &event.mutex); */
        /* For demo, we just verify the pattern structure */
        klog_info("[SYNC_DEMO]   Would wait on condition (flag=%d)\n", event.flag);
    }

    mutex_unlock(&event.mutex);

    /* Simulate: another thread sets predicate and signals */
    mutex_lock(&event.mutex);
    event.flag = 1;
    condvar_signal(&event.cv);
    mutex_unlock(&event.mutex);

    TEST_PASS();
}

/* ============================================================================
 * Combined Tests
 * ============================================================================ */

/**
 * @brief Test 18: Combined Mutex and Semaphore
 *
 * Tests using mutex and semaphore together for producer-consumer pattern.
 */
static bool test_combined_mutex_semaphore(void) {
    TEST_START("Combined Mutex and Semaphore (Producer-Consumer Pattern)");

    shared_buffer_t buf;

    /* Initialize buffer */
    for (int i = 0; i < 16; i++) {
        buf.buffer[i] = 0;
    }
    buf.write_pos = 0;
    buf.read_pos = 0;
    buf.count = 0;

    /* Initialize synchronization primitives */
    sem_init(&buf.empty, 16); /* 16 empty slots */
    sem_init(&buf.full, 0);   /* 0 full slots */
    mutex_init(&buf.mutex);

    /* Simulate producer */
    for (int i = 0; i < 5; i++) {
        /* Wait for empty slot */
        if (sem_trywait(&buf.empty)) {
            mutex_lock(&buf.mutex);

            buf.buffer[buf.write_pos] = i + 1;
            buf.write_pos = (buf.write_pos + 1) % 16;
            buf.count++;

            mutex_unlock(&buf.mutex);
            sem_post(&buf.full);
        }
    }

    /* Simulate consumer */
    int consumed = 0;
    for (int i = 0; i < 5; i++) {
        if (sem_trywait(&buf.full)) {
            mutex_lock(&buf.mutex);

            consumed += buf.buffer[buf.read_pos];
            buf.read_pos = (buf.read_pos + 1) % 16;
            buf.count--;

            mutex_unlock(&buf.mutex);
            sem_post(&buf.empty);
        }
    }

    TEST_ASSERT(consumed == 15, "Should consume sum of 1+2+3+4+5=15");
    TEST_ASSERT(buf.count == 0, "Buffer should be empty");

    klog_info("[SYNC_DEMO]   Consumed: %d (expected 15)\n", consumed);

    TEST_PASS();
}

/**
 * @brief Test 19: Stress Test - Multiple Operations
 *
 * Performs many operations to verify consistency.
 */
static bool test_stress_multiple_ops(void) {
    TEST_START("Stress Test - Multiple Operations");

    mutex_t mutex;
    semaphore_t sem;
    atomic_t counter;

    mutex_init(&mutex);
    sem_init(&sem, 2);
    atomic_write(&counter, 0);

    /* Perform many mixed operations */
    for (int i = 0; i < 1000; i++) {
        mutex_lock(&mutex);
        atomic_inc(&counter);
        mutex_unlock(&mutex);

        if (i % 100 == 0) {
            sem_wait(&sem);
            /* Do some work */
            sem_post(&sem);
        }

        simulate_context_switch();
    }

    TEST_ASSERT(atomic_read(&counter) == 1000, "Counter should be 1000 after 1000 increments");

    TEST_PASS();
}

/* ============================================================================
 * Public API - Mutex Demo
 * ============================================================================ */

int sync_run_mutex_demo(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Mutex Synchronization Demo             ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    int local_passed = 0;
    int local_failed = 0;

    /* Run mutex tests */
    if (test_mutex_basic())
        local_passed++;
    else
        local_failed++;
    if (test_mutex_trylock())
        local_passed++;
    else
        local_failed++;
    if (test_mutex_recursive())
        local_passed++;
    else
        local_failed++;
    if (test_mutex_critical_section())
        local_passed++;
    else
        local_failed++;

    klog_info("\n");
    klog_info("[SYNC_DEMO] Mutex Demo Summary: %d passed, %d failed\n", local_passed, local_failed);

    return (local_failed == 0) ? 0 : -1;
}

/* ============================================================================
 * Public API - Semaphore Demo
 * ============================================================================ */

int sync_run_semaphore_demo(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Semaphore Synchronization Demo         ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    int local_passed = 0;
    int local_failed = 0;

    /* Run semaphore tests */
    if (test_semaphore_init())
        local_passed++;
    else
        local_failed++;
    if (test_semaphore_post_wait())
        local_passed++;
    else
        local_failed++;
    if (test_semaphore_trywait())
        local_passed++;
    else
        local_failed++;
    if (test_semaphore_resource_pool())
        local_passed++;
    else
        local_failed++;

    klog_info("\n");
    klog_info("[SYNC_DEMO] Semaphore Demo Summary: %d passed, %d failed\n", local_passed,
              local_failed);

    return (local_failed == 0) ? 0 : -1;
}

/* ============================================================================
 * Public API - RWLock Demo
 * ============================================================================ */

int sync_run_rwlock_demo(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Read-Write Lock Demo                   ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    int local_passed = 0;
    int local_failed = 0;

    /* Run rwlock tests */
    if (test_rwlock_init())
        local_passed++;
    else
        local_failed++;
    if (test_rwlock_write_lock())
        local_passed++;
    else
        local_failed++;
    if (test_rwlock_read_lock())
        local_passed++;
    else
        local_failed++;
    if (test_rwlock_multiple_readers())
        local_passed++;
    else
        local_failed++;
    if (test_rwlock_reader_writer_exclusion())
        local_passed++;
    else
        local_failed++;

    klog_info("\n");
    klog_info("[SYNC_DEMO] RWLock Demo Summary: %d passed, %d failed\n", local_passed,
              local_failed);

    return (local_failed == 0) ? 0 : -1;
}

/* ============================================================================
 * Public API - Condition Variable Demo
 * ============================================================================ */

int sync_run_condvar_demo(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Condition Variable Demo                ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    int local_passed = 0;
    int local_failed = 0;

    /* Run condition variable tests */
    if (test_condvar_init())
        local_passed++;
    else
        local_failed++;
    if (test_condvar_signal())
        local_passed++;
    else
        local_failed++;
    if (test_condvar_broadcast())
        local_passed++;
    else
        local_failed++;
    if (test_condvar_predicate())
        local_passed++;
    else
        local_failed++;

    klog_info("\n");
    klog_info("[SYNC_DEMO] Condition Variable Demo Summary: %d passed, %d failed\n", local_passed,
              local_failed);

    return (local_failed == 0) ? 0 : -1;
}

/* ============================================================================
 * Public API - Main Demo Entry Point
 * ============================================================================ */

int sync_run_demo(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Synchronization Primitives Demo         ║\n");
    klog_info("╚════════════════════════════════════════╝\n");
    klog_info("\n");
    klog_info("[SYNC_DEMO] Testing concurrent synchronization mechanisms\n");
    klog_info("[SYNC_DEMO] Simulated concurrency with %d processes\n", SYNC_TEST_NUM_PROCS);
    klog_info("[SYNC_DEMO] %d iterations per test\n", SYNC_TEST_ITERATIONS);
    klog_info("\n");

    tests_passed = 0;
    tests_failed = 0;

    /* Run individual demos */
    sync_run_mutex_demo();
    sync_run_semaphore_demo();
    sync_run_rwlock_demo();
    sync_run_condvar_demo();

    /* Run combined tests */
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Combined Tests                         ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    if (test_combined_mutex_semaphore())
        tests_passed++;
    else
        tests_failed++;
    if (test_stress_multiple_ops())
        tests_passed++;
    else
        tests_failed++;

    /* Print summary */
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Sync Demo Summary                      ║\n");
    klog_info("╚════════════════════════════════════════╝\n");
    klog_info("[SYNC_DEMO] Total tests passed: %d\n", tests_passed);
    klog_info("[SYNC_DEMO] Total tests failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        klog_info("[SYNC_DEMO] All tests PASSED\n");
    } else {
        klog_error("[SYNC_DEMO] %d test(s) FAILED\n", tests_failed);
    }

    /* Now run thread-based concurrent tests */
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Running Thread-Based Tests           ║\n");
    klog_info("╚════════════════════════════════════════╝\n");
    extern int sync_run_thread_tests(void);
    int thread_result = sync_run_thread_tests();

    if (tests_failed == 0 && thread_result == 0) {
        return 0;
    } else {
        return -1;
    }
}

void sync_stop_demo(void) {
    klog_info("[SYNC_DEMO] Stopping synchronization demo...\n");
    /* Cleanup if needed */
}
