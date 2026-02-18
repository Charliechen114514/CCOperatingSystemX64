/**
 * @file process_demo.c
 * @brief Process Management Demo - Demonstrates process management functionality
 */

#include "process_demo.h"
#include "process/process.h"
#include "mm/heap/heap.h"
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
            klog_error("[PROC_DEMO] FAILED: %s\n", message); \
            tests_failed++; \
            return false; \
        } \
    } while(0)

#define TEST_START(name) \
    klog_info("[PROC_DEMO] Test: %s...\n", name)

#define TEST_PASS() \
    do { \
        klog_info("[PROC_DEMO] PASSED\n"); \
        tests_passed++; \
        return true; \
    } while(0)

/* ============================================================================
 * Test 1: PID Allocation
 * ============================================================================ */

static bool test_pid_allocation(void) {
    TEST_START("PID Allocation");

    /* Initialize PID allocator */
    pid_alloc_init();

    /* Allocate some PIDs */
    int32_t pid1 = pid_alloc();
    TEST_ASSERT(pid1 > 0, "First PID allocation failed");

    int32_t pid2 = pid_alloc();
    TEST_ASSERT(pid2 > 0 && pid2 != pid1, "Second PID allocation failed or duplicate");

    /* Free first PID */
    pid_free(pid1);

    /* Allocate again - should get the same PID back */
    int32_t pid3 = pid_alloc();
    TEST_ASSERT(pid3 == pid1, "PID reuse after free failed");

    TEST_PASS();
}

/* ============================================================================
 * Test 2: PCB Allocation
 * ============================================================================ */

static bool test_pcb_allocation(void) {
    TEST_START("PCB Allocation");

    /* Allocate a PCB */
    pcb_t* pcb = (pcb_t*)kmalloc(sizeof(pcb_t));
    TEST_ASSERT(pcb != NULL, "PCB allocation failed");

    /* Initialize basic fields */
    memset(pcb, 0, sizeof(pcb_t));
    pcb->pid = 100;
    pcb->ppid = 1;
    pcb->state = PROC_READY;

    /* Verify fields */
    TEST_ASSERT(pcb->pid == 100, "PID not set correctly");
    TEST_ASSERT(pcb->state == PROC_READY, "State not set correctly");

    /* Free PCB */
    kfree(pcb);

    TEST_PASS();
}

/* ============================================================================
 * Test 3: Process State Transitions
 * ============================================================================ */

static bool test_process_states(void) {
    TEST_START("Process State Transitions");

    /* Create a test PCB */
    pcb_t* pcb = (pcb_t*)kmalloc(sizeof(pcb_t));
    TEST_ASSERT(pcb != NULL, "PCB allocation failed");
    memset(pcb, 0, sizeof(pcb_t));

    /* Test state transitions */
    pcb->state = PROC_READY;
    TEST_ASSERT(pcb->state == PROC_READY, "Ready state not set");

    pcb->state = PROC_RUNNING;
    TEST_ASSERT(pcb->state == PROC_RUNNING, "Running state not set");

    pcb->state = PROC_BLOCKED;
    TEST_ASSERT(pcb->state == PROC_BLOCKED, "Blocked state not set");

    pcb->state = PROC_ZOMBIE;
    TEST_ASSERT(pcb->state == PROC_ZOMBIE, "Zombie state not set");

    kfree(pcb);

    TEST_PASS();
}

/* ============================================================================
 * Test 4: Scheduler Initialization
 * ============================================================================ */

static bool test_scheduler_init(void) {
    TEST_START("Scheduler Initialization");

    /* Initialize process subsystem */
    int result = proc_init();
    TEST_ASSERT(result == 0, "Process initialization failed");

    /* Verify scheduler is initialized */
    extern scheduler_t scheduler;
    TEST_ASSERT(scheduler.current == NULL, "Current process should be NULL after init");
    TEST_ASSERT(list_is_empty(&scheduler.run_queue), "Run queue should be empty after init");
    TEST_ASSERT(scheduler.nr_running == 0, "Running count should be 0 after init");

    TEST_PASS();
}

/* ============================================================================
 * Test 5: PCB List Management
 * ============================================================================ */

