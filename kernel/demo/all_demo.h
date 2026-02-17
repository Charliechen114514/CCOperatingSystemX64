/**
 * @file all_demo.h
 * @brief Central demo controller - runs all enabled demos
 *
 * This file provides a unified interface for running all enabled demos.
 * Individual demos are controlled by CMake options (e.g., ENABLE_RTC_DEMO).
 */

#pragma once

/**
 * @brief Run all enabled demos
 *
 * This function checks which demos are enabled at compile time and runs them.
 * Demos are controlled by CMake options like:
 *   - ENABLE_RTC_DEMO: Enable RTC driver demo
 *
 * This function should be called after all device initialization is complete.
 */
void run_possible_demos(void);
