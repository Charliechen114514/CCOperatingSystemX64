/**
 * @file mock_syscall_demo.h
 * @brief System Call Framework Mock Demo
 *
 * This demo tests the syscall framework by simulating system calls
 * from kernel mode (since we don't have user mode support yet).
 */

#pragma once

#include "defines/types.h"

/**
 * @brief Run the system call mock demo
 *
 * This demonstrates:
 * 1. MSR register configuration verification
 * 2. System call dispatch testing
 * 3. System call statistics
 *
 * @return 0 on success, negative on error
 */
int mock_syscall_run_demo(void);

/**
 * @brief Stop the mock syscall demo
 */
void mock_syscall_stop_demo(void);
