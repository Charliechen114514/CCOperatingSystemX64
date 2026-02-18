/**
 * @file sched_demo.c
 * @brief Scheduling Class Demo - Demonstrates RR and Priority scheduling algorithms
 */

#include "sched_demo.h"
#include "process/sched.h"
#include "process/sched_rr.h"
#include "process/sched_prio.h"
#include "process/process.h"
#include "mm/heap/heap.h"
#include "driver/timer/timer.h"
#include "base/string.h"
#include "base/memory.h"
#include "klogs/kprintf.h"

/* ============================================================================
 * Test Result Tracking
 * ============================================================================ */

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            klog_error("[SCHED_DEMO] FAILED: %s\n", message); \
            tests_failed++; \
            return false; \
        } \
    } while(0)

#define TEST_START(name) \
    klog_info("[SCHED_DEMO] Test: %s...\n", name)

#define TEST_PASS() \
    do { \
        klog_info("[SCHED_DEMO] PASSED\n"); \
        tests_passed++; \
        return true; \
    } while(0)

/* ============================================================================
 * Round-Robin Demo Tests
 * ============================================================================ */

/**
 * @brief Test 1: RR Class Initialization
 */
static bool test_rr_class_init(void) {
    TEST_START("RR Class Initialization");

    /* Verify class is registered */
    sched_class_t* rr_class = sched_rr_get_class();
    TEST_ASSERT(rr_class != NULL, "RR class not registered");
    TEST_ASSERT(rr_class->policy == SCHED_NORMAL, "RR class has wrong policy");
    TEST_ASSERT(rr_class->name != NULL, "RR class has no name");
    TEST_ASSERT(rr_class->enqueue_task != NULL, "RR class missing enqueue_task");
    TEST_ASSERT(rr_class->dequeue_task != NULL, "RR class missing dequeue_task");
    TEST_ASSERT(rr_class->pick_next_task != NULL, "RR class missing pick_next_task");
    TEST_ASSERT(rr_class->task_tick != NULL, "RR class missing task_tick");
    TEST_ASSERT(rr_class->task_fork != NULL, "RR class missing task_fork");
    TEST_ASSERT(rr_class->get_time_slice != NULL, "RR class missing get_time_slice");

    TEST_PASS();
}

/**
 * @brief Test 2: RR Time Slice Constants
 */
static bool test_rr_time_slice_constants(void) {
    TEST_START("RR Time Slice Constants");

    /* Verify RR time slice constants */
    TEST_ASSERT(RR_TIMESLICE_DEFAULT == DEF_TIMESLICE_MS,
               "RR time slice should match default");
    TEST_ASSERT(RR_TIMESLICE_DEFAULT == 10, "RR time slice should be 10ms");

    TEST_PASS();
}

/**
 * @brief Test 3: RR Should Preempt Logic
 */
static bool test_rr_should_preempt(void) {
    TEST_START("RR Should Preempt Logic");

    sched_class_t* rr_class = sched_rr_get_class();

    /* Create test tasks with minimal pcb_t structure */
    pcb_t task1, task2;
    memset(&task1, 0, sizeof(pcb_t));
    memset(&task2, 0, sizeof(pcb_t));

    INIT_LIST_HEAD(&task1.sched_entity.run_list);
    INIT_LIST_HEAD(&task2.sched_entity.run_list);

    task1.sched_entity.sched_class = rr_class;
    task1.sched_entity.policy = SCHED_NORMAL;
    task1.sched_entity.time_slice = 5;
    task1.sched_entity.time_slice_total = 10;

    task2.sched_entity.sched_class = rr_class;
    task2.sched_entity.policy = SCHED_NORMAL;
    task2.sched_entity.time_slice = 10;
    task2.sched_entity.time_slice_total = 10;

    /* Test 1: Task with time slice should not be preempted in RR */
    bool should_preempt = rr_class->should_preempt(&task2, &task1);
    TEST_ASSERT(!should_preempt,
               "Task with time slice should not be preempted in RR");

    /* Test 2: Task without time slice can be preempted */
    task1.sched_entity.time_slice = 0;
    should_preempt = rr_class->should_preempt(&task2, &task1);
    TEST_ASSERT(should_preempt,
               "Task without time slice should be preempted in RR");

    TEST_PASS();
}

