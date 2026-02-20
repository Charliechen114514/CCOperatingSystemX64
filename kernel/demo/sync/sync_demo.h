/**
 * @file sync_demo.h
 * @brief Synchronization Primitives Demo - Demonstrates concurrent testing of sync mechanisms
 *
 * This demo tests the synchronization primitives with multiple processes/threads:
 * - Mutex: Mutual exclusion lock testing
 * - Semaphore: Resource counting and signaling
 * - Read-Write Lock: Reader-preference concurrency
 * - Condition Variable: Event-based synchronization
 */

#pragma once

#include "defines/types.h"

/* ============================================================================
 * Public API - Run all sync demos
 * ============================================================================ */

/**
 * @brief Run all synchronization primitive demos
 * @return 0 on success, negative on error
 */
int sync_run_demo(void);

/**
 * @brief Stop/cleanup sync demo resources
 */
void sync_stop_demo(void);

/* ============================================================================
 * Individual Test Functions
 * ============================================================================ */

/**
 * @brief Run mutex concurrency tests
 * @return 0 on success, negative on error
 */
int sync_run_mutex_demo(void);

/**
 * @brief Run semaphore concurrency tests
 * @return 0 on success, negative on error
 */
int sync_run_semaphore_demo(void);

/**
 * @brief Run read-write lock concurrency tests
 * @return 0 on success, negative on error
 */
int sync_run_rwlock_demo(void);

/**
 * @brief Run condition variable concurrency tests
 * @return 0 on success, negative on error
 */
int sync_run_condvar_demo(void);

/* ============================================================================
 * Thread-Based Tests
 * ============================================================================ */

/**
 * @brief Run synchronization tests using real kernel threads
 * @return 0 on success, negative on error
 *
 * These tests use actual kernel threads to verify that synchronization
 * primitives work correctly with concurrent execution.
 */
int sync_run_thread_tests(void);
