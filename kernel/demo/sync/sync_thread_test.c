/**
 * @file sync_thread_test.c
 * @brief Synchronization Primitives Test Using Real Threads
 *
 * This demo tests synchronization primitives using real kernel threads
 * to verify proper concurrent behavior.
 */

#include "klogs/kprintf.h"
#include "mm/heap/heap.h"
#include "process/process.h"
#include "process/sched.h"
#include "sync/atomic.h"
#include "sync/condvar.h"
#include "sync/mutex.h"
#include "sync/rwlock.h"
#include "sync/semaphore.h"

/* ============================================================================
 * Test Configuration
 * ============================================================================ */

#define NUM_THREADS 4
#define ITERATIONS 1000

/* ============================================================================
 * Shared Test Data
 * ============================================================================ */

typedef struct {
    atomic_t counter;
    int unsafe_counter;
    mutex_t mutex;
    int protected_counter;
    volatile bool ready;
    atomic_t threads_done;  /* Number of threads that finished */
} counter_test_t;

static counter_test_t* g_counter_test = NULL;

/* ============================================================================
 * Thread Functions
 * ============================================================================ */

static void unsafe_counter_thread(void* arg) {
    (void)arg;
    while (!g_counter_test->ready) {
        __asm__ volatile("pause");
    }

    for (int i = 0; i < ITERATIONS; i++) {
        g_counter_test->unsafe_counter++;
    }

    atomic_inc(&g_counter_test->threads_done);
    klog_info("[SYNC_THREAD] Unsafe thread done: %d\n", g_counter_test->unsafe_counter);
}

static void safe_counter_thread(void* arg) {
    (void)arg;
    while (!g_counter_test->ready) {
        __asm__ volatile("pause");
    }

    for (int i = 0; i < ITERATIONS; i++) {
        mutex_lock(&g_counter_test->mutex);
        g_counter_test->protected_counter++;
        atomic_inc(&g_counter_test->counter);
        mutex_unlock(&g_counter_test->mutex);
    }

    klog_info("[SYNC_THREAD] Safe thread done: protected=%d, atomic=%d\n",
              g_counter_test->protected_counter, atomic_read(&g_counter_test->counter));
}

/* ============================================================================
 * Producer-Consumer Test
 * ============================================================================ */

typedef struct {
    int buffer[8];
    int write_pos;
    int read_pos;
    int count;
    semaphore_t empty;
    semaphore_t full;
    mutex_t mutex;
    atomic_t produced;
    atomic_t consumed;
    volatile bool ready;
} pc_buffer_t;

static pc_buffer_t* g_pc_buffer = NULL;

static void producer_thread(void* arg) {
    (void)arg;
    while (!g_pc_buffer->ready) {
        __asm__ volatile("pause");
    }

    for (int i = 0; i < ITERATIONS; i++) {
        sem_wait(&g_pc_buffer->empty);
        mutex_lock(&g_pc_buffer->mutex);

        g_pc_buffer->buffer[g_pc_buffer->write_pos] = i;
        g_pc_buffer->write_pos = (g_pc_buffer->write_pos + 1) % 8;
        g_pc_buffer->count++;
        atomic_inc(&g_pc_buffer->produced);

        mutex_unlock(&g_pc_buffer->mutex);
        sem_post(&g_pc_buffer->full);
    }

    klog_info("[SYNC_THREAD] Producer done: produced=%d\n", atomic_read(&g_pc_buffer->produced));
}

static void consumer_thread(void* arg) {
    (void)arg;
    while (!g_pc_buffer->ready) {
        __asm__ volatile("pause");
    }

    int sum = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        sem_wait(&g_pc_buffer->full);
        mutex_lock(&g_pc_buffer->mutex);

        int value = g_pc_buffer->buffer[g_pc_buffer->read_pos];
        g_pc_buffer->read_pos = (g_pc_buffer->read_pos + 1) % 8;
        g_pc_buffer->count--;
        atomic_inc(&g_pc_buffer->consumed);
        sum += value;

        mutex_unlock(&g_pc_buffer->mutex);
        sem_post(&g_pc_buffer->empty);
    }

    klog_info("[SYNC_THREAD] Consumer done: consumed=%d, sum=%d\n",
              atomic_read(&g_pc_buffer->consumed), sum);
}

