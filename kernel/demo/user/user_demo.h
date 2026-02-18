/**
 * @file user_demo.h
 * @brief User Mode Support Demo
 *
 * This demo tests the user mode (Ring 3) support including:
 * - User memory management (brk, mmap)
 * - User mode process creation
 * - Safe user memory access
 * - Syscalls from user mode
 */

#pragma once

#include "defines/types.h"

/**
 * @brief Run the user mode demo
 *
 * This demonstrates:
 * 1. Creating a user mode process
 * 2. Setting up user stack and memory
 * 3. Testing syscalls from user mode
 * 4. Verifying user/kernel mode isolation
 *
 * @return 0 on success, negative on error
 */
int user_run_demo(void);

/**
 * @brief Stop the user mode demo
 */
void user_stop_demo(void);
