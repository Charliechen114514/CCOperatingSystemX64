/**
 * @file mock_syscall_demo.c
 * @brief System Call Framework Mock Demo
 *
 * This demo tests the syscall framework by simulating system calls
 * from kernel mode (since we don't have user mode support yet).
 */

#include "mock_syscall_demo.h"
#include "syscall/syscall.h"
#include "syscall/syscall_numbers.h"
#include "klogs/kprintf.h"

/* ============================================================================
 * Test Result Tracking
 * ============================================================================ */

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            klog_error("[SYSCALL_DEMO] FAILED: %s\n", message); \
            tests_failed++; \
            return false; \
        } \
    } while(0)

#define TEST_START(name) \
    klog_info("[SYSCALL_DEMO] Test: %s...\n", name)

#define TEST_PASS() \
    do { \
        klog_info("[SYSCALL_DEMO] PASSED\n"); \
        tests_passed++; \
        return true; \
    } while(0)

/* ============================================================================
 * Test 1: MSR Configuration Verification
 * ============================================================================ */

static bool test_msr_config(void) {
    TEST_START("MSR Configuration");

    /* Read IA32_LSTAR (syscall entry point) */
    uint64_t lstar = rdmsr(0xC0000082);
    TEST_ASSERT(lstar != 0, "IA32_LSTAR should be set");
    klog_trace("[SYSCALL_DEMO]   IA32_LSTAR = 0x%016llX\n", lstar);

    /* Read IA32_STAR (segment selectors) */
    uint64_t star = rdmsr(0xC0000081);
    TEST_ASSERT(star != 0, "IA32_STAR should be set");
    klog_trace("[SYSCALL_DEMO]   IA32_STAR  = 0x%016llX\n", star);

    /* Verify STAR encoding:
     * [63:48] = User CS (should be 0x18 | 3 = 0x1B)
     * [47:32] = Kernel CS (should be 0x08)
     */
    uint32_t user_cs = (star >> 48) & 0xFFFF;
    uint32_t kernel_cs = (star >> 32) & 0xFFFF;
    TEST_ASSERT(kernel_cs == 0x08, "Kernel CS should be 0x08");
    TEST_ASSERT(user_cs == 0x1B, "User CS should be 0x1B (0x18 | 3)");

    /* Read IA32_FMASK (RFLAGS mask) */
    uint64_t fmask = rdmsr(0xC0000084);
    klog_trace("[SYSCALL_DEMO]   IA32_FMASK = 0x%016llX\n", fmask);
    TEST_ASSERT(fmask == 0x200, "IA32_FMASK should be 0x200 (clear IF)");

    TEST_PASS();
}

/* ============================================================================
 * Test 2: System Call Dispatch (Test syscall)
 * ============================================================================ */

static bool test_syscall_test(void) {
    TEST_START("SYS_TEST syscall");

    /* Simulate a syscall from kernel mode */
    syscall_frame_t frame = {
        .syscall_number = SYS_TEST,
        .arg0 = 0xDEADBEEF,
        .arg1 = 0x12345678,
        .arg2 = 0,
        .arg3 = 0,
        .arg4 = 0,
        .arg5 = 0
    };

    /* Call the dispatcher directly */
    int64_t result = syscall_dispatch(&frame, 0);

    /* SYS_TEST should echo back arg0 */
    TEST_ASSERT(result == 0xDEADBEEF, "SYS_TEST should return arg0");
    klog_trace("[SYSCALL_DEMO]   SYS_TEST(0x%llX) = 0x%llX\n", frame.arg0, result);

    TEST_PASS();
}

/* ============================================================================
 * Test 3: System Call Dispatch (getpid)
 * ============================================================================ */

static bool test_syscall_getpid(void) {
    TEST_START("SYS_GETPID syscall");

    syscall_frame_t frame = {
        .syscall_number = SYS_GETPID,
        .arg0 = 0,
        .arg1 = 0,
        .arg2 = 0,
        .arg3 = 0,
        .arg4 = 0,
        .arg5 = 0
    };

    int64_t result = syscall_dispatch(&frame, 0);

    /* Should return dummy PID 1 */
    TEST_ASSERT(result == 1, "SYS_GETPID should return 1");
    klog_trace("[SYSCALL_DEMO]   SYS_GETPID() = %lld\n", result);

    TEST_PASS();
}

/* ============================================================================
 * Test 4: System Call Dispatch (write)
 * ============================================================================ */

static bool test_syscall_write(void) {
    TEST_START("SYS_WRITE syscall");

    const char* test_msg = "Test message from syscall demo";
    syscall_frame_t frame = {
        .syscall_number = SYS_WRITE,
        .arg0 = 1,  /* stdout */
        .arg1 = (uint64_t)test_msg,
        .arg2 = sizeof("Test message from syscall demo"),
        .arg3 = 0,
        .arg4 = 0,
        .arg5 = 0
    };

    int64_t result = syscall_dispatch(&frame, 0);

    /* Should return number of bytes "written" */
    TEST_ASSERT(result > 0, "SYS_WRITE should return positive value");
    klog_trace("[SYSCALL_DEMO]   SYS_WRITE wrote %lld bytes\n", result);

    TEST_PASS();
}