/**
 * @brief Test 4: RR Get Time Slice
 */
static bool test_rr_get_time_slice(void) {
    TEST_START("RR Get Time Slice");

    sched_class_t* rr_class = sched_rr_get_class();

    pcb_t task;
    memset(&task, 0, sizeof(pcb_t));
    INIT_LIST_HEAD(&task.sched_entity.run_list);
    task.sched_entity.sched_class = rr_class;
    task.sched_entity.policy = SCHED_NORMAL;
    task.sched_entity.time_slice = 10;
    task.sched_entity.time_slice_total = 10;

    /* RR uses fixed time slice regardless of task */
    uint32_t slice = rr_class->get_time_slice(&task);
    TEST_ASSERT(slice == RR_TIMESLICE_DEFAULT,
               "RR should return fixed time slice");

    TEST_PASS();
}

/* ============================================================================
 * Priority Demo Tests
 * ============================================================================ */

/**
 * @brief Test 5: Priority Class Initialization
 */
static bool test_prio_class_init(void) {
    TEST_START("Priority Class Initialization");

    /* Verify class is registered */
    sched_class_t* prio_class = sched_prio_get_class();
    TEST_ASSERT(prio_class != NULL, "Priority class not registered");
    TEST_ASSERT(prio_class->policy == SCHED_PRIORITY,
               "Priority class has wrong policy");
    TEST_ASSERT(prio_class->name != NULL, "Priority class has no name");
    TEST_ASSERT(prio_class->enqueue_task != NULL, "Priority class missing enqueue_task");
    TEST_ASSERT(prio_class->dequeue_task != NULL, "Priority class missing dequeue_task");
    TEST_ASSERT(prio_class->pick_next_task != NULL, "Priority class missing pick_next_task");
    TEST_ASSERT(prio_class->task_tick != NULL, "Priority class missing task_tick");
    TEST_ASSERT(prio_class->task_fork != NULL, "Priority class missing task_fork");
    TEST_ASSERT(prio_class->get_time_slice != NULL, "Priority class missing get_time_slice");

    TEST_PASS();
}

/**
 * @brief Test 6: Priority Constants
 */
static bool test_priority_constants(void) {
    TEST_START("Priority Constants");

    /* Verify priority constants */
    TEST_ASSERT(PRIO_MAX == 0, "PRIO_MAX should be 0");
    TEST_ASSERT(PRIO_MIN == 127, "PRIO_MIN should be 127");
    TEST_ASSERT(PRIO_DEFAULT == 64, "PRIO_DEFAULT should be 64");
    TEST_ASSERT(PRIO_LEVELS == 128, "PRIO_LEVELS should be 128");

    /* Verify time slice calculation */
    TEST_ASSERT(PRIO_TIMESLICE(32) == 5, "High priority should get 5ms");
    TEST_ASSERT(PRIO_TIMESLICE(96) == 20, "Low priority should get 20ms");

    TEST_PASS();
}

/**
 * @brief Test 7: Priority Should Preempt Logic
 */
