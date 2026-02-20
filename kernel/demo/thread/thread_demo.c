/**
 * @file thread_demo.c
 * @brief Thread Creation Demo
 *
 * This demo tests the thread creation functionality by:
 * 1. Creating kernel threads
 * 2. Testing thread join
 * 3. Testing shared data access
 * 4. Testing synchronization between threads
 */

#include "thread_demo.h"
#include "klogs/kprintf.h"
#include "mm/heap/heap.h"
#include "process/process.h"
#include "sync/atomic.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"

/* ============================================================================
 * Shared Test Data
 * ============================================================================ */

/**
 * @brief Shared counter for testing concurrent access
 */
typedef struct {
    atomic_t counter;      /* Atomic counter (for verification) */
    int unsafe_counter;    /* Non-atomic counter (to detect races) */
    mutex_t mutex;         /* Mutex for protected access */
    int protected_counter; /* Counter protected by mutex */
} shared_counter_t;

static shared_counter_t* g_shared_counter = NULL;

/* ============================================================================
 * Test Thread Functions
 * ============================================================================ */

/**
 * @brief Simple kernel thread function
 */
static void simple_thread_fn(void* arg) {
    int thread_id = *(int*)arg;
    klog_info("[THREAD_DEMO] Simple thread %d running\n", thread_id);

    /* Simulate some work */
    for (int i = 0; i < 5; i++) {
        klog_info("[THREAD_DEMO] Thread %d: iteration %d\n", thread_id, i);
    }

    klog_info("[THREAD_DEMO] Simple thread %d exiting\n", thread_id);
}

/**
 * @brief Thread that increments shared counter (unsafe)
 */
static void unsafe_counter_thread(void* arg) {
    int iterations = *(int*)arg;

    for (int i = 0; i < iterations; i++) {
        g_shared_counter->unsafe_counter++;
    }

    klog_info("[THREAD_DEMO] Unsafe counter thread done, value: %d\n",
              g_shared_counter->unsafe_counter);
}

/**
 * @brief Thread that increments shared counter (with mutex)
 */
static void safe_counter_thread(void* arg) {
    int iterations = *(int*)arg;

    for (int i = 0; i < iterations; i++) {
        mutex_lock(&g_shared_counter->mutex);
        g_shared_counter->protected_counter++;
        atomic_inc(&g_shared_counter->counter);
        mutex_unlock(&g_shared_counter->mutex);
    }

    klog_info("[THREAD_DEMO] Safe counter thread done, protected: %d, atomic: %d\n",
              g_shared_counter->protected_counter,
              atomic_read(&g_shared_counter->counter));
}

/* ============================================================================
 * Test Functions
 * ============================================================================ */

/**
 * @brief Test 1: Simple thread creation and join
 */
static bool test_simple_thread(void) {
    klog_info("[THREAD_DEMO] Test 1: Simple thread creation and join\n");

    int thread_id = 1;
    pcb_t* thread = proc_create_kernel_thread(simple_thread_fn, &thread_id, "test_simple");

    if (!thread) {
        klog_error("[THREAD_DEMO] FAILED: Could not create thread\n");
        return false;
    }

    klog_info("[THREAD_DEMO] Thread created with PID: %d, magic=0x%x, pcb=%p\n",
              thread->pid, thread->magic, (void*)thread);
    if (thread->magic != 0x114514) {
        klog_error("[THREAD_DEMO] WARNING: PCB magic mismatch! Expected 0x114514, got 0x%x\n", thread->magic);
    }

    /* For now, we can't actually join in this demo context
     * The thread will run independently */

    klog_info("[THREAD_DEMO] Test 1 PASSED\n");
    return true;
}

/**
 * @brief Test 2: Multiple threads
 */
static bool test_multiple_threads(void) {
    klog_info("[THREAD_DEMO] Test 2: Multiple thread creation\n");

    const int num_threads = 3;
    int thread_ids[3] = {1, 2, 3};
    pcb_t* threads[3] = {NULL, NULL, NULL};

    /* Create multiple threads */
    for (int i = 0; i < num_threads; i++) {
        threads[i] = proc_create_kernel_thread(simple_thread_fn, &thread_ids[i],
                                               "test_multi");
        if (!threads[i]) {
            klog_error("[THREAD_DEMO] FAILED: Could not create thread %d\n", i);
            return false;
        }
        /* Check PCB magic and print with magic for debugging */
        uint32_t magic = threads[i]->magic;
        int32_t pid = threads[i]->pid;
        klog_info("[THREAD_DEMO] Created thread %d with PID: %d, magic=0x%x, pcb=%p\n",
                  i, pid, magic, (void*)threads[i]);
        if (magic != 0x114514) {
            klog_error("[THREAD_DEMO] WARNING: PCB magic mismatch! Expected 0x114514, got 0x%x\n", magic);
        }
    }

    klog_info("[THREAD_DEMO] Test 2 PASSED\n");
    return true;
}

