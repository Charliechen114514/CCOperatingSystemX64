/**
 * @file vmm_demo.h
 * @brief VMM Demo - Demonstrates Virtual Memory Management functionality
 */

#pragma once

#include "defines/types.h"

/**
 * vmm_run_demo - Run the VMM demonstration
 *
 * This demo tests various VMM features:
 * 1. Page table information and statistics
 * 2. Virtual page allocation
 * 3. Physical-to-virtual mapping
 * 4. Address translation tests
 * 5. User address space creation
 * 6. Page fault handling (optional, requires explicit enable)
 *
 * @param test_page_fault If true, intentionally trigger a page fault to test handler
 */
void vmm_run_demo(bool test_page_fault);

/**
 * vmm_stop_demo - Stop the VMM demo and cleanup resources
 */
void vmm_stop_demo(void);