static bool test_priority_should_preempt(void) {
    TEST_START("Priority Should Preempt Logic");

    sched_class_t* prio_class = sched_prio_get_class();

    /* Create test tasks */
    pcb_t high_task, low_task, same_prio_task;
    memset(&high_task, 0, sizeof(pcb_t));
    memset(&low_task, 0, sizeof(pcb_t));
    memset(&same_prio_task, 0, sizeof(pcb_t));

    INIT_LIST_HEAD(&high_task.sched_entity.run_list);
    INIT_LIST_HEAD(&low_task.sched_entity.run_list);
    INIT_LIST_HEAD(&same_prio_task.sched_entity.run_list);

    high_task.sched_entity.sched_class = prio_class;
    high_task.sched_entity.policy = SCHED_PRIORITY;
    high_task.sched_entity.priority = 10;   /* Higher priority */
    high_task.sched_entity.time_slice = 10;

    low_task.sched_entity.sched_class = prio_class;
    low_task.sched_entity.policy = SCHED_PRIORITY;
    low_task.sched_entity.priority = 100;  /* Lower priority */
    low_task.sched_entity.time_slice = 10;

    same_prio_task.sched_entity.sched_class = prio_class;
    same_prio_task.sched_entity.policy = SCHED_PRIORITY;
    same_prio_task.sched_entity.priority = 10;   /* Same as high_task */
    same_prio_task.sched_entity.time_slice = 10;

    /* Test 1: Higher priority task preempts lower priority */
    bool should_preempt = prio_class->should_preempt(&high_task, &low_task);
    TEST_ASSERT(should_preempt,
               "Higher priority task should preempt lower priority");

    /* Test 2: Lower priority task does not preempt higher priority */
    should_preempt = prio_class->should_preempt(&low_task, &high_task);
    TEST_ASSERT(!should_preempt,
               "Lower priority task should not preempt higher priority");

    /* Test 3: Same priority with time slice - no preempt */
    should_preempt = prio_class->should_preempt(&same_prio_task, &high_task);
    TEST_ASSERT(!should_preempt,
               "Same priority task with time slice should not preempt");

    /* Test 4: Same priority - preempt if current has no time slice */
    high_task.sched_entity.time_slice = 0;
    should_preempt = prio_class->should_preempt(&same_prio_task, &high_task);
    TEST_ASSERT(should_preempt,
               "Same priority task should preempt when current has no time slice");

    TEST_PASS();
}

/**
 * @brief Test 8: Priority Time Slice Calculation
 */