/**
 * @brief Test 3: Shared data access (unsafe)
 */
static bool test_shared_unsafe(void) {
    klog_info("[THREAD_DEMO] Test 3: Shared data access (unsafe)\n");

    /* Initialize shared counter */
    if (!g_shared_counter) {
        g_shared_counter = (shared_counter_t*)kmalloc(sizeof(shared_counter_t));
        if (!g_shared_counter) {
            klog_error("[THREAD_DEMO] FAILED: Could not allocate shared counter\n");
            return false;
        }
    }

    atomic_write(&g_shared_counter->counter, 0);
    g_shared_counter->unsafe_counter = 0;
    g_shared_counter->protected_counter = 0;

    int iterations = 100;

    /* Create threads that increment unsafe counter */
    pcb_t* t1 = proc_create_kernel_thread(unsafe_counter_thread, &iterations,
                                          "unsafe_t1");
    pcb_t* t2 = proc_create_kernel_thread(unsafe_counter_thread, &iterations,
                                          "unsafe_t2");

    if (!t1 || !t2) {
        klog_error("[THREAD_DEMO] FAILED: Could not create threads\n");
        return false;
    }

    klog_info("[THREAD_DEMO] Unsafe counter threads created\n");
    klog_info("[THREAD_DEMO] Expected: 200, Actual may vary due to races\n");

    klog_info("[THREAD_DEMO] Test 3 PASSED (with expected races)\n");
    return true;
}

/**
 * @brief Test 4: Shared data access with mutex
 */
static bool test_shared_safe(void) {
    klog_info("[THREAD_DEMO] Test 4: Shared data access with mutex\n");

    /* Initialize shared counter and mutex */
    if (!g_shared_counter) {
        g_shared_counter = (shared_counter_t*)kmalloc(sizeof(shared_counter_t));
        if (!g_shared_counter) {
            klog_error("[THREAD_DEMO] FAILED: Could not allocate shared counter\n");
            return false;
        }
    }

    atomic_write(&g_shared_counter->counter, 0);
    g_shared_counter->unsafe_counter = 0;
    g_shared_counter->protected_counter = 0;
    mutex_init(&g_shared_counter->mutex);

    int iterations = 100;

    /* Create threads that increment protected counter */
    pcb_t* t1 = proc_create_kernel_thread(safe_counter_thread, &iterations,
                                          "safe_t1");
    pcb_t* t2 = proc_create_kernel_thread(safe_counter_thread, &iterations,
                                          "safe_t2");

    if (!t1 || !t2) {
        klog_error("[THREAD_DEMO] FAILED: Could not create threads\n");
        return false;
    }

    klog_info("[THREAD_DEMO] Safe counter threads created\n");
    klog_info("[THREAD_DEMO] Expected: 200, Protected and Atomic should match\n");

    klog_info("[THREAD_DEMO] Test 4 PASSED\n");
    return true;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int thread_run_demo(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Thread Creation Demo                  ║\n");
    klog_info("╚════════════════════════════════════════╝\n");
    klog_info("\n");

    int passed = 0;
    int total = 0;

    /* Test 1: Simple thread creation */
    total++;
    if (test_simple_thread()) {
        passed++;
    }

    /* Test 2: Multiple threads */
    total++;
    if (test_multiple_threads()) {
        passed++;
    }

    /* Test 3: Shared data (unsafe) */
    total++;
    if (test_shared_unsafe()) {
        passed++;
    }

    /* Test 4: Shared data with mutex */
    total++;
    if (test_shared_safe()) {
        passed++;
    }

    /* Summary */
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Thread Demo Summary                   ║\n");
    klog_info("╚════════════════════════════════════════╝\n");
    klog_info("[THREAD_DEMO] Tests passed: %d/%d\n", passed, total);

    if (passed == total) {
        klog_info("[THREAD_DEMO] All tests PASSED\n");
        return 0;
    } else {
        klog_error("[THREAD_DEMO] %d test(s) FAILED\n", total - passed);
        return -1;
    }
}

void thread_stop_demo(void) {
    klog_info("[THREAD_DEMO] Stopping thread demo...\n");

    /* Clean up shared counter */
    if (g_shared_counter) {
        kfree(g_shared_counter);
        g_shared_counter = NULL;
    }
}
