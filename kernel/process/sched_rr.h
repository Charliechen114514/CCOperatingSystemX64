/* ==============================================================================
 * CCOS - Round-Robin Scheduling Class
 * ==============================================================================
 * This module implements a simple Round-Robin scheduling algorithm.
 * Each task gets a time slice and executes in FIFO order.
 * ==============================================================================
 */

#pragma once

#include "process/sched.h"

/* ==============================================================================
 * Round-Robin Constants
 * ==============================================================================
 */

/**
 * @brief Default RR time slice in milliseconds
 */
#define RR_TIMESLICE_DEFAULT    DEF_TIMESLICE_MS

/* ==============================================================================
 * Round-Robin Class API
 * ==============================================================================
 */

/**
 * @brief Initialize the Round-Robin scheduling class
 * @return 0 on success, negative on error
 */
int sched_rr_init(void);

/**
 * @brief Get the RR scheduling class structure
 * @return Pointer to the RR class
 */
sched_class_t* sched_rr_get_class(void);
