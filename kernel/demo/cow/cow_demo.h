/**
 * @file cow_demo.h
 * @brief COW Demo - Demonstrates Copy-on-Write functionality
 */

#pragma once

#include "defines/types.h"

/**
 * cow_run_demo - Run the COW demonstration
 *
 * This demo tests Copy-on-Write functionality:
 * 1. Create shared memory mappings
 * 2. Register COW region
 * 3. Trigger write faults to verify COW behavior
 * 4. Display COW statistics
 *
 * @return 0 on success, negative on error
 */
int cow_run_demo(void);

/**
 * cow_stop_demo - Stop the COW demo and cleanup resources
 */
void cow_stop_demo(void);