static bool test_pcb_lists(void) {
    TEST_START("PCB List Management");

    /* Create test PCBs */
    pcb_t* pcb1 = (pcb_t*)kmalloc(sizeof(pcb_t));
    pcb_t* pcb2 = (pcb_t*)kmalloc(sizeof(pcb_t));
    TEST_ASSERT(pcb1 != NULL && pcb2 != NULL, "PCB allocation failed");

    memset(pcb1, 0, sizeof(pcb_t));
    memset(pcb2, 0, sizeof(pcb_t));

    INIT_LIST_HEAD(&pcb1->siblings);
    INIT_LIST_HEAD(&pcb2->siblings);
    INIT_LIST_HEAD(&pcb1->children);
    INIT_LIST_HEAD(&pcb2->children);
    INIT_LIST_HEAD(&pcb1->run_list);
    INIT_LIST_HEAD(&pcb2->run_list);

    /* Create a parent-child relationship */
    pcb1->pid = 1;
    pcb2->pid = 2;
    pcb2->parent = pcb1;
    pcb2->ppid = 1;

    /* Add child to parent's children list */
    list_add_tail(&pcb2->siblings, &pcb1->children);

    /* Verify relationship */
    TEST_ASSERT(!list_is_empty(&pcb1->children), "Children list should not be empty");

    /* Cleanup */
    list_del(&pcb2->siblings);
    kfree(pcb1);
    kfree(pcb2);

    TEST_PASS();
}

/* ============================================================================
 * Test 6: Kernel Stack Management
 * ============================================================================ */

static bool test_kernel_stack(void) {
    TEST_START("Kernel Stack Management");

    /* Allocate PCB with kernel stack */
    pcb_t* pcb = (pcb_t*)kmalloc(sizeof(pcb_t));
    TEST_ASSERT(pcb != NULL, "PCB allocation failed");
    memset(pcb, 0, sizeof(pcb_t));

    /* Allocate kernel stack */
    pcb->kernel_stack_base = (virtual_addr_t)kmalloc(KERNEL_STACK_SIZE);
    TEST_ASSERT(pcb->kernel_stack_base != 0, "Kernel stack allocation failed");

    /* Set stack top (stack grows down) */
    pcb->kernel_stack = pcb->kernel_stack_base + KERNEL_STACK_SIZE;

    /* Verify stack is properly aligned */
    TEST_ASSERT((pcb->kernel_stack & 0xF) == 0, "Kernel stack not 16-byte aligned");

    /* Cleanup */
    kfree((void*)pcb->kernel_stack_base);
    kfree(pcb);

    TEST_PASS();
}

/* ============================================================================
 * Test 7: Memory Context
 * ============================================================================ */

static bool test_memory_context(void) {
    TEST_START("Memory Context");

    /* Create PCB with memory context */
    pcb_t* pcb = (pcb_t*)kmalloc(sizeof(pcb_t));
    TEST_ASSERT(pcb != NULL, "PCB allocation failed");
    memset(pcb, 0, sizeof(pcb_t));

    /* Initialize memory context */
    pcb->mm.pml4_phys = 0x1000;
    pcb->mm.brk = 0x400000;
    pcb->mm.stack_start = USER_STACK;

    /* Verify values */
    TEST_ASSERT(pcb->mm.pml4_phys == 0x1000, "PML4 not set correctly");
    TEST_ASSERT(pcb->mm.brk == 0x400000, "Break not set correctly");
    TEST_ASSERT(pcb->mm.stack_start == USER_STACK, "Stack start not set correctly");

    kfree(pcb);

    TEST_PASS();
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int process_run_demo(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Process Management Demo                   ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    tests_passed = 0;
    tests_failed = 0;

    /* Run all tests */
    test_pid_allocation();
    test_pcb_allocation();
    test_process_states();
    test_scheduler_init();
    test_pcb_lists();
    test_kernel_stack();
    test_memory_context();

    /* Print summary */
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   Process Demo Summary                     ║\n");
    klog_info("╚════════════════════════════════════════╝\n");
    klog_info("[PROC_DEMO] Tests passed: %d\n", tests_passed);
    klog_info("[PROC_DEMO] Tests failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        klog_info("[PROC_DEMO] ✓ All tests PASSED\n");
        return 0;
    } else {
        klog_error("[PROC_DEMO] ✗ %d test(s) FAILED\n", tests_failed);
        return -1;
    }
}

void process_stop_demo(void) {
    klog_info("[PROC_DEMO] Stopping process demo...\n");
    /* Cleanup if needed */
}