static bool test_priority_time_slice(void) {
    TEST_START("Priority Time Slice Calculation");

    sched_class_t* prio_class = sched_prio_get_class();

    pcb_t high_task, low_task;
    memset(&high_task, 0, sizeof(pcb_t));
    memset(&low_task, 0, sizeof(pcb_t));

    INIT_LIST_HEAD(&high_task.sched_entity.run_list);
    INIT_LIST_HEAD(&low_task.sched_entity.run_list);

    high_task.sched_entity.sched_class = prio_class;
    high_task.sched_entity.policy = SCHED_PRIORITY;
    high_task.sched_entity.priority = 32;   /* High priority */

    low_task.sched_entity.sched_class = prio_class;
    low_task.sched_entity.policy = SCHED_PRIORITY;
    low_task.sched_entity.priority = 96;    /* Low priority */

    /* High priority gets shorter time slice (5ms) */
    uint32_t high_slice = prio_class->get_time_slice(&high_task);

    /* Low priority gets longer time slice (20ms) */
    uint32_t low_slice = prio_class->get_time_slice(&low_task);

    TEST_ASSERT(high_slice == 5, "High priority task should get 5ms time slice");
    TEST_ASSERT(low_slice == 20, "Low priority task should get 20ms time slice");
    TEST_ASSERT(high_slice < low_slice,
               "High priority task should get shorter time slice");

    TEST_PASS();
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

/**
 * @brief Test 9: Scheduler Framework Integration
 */
static bool test_scheduler_integration(void) {
    TEST_START("Scheduler Framework Integration");

    extern scheduler_t scheduler;

    /* Check if scheduler has been initialized by checking if classes are set */
    /* Note: This test may run before proc_init(), so we check conditionally */
    if (scheduler.classes == NULL || scheduler.rq == NULL) {
        klog_info("[SCHED_DEMO] Scheduler not yet initialized (will be initialized in proc_init)\n");
        TEST_PASS();
        return true;
    }

    /* Verify scheduler structure */
    TEST_ASSERT(scheduler.rq != NULL, "Scheduler run queues should be initialized");
    TEST_ASSERT(scheduler.classes != NULL, "Scheduler classes array should be initialized");

    /* Verify RR class is registered */
    TEST_ASSERT(scheduler.classes[SCHED_NORMAL] != NULL,
               "RR class should be registered");
    TEST_ASSERT(scheduler.rq[SCHED_NORMAL].sched_class != NULL,
               "RR run queue should have sched_class");

    /* Verify Priority class is registered */
    TEST_ASSERT(scheduler.classes[SCHED_PRIORITY] != NULL,
               "Priority class should be registered");
    TEST_ASSERT(scheduler.rq[SCHED_PRIORITY].sched_class != NULL,
               "Priority run queue should have sched_class");

    TEST_PASS();
}

/**
 * @brief Test 10: Task Tick Integration
 */
static bool test_task_tick_integration(void) {
    TEST_START("Task Tick Integration");

    sched_class_t* rr_class = sched_rr_get_class();

    pcb_t task;
    memset(&task, 0, sizeof(pcb_t));
    INIT_LIST_HEAD(&task.sched_entity.run_list);
    task.sched_entity.sched_class = rr_class;
    task.sched_entity.policy = SCHED_NORMAL;
    task.sched_entity.time_slice = 3;  /* Small time slice for testing */
    task.sched_entity.time_slice_total = 10;

    extern scheduler_t scheduler;
    sched_rq_t* rq = &scheduler.rq[SCHED_NORMAL];

    /* Simulate 3 timer ticks */
    for (int i = 0; i < 3; i++) {
        rr_class->task_tick(rq, &task);
    }

    /* Verify time slice decreased */
    TEST_ASSERT(task.sched_entity.time_slice == 0,
               "Time slice should be 0 after 3 ticks");

    TEST_PASS();
}

/* ============================================================================
 * Public API - Round-Robin Demo
 * ============================================================================ */

int sched_run_rr_demo(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Round-Robin Scheduling Demo             ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    tests_passed = 0;
    tests_failed = 0;

    /* Run RR tests */
    test_rr_class_init();
    test_rr_time_slice_constants();
    test_rr_should_preempt();
    test_rr_get_time_slice();
    test_scheduler_integration();
    test_task_tick_integration();

    /* Print summary */
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   RR Demo Summary                         ║\n");
    klog_info("╚════════════════════════════════════════╝\n");
    klog_info("[SCHED_DEMO] Tests passed: %d\n", tests_passed);
    klog_info("[SCHED_DEMO] Tests failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        klog_info("[SCHED_DEMO] ✓ All RR tests PASSED\n");
        return 0;
    } else {
        klog_error("[SCHED_DEMO] ✗ %d RR test(s) FAILED\n", tests_failed);
        return -1;
    }
}

void sched_stop_rr_demo(void) {
    klog_info("[SCHED_DEMO] Stopping RR demo...\n");
}

/* ============================================================================
 * Public API - Priority Demo
 * ============================================================================ */

int sched_run_prio_demo(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Priority Scheduling Demo                ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    tests_passed = 0;
    tests_failed = 0;

    /* Run Priority tests */
    test_prio_class_init();
    test_priority_constants();
    test_priority_should_preempt();
    test_priority_time_slice();
    test_scheduler_integration();

    /* Print summary */
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Priority Demo Summary                   ║\n");
    klog_info("╚════════════════════════════════════════╝\n");
    klog_info("[SCHED_DEMO] Tests passed: %d\n", tests_passed);
    klog_info("[SCHED_DEMO] Tests failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        klog_info("[SCHED_DEMO] ✓ All Priority tests PASSED\n");
        return 0;
    } else {
        klog_error("[SCHED_DEMO] ✗ %d Priority test(s) FAILED\n", tests_failed);
        return -1;
    }
}

void sched_stop_prio_demo(void) {
    klog_info("[SCHED_DEMO] Stopping Priority demo...\n");
}
