/**
 * @file heap_demo.h
 * @brief Heap Allocator Demo - Demonstrates kmalloc/kfree functionality
 */

#pragma once

#include "defines/types.h"

/**
 * heap_run_demo - Run the heap allocator demonstration
 *
 * This demo tests various heap features:
 * 1. Basic allocation and deallocation
 * 2. Multiple allocations
 * 3. Block splitting and coalescing
 * 4. Heap expansion
 * 5. Aligned allocation
 * 6. Reallocation
 * 7. Edge cases (NULL, zero size, double free)
 *
 * @return 0 on success, negative on failure
 */
int heap_run_demo(void);

/**
 * heap_stop_demo - Stop the heap demo and cleanup resources
 */
void heap_stop_demo(void);
