/**
 * @file rtc_demo.h
 * @brief RTC Driver Demo - Demonstrates RTC functionality
 */

#pragma once

#include "defines/types.h"

/**
 * @brief Run all RTC demos
 *
 * This function demonstrates the various RTC capabilities:
 * 1. Reading current time
 * 2. Checking RTC status
 * 3. Enabling periodic interrupts (1 Hz ticks)
 * 4. Setting an alarm (triggers in ~10 seconds)
 *
 * Call this function after RTC initialization to see the demos.
 */
void rtc_run_demo(void);

/**
 * @brief Stop the RTC demo
 *
 * Disables periodic interrupts and alarm.
 */
void rtc_stop_demo(void);
