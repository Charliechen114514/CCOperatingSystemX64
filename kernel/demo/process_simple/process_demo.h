/**
 * @file process_demo.h
 * @brief Process Management Demo - Demonstrates process management functionality
 */

#pragma once

#include "defines/types.h"

/**
 * process_run_demo - Run the process management demonstration
 *
 * This demo tests various process management features:
 * 1. PID allocation and deallocation
 * 2. PCB allocation and deallocation
 * 3. Process state management
 * 4. Scheduler initialization
 *
 * @return 0 on success, negative on failure
 */
int process_run_demo(void);

/**
 * process_stop_demo - Stop the process demo and cleanup resources
 */
void process_stop_demo(void);