/* ============================================================================
 * Read-Write Lock Test
 * ============================================================================ */

typedef struct {
    rwlock_t rwlock;
    int shared_data;
    atomic_t read_count;
    atomic_t write_count;
    volatile bool ready;
} rwlock_test_t;

static rwlock_test_t* g_rwlock_test = NULL;

static void reader_thread(void* arg) {
    int id = *(int*)arg;

    while (!g_rwlock_test->ready) {
        __asm__ volatile("pause");
    }

    for (int i = 0; i < 100; i++) {
        rwlock_read_lock(&g_rwlock_test->rwlock);
        int value = g_rwlock_test->shared_data;
        atomic_inc(&g_rwlock_test->read_count);
        (void)value; /* Suppress unused warning */
        rwlock_read_unlock(&g_rwlock_test->rwlock);
    }

    klog_info("[SYNC_THREAD] Reader %d done\n", id);
}

static void writer_thread(void* arg) {
    int id = *(int*)arg;

    while (!g_rwlock_test->ready) {
        __asm__ volatile("pause");
    }

    for (int i = 0; i < 50; i++) {
        rwlock_write_lock(&g_rwlock_test->rwlock);
        g_rwlock_test->shared_data++;
        atomic_inc(&g_rwlock_test->write_count);
        rwlock_write_unlock(&g_rwlock_test->rwlock);
    }

    klog_info("[SYNC_THREAD] Writer %d done\n", id);
}

/* ============================================================================
 * Condition Variable Test
 * ============================================================================ */

typedef struct {
    mutex_t mutex;
    condvar_t cv;
    int flag;
    int wait_count;
    volatile bool ready;
} condvar_test_t;

static condvar_test_t* g_condvar_test = NULL;

static void waiter_thread(void* arg) {
    (void)arg;
    while (!g_condvar_test->ready) {
        __asm__ volatile("pause");
    }

    mutex_lock(&g_condvar_test->mutex);
    g_condvar_test->wait_count++;

    /* Wait for flag to be set */
    while (g_condvar_test->flag == 0) {
        condvar_wait(&g_condvar_test->cv, &g_condvar_test->mutex);
    }

    g_condvar_test->wait_count--;
    mutex_unlock(&g_condvar_test->mutex);

    klog_info("[SYNC_THREAD] Waiter thread awakened\n");
}

static void signaler_thread(void* arg) {
    (void)arg;
    while (!g_condvar_test->ready) {
        __asm__ volatile("pause");
    }

    /* Small delay to let waiters start waiting */
    for (volatile int i = 0; i < 10000; i++)
        ;

    mutex_lock(&g_condvar_test->mutex);
    g_condvar_test->flag = 1;

    /* Signal all waiters */
    for (int i = 0; i < 3; i++) {
        condvar_signal(&g_condvar_test->cv);
    }

    mutex_unlock(&g_condvar_test->mutex);

    klog_info("[SYNC_THREAD] Signaler thread done\n");
}

/* ============================================================================
 * Test Functions
 * ============================================================================ */

int sync_test_mutex_with_threads(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Mutex Test With Threads              ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    /* Allocate test data */
    g_counter_test = (counter_test_t*)kmalloc(sizeof(counter_test_t));
    if (!g_counter_test) {
        klog_error("[SYNC_THREAD] Failed to allocate test data\n");
        return -1;
    }

    atomic_write(&g_counter_test->counter, 0);
    g_counter_test->unsafe_counter = 0;
    g_counter_test->protected_counter = 0;
    mutex_init(&g_counter_test->mutex);
    g_counter_test->ready = false;

    /* Create threads */
    pcb_t* threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        if (i < NUM_THREADS / 2) {
            threads[i] =
                proc_create_kernel_thread(unsafe_counter_thread, &thread_ids[i], "unsafe_cnt");
        } else {
            threads[i] = proc_create_kernel_thread(safe_counter_thread, &thread_ids[i], "safe_cnt");
        }
        if (!threads[i]) {
            klog_error("[SYNC_THREAD] Failed to create thread %d\n", i);
            kfree(g_counter_test);
            return -1;
        }
    }

    /* Start all threads */
    g_counter_test->ready = true;

    /* Give threads time to complete */
    for (volatile int i = 0; i < 1000000; i++)
        ;

    int expected = (NUM_THREADS / 2) * ITERATIONS;
    int unsafe = g_counter_test->unsafe_counter;
    int protected = g_counter_test->protected_counter;
    int atomic_val = atomic_read(&g_counter_test->counter);

    klog_info("[SYNC_THREAD] Results:\n");
    klog_info("[SYNC_THREAD]   Expected: %d\n", expected);
    klog_info("[SYNC_THREAD]   Unsafe: %d (may differ due to races)\n", unsafe);
    klog_info("[SYNC_THREAD]   Protected: %d\n", protected);
    klog_info("[SYNC_THREAD]   Atomic: %d\n", atomic_val);

    bool passed = (protected == expected) && (protected == atomic_val);

    kfree(g_counter_test);
    g_counter_test = NULL;

    if (passed) {
        klog_info("[SYNC_THREAD] Mutex test PASSED\n");
        return 0;
    } else {
        klog_error("[SYNC_THREAD] Mutex test FAILED\n");
        return -1;
    }
}

