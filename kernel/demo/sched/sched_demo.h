/**
 * @file sched_demo.h
 * @brief Scheduling Class Demo - Demonstrates RR and Priority scheduling algorithms
 */

#pragma once

#include "defines/types.h"

/**
 * sched_run_rr_demo - Run the Round-Robin scheduling demo
 *
 * This demo demonstrates the Round-Robin scheduling algorithm:
 * 1. Multiple tasks with equal priority
 * 2. Time slice management (10ms default)
 * 3. FIFO task selection
 * 4. Fair scheduling among tasks
 *
 * @return 0 on success, negative on failure
 */
int sched_run_rr_demo(void);

/**
 * sched_stop_rr_demo - Stop the RR demo and cleanup resources
 */
void sched_stop_rr_demo(void);

/**
 * sched_run_prio_demo - Run the Priority scheduling demo
 *
 * This demo demonstrates the Priority scheduling algorithm:
 * 1. Tasks with different priorities (0-127)
 * 2. Higher priority (lower number) tasks run first
 * 3. Active/Expired queue mechanism
 * 4. Priority-based preemption
 *
 * @return 0 on success, negative on failure
 */
int sched_run_prio_demo(void);

/**
 * sched_stop_prio_demo - Stop the Priority demo and cleanup resources
 */
void sched_stop_prio_demo(void);