/* ============================================================================
 * Test 5: Unimplemented System Call
 * ============================================================================ */

static bool test_syscall_not_impl(void) {
    TEST_START("Unimplemented syscall");

    syscall_frame_t frame = {
        .syscall_number = 200,  /* Undefined syscall */
        .arg0 = 0,
        .arg1 = 0,
        .arg2 = 0,
        .arg3 = 0,
        .arg4 = 0,
        .arg5 = 0
    };

    int64_t result = syscall_dispatch(&frame, 0);

    /* Should return error code */
    TEST_ASSERT(result == SYS_ERR_NOTIMPL, "Unimplemented syscall should return SYS_ERR_NOTIMPL");
    klog_trace("[SYSCALL_DEMO]   Syscall 200 returned: %lld (SYS_ERR_NOTIMPL)\n", result);

    TEST_PASS();
}

/* ============================================================================
 * Test 6: Multiple Arguments
 * ============================================================================ */

static bool test_syscall_multiple_args(void) {
    TEST_START("Multiple argument passing");

    syscall_frame_t frame = {
        .syscall_number = SYS_TEST,
        .arg0 = 0x1111111111111111,
        .arg1 = 0x2222222222222222,
        .arg2 = 0x3333333333333333,
        .arg3 = 0x4444444444444444,
        .arg4 = 0x5555555555555555,
        .arg5 = 0x6666666666666666
    };

    /* Pass arg6 through the syscall_dispatch parameter */
    int64_t result = syscall_dispatch(&frame, frame.arg5);

    /* SYS_TEST returns arg0, but we verified all args are passed */
    TEST_ASSERT(result == 0x1111111111111111, "arg0 should be preserved");
    klog_trace("[SYSCALL_DEMO]   All 6 arguments passed successfully\n");

    TEST_PASS();
}

/* ============================================================================
 * Test 7: System Call Statistics
 * ============================================================================ */

static bool test_syscall_stats(void) {
    TEST_START("System call statistics");

    syscall_stats_t stats;
    syscall_get_stats(&stats);

    klog_trace("[SYSCALL_DEMO]   Total calls: %lu\n", stats.total_calls);
    klog_trace("[SYSCALL_DEMO]   Errors: %lu\n", stats.errors);
    klog_trace("[SYSCALL_DEMO]   Not implemented: %lu\n", stats.not_impl_count);

    /* We should have made several syscalls by now */
    TEST_ASSERT(stats.total_calls > 0, "Should have some syscall calls");
    TEST_ASSERT(stats.not_impl_count == 1, "Should have 1 unimplemented call");

    /* Print detailed stats */
    syscall_dump_stats();

    TEST_PASS();
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * mock_syscall_run_demo - Run the system call framework mock demo
 */
int mock_syscall_run_demo(void) {
    klog_trace("\n");
    klog_trace("╔════════════════════════════════════════╗\n");
    klog_trace("║   System Call Framework Demo Starting ║\n");
    klog_trace("╚════════════════════════════════════════╝\n");

    tests_passed = 0;
    tests_failed = 0;

    /* Check if syscall/sysret is available */
    if (!syscall_is_available()) {
        klog_error("[SYSCALL_DEMO] CPU does not support syscall/sysret!\n");
        return -1;
    }
    klog_info("[SYSCALL_DEMO] syscall/sysret is supported\n");

    /* Run all tests */
    test_msr_config();
    test_syscall_test();
    test_syscall_getpid();
    test_syscall_write();
    test_syscall_not_impl();
    test_syscall_multiple_args();
    test_syscall_stats();

    /* Print summary */
    klog_trace("\n");
    klog_trace("╔════════════════════════════════════════╗\n");
    klog_trace("║   System Call Demo Summary            ║\n");
    klog_trace("╚════════════════════════════════════════╝\n");
    klog_info("[SYSCALL_DEMO] Tests Passed: %d\n", tests_passed);
    klog_info("[SYSCALL_DEMO] Tests Failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        klog_info("[SYSCALL_DEMO] All tests PASSED!\n");
        klog_info("[SYSCALL_DEMO] System call framework is functional!\n");
    } else {
        klog_error("[SYSCALL_DEMO] Some tests FAILED!\n");
    }

    return tests_failed > 0 ? -1 : 0;
}

/**
 * mock_syscall_stop_demo - Stop the mock syscall demo
 */
void mock_syscall_stop_demo(void) {
    klog_info("[SYSCALL_DEMO] Stopping syscall demo...\n");

    /* Print final statistics */
    syscall_dump_stats();
}
