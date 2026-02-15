/**
 * @file host_support.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Host-side support for kernel static testing
 * @version 0.2
 * @date 2026-02-15
 *
 * @copyright Copyright (c) 2026
 *
 * ============================================================================
 * Design Rationale:
 * ============================================================================
 *
 * This header provides host-side testing support for kernel code. When running
 * static tests, we use a hosted environment (with standard library) instead of
 * the freestanding kernel environment. This allows us to:
 *
 * 1. Use printf() for test output and debugging
 * 2. Validate static assertions and type sizes
 * 3. Catch compilation issues before kernel integration
 * 4. Maintain strict compilation standards matching the kernel
 *
 * The key principle is: TEST CODE USES HOSTED ENV, BUT KERNEL CODE REMAINS
 * FREESTANDING. The test framework wraps kernel headers and validates them
 * without changing the kernel's compilation model.
 */
#pragma once

#include <stdio.h>
#include <stdbool.h>

// ============================================================================
#// Color Output for Better Test Visibility
// ============================================================================
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"

// Disable colors on Windows or when not a TTY
#if defined(_WIN32) || !defined(__unix__)
    #define COLOR_RESET ""
    #define COLOR_RED ""
    #define COLOR_GREEN ""
    #define COLOR_YELLOW ""
    #define COLOR_BLUE ""
    #define COLOR_MAGENTA ""
    #define COLOR_CYAN ""
#endif

// ============================================================================
// Test Section Macros
// ============================================================================

/**
 * STATIC_TEST_SECTION - Declare a logical group of static tests
 * @param SectionName: Identifier for this test section (must be valid C identifier)
 *
 * Usage:
 *   STATIC_TEST_SECTION(type_sizes);
 *   STATIC_ASSERT(sizeof(int8_t) == 1, int8_size);
 *   STATIC_TEST_SECTION_END(type_sizes);
 */
#define STATIC_TEST_SECTION(SectionName) \
    void __test_section_begin_##SectionName(void) { \
        printf(COLOR_CYAN "[TEST] " COLOR_RESET "Starting section: %s\n", #SectionName); \
    }

/**
 * STATIC_TEST_SECTION_END - Mark the end of a test section
 * @param SectionName: Must match the corresponding STATIC_TEST_SECTION call
 */
#define STATIC_TEST_SECTION_END(SectionName) \
    void __test_section_end_##SectionName(void) { \
        printf(COLOR_GREEN "[PASS] " COLOR_RESET "Section completed: %s\n", #SectionName); \
    }

// ============================================================================
// Test Assertion Macros
// ============================================================================

/**
 * TEST_COMPILE_TIME - Verify a compile-time constant expression
 * @param expr: Constant expression to verify
 * @param name: Unique identifier for this assertion
 *
 * This uses _Static_assert for compile-time validation.
 */
#define TEST_COMPILE_TIME(expr, name) \
    _Static_assert((expr), "Compile-time test failed: " #name)

/**
 * TEST_ASSERT_SIZEOF - Verify type size at compile time
 * @param type: The type to check
 * @param expected_size: Expected size in bytes
 * @param name: Unique identifier
 */
#define TEST_ASSERT_SIZEOF(type, expected_size, name) \
    TEST_COMPILE_TIME(sizeof(type) == (expected_size), name##_sizeof)

/**
 * TEST_ASSERT_ALIGN - Verify type alignment at compile time
 * @param type: The type to check
 * @param expected_align: Expected alignment in bytes
 * @param name: Unique identifier
 */
#define TEST_ASSERT_ALIGN(type, expected_align, name) \
    TEST_COMPILE_TIME(_Alignof(type) == (expected_align), name##_align)

// ============================================================================
// Test Runner Support
// ============================================================================

/**
 * TEST_RUNNER_BEGIN - Entry point for test execution
 * @param test_suite_name: Name of the test suite
 *
 * Place at the beginning of main() after any setup.
 */
#define TEST_RUNNER_BEGIN(test_suite_name) \
    printf("\n"); \
    printf("========================================\n"); \
    printf(COLOR_MAGENTA "CCOS Kernel Static Test Suite" COLOR_RESET "\n"); \
    printf("Suite: %s\n", test_suite_name); \
    printf("========================================\n\n"); \
    int test_passed = 0; \
    int test_failed = 0; \
    (void)test_passed; \
    (void)test_failed;

/**
 * TEST_RUNNER_END - Exit point for test execution
 *
 * Place at the end of main() before return.
 */
#define TEST_RUNNER_END() \
    printf("\n"); \
    printf("========================================\n"); \
    if (test_failed == 0) { \
        printf(COLOR_GREEN "ALL TESTS PASSED" COLOR_RESET "\n"); \
    } else { \
        printf(COLOR_RED "SOME TESTS FAILED" COLOR_RESET "\n"); \
    } \
    printf("========================================\n"); \
    return (test_failed == 0) ? 0 : 1;

// ============================================================================
// Utility Macros
// ============================================================================

/**
 * TEST_INFO - Print informational message during test
 */
#define TEST_INFO(fmt, ...) \
    printf(COLOR_BLUE "[INFO] " COLOR_RESET fmt "\n", ##__VA_ARGS__)

/**
 * TEST_WARN - Print warning during test (non-fatal)
 */
#define TEST_WARN(fmt, ...) \
    printf(COLOR_YELLOW "[WARN] " COLOR_RESET fmt "\n", ##__VA_ARGS__)

/**
 * TEST_PASS - Explicitly mark a test as passed
 */
#define TEST_PASS(name) \
    do { \
        test_passed++; \
        printf(COLOR_GREEN "[PASS] " COLOR_RESET "%s\n", name); \
    } while(0)

/**
 * TEST_FAIL - Explicitly mark a test as failed
 */
#define TEST_FAIL(name, reason) \
    do { \
        test_failed++; \
        printf(COLOR_RED "[FAIL] " COLOR_RESET "%s: %s\n", name, reason); \
    } while(0)

// ============================================================================
// Legacy Compatibility Macros
// ============================================================================
// These maintain compatibility with existing test code
#define STATIC_TEST_OK(SectionName) \
    printf(COLOR_GREEN "[OK] " COLOR_RESET "Test section: %s\n", #SectionName)