int sync_test_producer_consumer(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Producer-Consumer Test               ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    /* Allocate test data */
    g_pc_buffer = (pc_buffer_t*)kmalloc(sizeof(pc_buffer_t));
    if (!g_pc_buffer) {
        klog_error("[SYNC_THREAD] Failed to allocate test data\n");
        return -1;
    }

    for (int i = 0; i < 8; i++) {
        g_pc_buffer->buffer[i] = 0;
    }
    g_pc_buffer->write_pos = 0;
    g_pc_buffer->read_pos = 0;
    g_pc_buffer->count = 0;

    sem_init(&g_pc_buffer->empty, 8);
    sem_init(&g_pc_buffer->full, 0);
    mutex_init(&g_pc_buffer->mutex);
    atomic_write(&g_pc_buffer->produced, 0);
    atomic_write(&g_pc_buffer->consumed, 0);
    g_pc_buffer->ready = false;

    /* Create producer and consumer threads */
    pcb_t* producer = proc_create_kernel_thread(producer_thread, NULL, "producer");
    pcb_t* consumer = proc_create_kernel_thread(consumer_thread, NULL, "consumer");

    if (!producer || !consumer) {
        klog_error("[SYNC_THREAD] Failed to create threads\n");
        kfree(g_pc_buffer);
        return -1;
    }

    /* Start threads */
    g_pc_buffer->ready = true;

    /* Give threads time to complete */
    for (volatile int i = 0; i < 2000000; i++)
        ;

    int produced = atomic_read(&g_pc_buffer->produced);
    int consumed = atomic_read(&g_pc_buffer->consumed);

    klog_info("[SYNC_THREAD] Results:\n");
    klog_info("[SYNC_THREAD]   Produced: %d\n", produced);
    klog_info("[SYNC_THREAD]   Consumed: %d\n", consumed);
    klog_info("[SYNC_THREAD]   Buffer count: %d\n", g_pc_buffer->count);

    bool passed = (produced == ITERATIONS) && (consumed == ITERATIONS) && (g_pc_buffer->count == 0);

    kfree(g_pc_buffer);
    g_pc_buffer = NULL;

    if (passed) {
        klog_info("[SYNC_THREAD] Producer-Consumer test PASSED\n");
        return 0;
    } else {
        klog_error("[SYNC_THREAD] Producer-Consumer test FAILED\n");
        return -1;
    }
}

