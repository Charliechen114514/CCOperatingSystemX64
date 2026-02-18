/* ==============================================================================
 * CCOS - Priority Scheduling Class (System Processes Only)
 * ==============================================================================
 * This module implements a priority-based scheduling algorithm.
 * Higher priority (lower number) tasks run first.
 * Uses active/expired queue mechanism for fairness.
 * ==============================================================================
 */

#pragma once

#include "process/sched.h"

/* ==============================================================================
 * Priority Constants
 * ==============================================================================
 */

/**
 * @brief Priority levels
 * Higher numeric value = lower priority (Linux-style)
 * Range: 0-127
 */
#define PRIO_MAX      0      /* Highest priority */
#define PRIO_MIN      127    /* Lowest priority */
#define PRIO_DEFAULT  64     /* Default priority */

/* Number of priority levels */
#define PRIO_LEVELS   128

/**
 * @brief Time slice for each priority level
 * Higher priority (lower number) gets shorter time slice
 */
#define PRIO_TIMESLICE(prio) \
    ((prio) < 64 ? 5 : 20)   /* High priority: 5ms, Low: 20ms */

/* ==============================================================================
 * Priority Run Queue Data
 * ==============================================================================
 */

/**
 * @brief Per-priority queue data for priority scheduler
 */
typedef struct prio_rq_data {
    list_head    active[PRIO_LEVELS];   /* Active queues per priority */
    list_head    expired[PRIO_LEVELS];  /* Expired queues per priority */
    uint32_t     nr_active;             /* Total active tasks */
    uint32_t     nr_expired;            /* Total expired tasks */
    int          highest_prio;          /* Current highest priority */
    bool         active_expired;        /* Whether to swap arrays */
} prio_rq_data_t;

/* ==============================================================================
 * Priority Class API
 * ==============================================================================
 */

/**
 * @brief Initialize the Priority scheduling class
 * @return 0 on success, negative on error
 */
int sched_prio_init(void);

/**
 * @brief Get the Priority scheduling class structure
 * @return Pointer to the Priority class
 */
sched_class_t* sched_prio_get_class(void);