int sync_test_rwlock(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Read-Write Lock Test                  ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    /* Allocate test data */
    g_rwlock_test = (rwlock_test_t*)kmalloc(sizeof(rwlock_test_t));
    if (!g_rwlock_test) {
        klog_error("[SYNC_THREAD] Failed to allocate test data\n");
        return -1;
    }

    g_rwlock_test->shared_data = 0;
    rwlock_init(&g_rwlock_test->rwlock);
    atomic_write(&g_rwlock_test->read_count, 0);
    atomic_write(&g_rwlock_test->write_count, 0);
    g_rwlock_test->ready = false;

    /* Create reader and writer threads */
    pcb_t* threads[4];
    int ids[4] = {0, 1, 2, 3};

    threads[0] = proc_create_kernel_thread(reader_thread, &ids[0], "reader0");
    threads[1] = proc_create_kernel_thread(reader_thread, &ids[1], "reader1");
    threads[2] = proc_create_kernel_thread(writer_thread, &ids[2], "writer0");
    threads[3] = proc_create_kernel_thread(writer_thread, &ids[3], "writer1");

    if (!threads[0] || !threads[1] || !threads[2] || !threads[3]) {
        klog_error("[SYNC_THREAD] Failed to create threads\n");
        kfree(g_rwlock_test);
        return -1;
    }

    /* Start threads */
    g_rwlock_test->ready = true;

    /* Give threads time to complete */
    for (volatile int i = 0; i < 1000000; i++)
        ;

    int reads = atomic_read(&g_rwlock_test->read_count);
    int writes = atomic_read(&g_rwlock_test->write_count);
    int data = g_rwlock_test->shared_data;

    klog_info("[SYNC_THREAD] Results:\n");
    klog_info("[SYNC_THREAD]   Read operations: %d (expected ~200)\n", reads);
    klog_info("[SYNC_THREAD]   Write operations: %d (expected 100)\n", writes);
    klog_info("[SYNC_THREAD]   Final data value: %d (expected 100)\n", data);

    bool passed = (writes == 100) && (data == 100);

    kfree(g_rwlock_test);
    g_rwlock_test = NULL;

    if (passed) {
        klog_info("[SYNC_THREAD] RWLock test PASSED\n");
        return 0;
    } else {
        klog_error("[SYNC_THREAD] RWLock test FAILED\n");
        return -1;
    }
}

int sync_test_condvar(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Condition Variable Test               ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    /* Allocate test data */
    g_condvar_test = (condvar_test_t*)kmalloc(sizeof(condvar_test_t));
    if (!g_condvar_test) {
        klog_error("[SYNC_THREAD] Failed to allocate test data\n");
        return -1;
    }

    mutex_init(&g_condvar_test->mutex);
    condvar_init(&g_condvar_test->cv);
    g_condvar_test->flag = 0;
    g_condvar_test->wait_count = 0;
    g_condvar_test->ready = false;

    /* Create waiter and signaler threads */
    pcb_t* waiters[3];
    int ids[3] = {0, 1, 2};

    for (int i = 0; i < 3; i++) {
        waiters[i] = proc_create_kernel_thread(waiter_thread, &ids[i], "waiter");
        if (!waiters[i]) {
            klog_error("[SYNC_THREAD] Failed to create waiter thread\n");
            kfree(g_condvar_test);
            return -1;
        }
    }

    pcb_t* signaler = proc_create_kernel_thread(signaler_thread, NULL, "signaler");
    if (!signaler) {
        klog_error("[SYNC_THREAD] Failed to create signaler thread\n");
        kfree(g_condvar_test);
        return -1;
    }

    /* Start threads */
    g_condvar_test->ready = true;

    /* Give threads time to complete */
    for (volatile int i = 0; i < 2000000; i++)
        ;

    klog_info("[SYNC_THREAD] Results:\n");
    klog_info("[SYNC_THREAD]   Flag: %d\n", g_condvar_test->flag);
    klog_info("[SYNC_THREAD]   Wait count: %d\n", g_condvar_test->wait_count);

    bool passed = (g_condvar_test->flag == 1) && (g_condvar_test->wait_count == 0);

    kfree(g_condvar_test);
    g_condvar_test = NULL;

    if (passed) {
        klog_info("[SYNC_THREAD] Condition Variable test PASSED\n");
        return 0;
    } else {
        klog_error("[SYNC_THREAD] Condition Variable test FAILED\n");
        return -1;
    }
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int sync_run_thread_tests(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Sync Thread Tests                     ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    int passed = 0;
    int total = 4;

    if (sync_test_mutex_with_threads() == 0) {
        passed++;
    }
    if (sync_test_producer_consumer() == 0) {
        passed++;
    }
    if (sync_test_rwlock() == 0) {
        passed++;
    }
    if (sync_test_condvar() == 0) {
        passed++;
    }

    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Sync Thread Tests Summary             ║\n");
    klog_info("╚════════════════════════════════════════╝\n");
    klog_info("[SYNC_THREAD] Tests passed: %d/%d\n", passed, total);

    if (passed == total) {
        klog_info("[SYNC_THREAD] All thread tests PASSED\n");
        return 0;
    } else {
        klog_error("[SYNC_THREAD] %d test(s) FAILED\n", total - passed);
        return -1;
    }
}
