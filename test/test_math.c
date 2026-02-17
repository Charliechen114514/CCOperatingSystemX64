/**
 * @file test_math.c
 * @brief Unit tests for math module (min/max macros)
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>

// Include kernel types and math header
#include "defines/types.h"
#include "math/math.h"

// Define global test counters
int g_test_passed = 0;
int g_test_failed = 0;

#include "host_support.h"

// ============================================================================
// Test min macro
// ============================================================================

static void test_min(void) {
    TEST_INFO("Testing min macro");

    // Basic integer tests
    TEST_ASSERT_EQ(min(5, 10), 5, "min: 5, 10 -> 5");
    TEST_ASSERT_EQ(min(10, 5), 5, "min: 10, 5 -> 5");
    TEST_ASSERT_EQ(min(0, 0), 0, "min: 0, 0 -> 0");

    // Negative numbers
    TEST_ASSERT_EQ(min(-5, 5), -5, "min: -5, 5 -> -5");
    TEST_ASSERT_EQ(min(-10, -5), -10, "min: -10, -5 -> -10");

    // Edge cases
    TEST_ASSERT_EQ(min(INT_MIN, INT_MAX), INT_MIN, "min: INT_MIN, INT_MAX -> INT_MIN");
    TEST_ASSERT_EQ(min(INT_MAX, INT_MIN), INT_MIN, "min: INT_MAX, INT_MIN -> INT_MIN");

    // Same values
    TEST_ASSERT_EQ(min(42, 42), 42, "min: 42, 42 -> 42");

    // Unsigned types
    TEST_ASSERT_EQ(min(5U, 10U), 5U, "min: unsigned 5, 10 -> 5");
    TEST_ASSERT_EQ(min(0U, 100U), 0U, "min: unsigned 0, 100 -> 0");

    // Size type
    TEST_ASSERT_EQ(min((size_t)100, (size_t)200), (size_t)100, "min: size_t 100, 200 -> 100");

    // Side effects - macros should evaluate each parameter once
    int count = 0;
    int result = min(++count, 10);
    TEST_ASSERT_EQ(result, 1, "min: side effect result");
    TEST_ASSERT_EQ(count, 1, "min: side effect count");

    TEST_PASS("min macro");
}

// ============================================================================
// Test max macro
// ============================================================================

static void test_max(void) {
    TEST_INFO("Testing max macro");

    // Basic integer tests
    TEST_ASSERT_EQ(max(5, 10), 10, "max: 5, 10 -> 10");
    TEST_ASSERT_EQ(max(10, 5), 10, "max: 10, 5 -> 10");
    TEST_ASSERT_EQ(max(0, 0), 0, "max: 0, 0 -> 0");

    // Negative numbers
    TEST_ASSERT_EQ(max(-5, 5), 5, "max: -5, 5 -> 5");
    TEST_ASSERT_EQ(max(-10, -5), -5, "max: -10, -5 -> -5");

    // Edge cases
    TEST_ASSERT_EQ(max(INT_MIN, INT_MAX), INT_MAX, "max: INT_MIN, INT_MAX -> INT_MAX");
    TEST_ASSERT_EQ(max(INT_MAX, INT_MIN), INT_MAX, "max: INT_MAX, INT_MIN -> INT_MAX");

    // Same values
    TEST_ASSERT_EQ(max(42, 42), 42, "max: 42, 42 -> 42");

    // Unsigned types
    TEST_ASSERT_EQ(max(5U, 10U), 10U, "max: unsigned 5, 10 -> 10");
    TEST_ASSERT_EQ(max(0U, 100U), 100U, "max: unsigned 0, 100 -> 100");

    // Size type
    TEST_ASSERT_EQ(max((size_t)100, (size_t)200), (size_t)200, "max: size_t 100, 200 -> 200");

    // Side effects - macros should evaluate each parameter once
    int count = 0;
    int result = max(++count, 0);
    TEST_ASSERT_EQ(result, 1, "max: side effect result");
    TEST_ASSERT_EQ(count, 1, "max: side effect count");

    TEST_PASS("max macro");
}

// ============================================================================
// Test min/max combinations
// ============================================================================

static void test_minmax_combinations(void) {
    TEST_INFO("Testing min/max combinations");

    // Nested operations
    TEST_ASSERT_EQ(min(max(1, 5), 10), 5, "minmax: min(max(1,5), 10) -> 5");
    TEST_ASSERT_EQ(max(min(1, 5), 10), 10, "minmax: max(min(1,5), 10) -> 10");
    TEST_ASSERT_EQ(min(max(1, 10), max(5, 8)), 8, "minmax: min(max(1,10), max(5,8)) -> 8");
    TEST_ASSERT_EQ(max(min(1, 10), min(5, 8)), 5, "minmax: max(min(1,10), min(5,8)) -> 5");

    // Clamp operation using min/max: clamp(x, lo, hi) = max(min(x, hi), lo)
    int x = 15;
    int lo = 10;
    int hi = 20;
    TEST_ASSERT_EQ(max(min(x, hi), lo), 15, "minmax: clamp(15, 10, 20) -> 15");

    x = 5;
    TEST_ASSERT_EQ(max(min(x, hi), lo), 10, "minmax: clamp(5, 10, 20) -> 10");

    x = 25;
    TEST_ASSERT_EQ(max(min(x, hi), lo), 20, "minmax: clamp(25, 10, 20) -> 20");

    // Absolute value using min/max: abs(x) = max(x, -x) for signed ints
    TEST_ASSERT_EQ(max(5, -5), 5, "minmax: abs(5) -> 5");
    TEST_ASSERT_EQ(max(-5, 5), 5, "minmax: abs(-5) -> 5");

    TEST_PASS("min/max combinations");
}

// ============================================================================
// Test edge cases
// ============================================================================

static void test_edge_cases(void) {
    TEST_INFO("Testing edge cases");

    // Char type
    TEST_ASSERT_EQ(min('a', 'z'), 'a', "min: char 'a', 'z' -> 'a'");
    TEST_ASSERT_EQ(max('a', 'z'), 'z', "max: char 'a', 'z' -> 'z'");

    // Long long
    TEST_ASSERT_EQ(min(1000LL, 2000LL), 1000LL, "min: long long 1000, 2000 -> 1000");
    TEST_ASSERT_EQ(max(1000LL, 2000LL), 2000LL, "max: long long 1000, 2000 -> 2000");

    // Mixed signed/unsigned (should work due to promotion)
    TEST_ASSERT_TRUE(min(5, 10U) == 5, "min: mixed signed/unsigned");
    TEST_ASSERT_TRUE(max(5U, 10) == 10U, "max: mixed signed/unsigned");

    TEST_PASS("edge cases");
}

// ============================================================================
// Test abs macro
// ============================================================================

static void test_abs(void) {
    TEST_INFO("Testing abs macro");

    // Positive numbers
    TEST_ASSERT_EQ(abs(5), 5, "abs: 5 -> 5");
    TEST_ASSERT_EQ(abs(100), 100, "abs: 100 -> 100");
    TEST_ASSERT_EQ(abs(1), 1, "abs: 1 -> 1");

    // Negative numbers
    TEST_ASSERT_EQ(abs(-5), 5, "abs: -5 -> 5");
    TEST_ASSERT_EQ(abs(-100), 100, "abs: -100 -> 100");
    TEST_ASSERT_EQ(abs(-1), 1, "abs: -1 -> 1");

    // Zero
    TEST_ASSERT_EQ(abs(0), 0, "abs: 0 -> 0");

    // Edge cases - note: abs(INT_MIN) overflows with this macro implementation
    // We skip testing INT_MIN to avoid compiler warnings
    TEST_ASSERT_EQ(abs(INT_MAX), INT_MAX, "abs: INT_MAX -> INT_MAX");

    // Expression
    TEST_ASSERT_EQ(abs(5 - 10), 5, "abs: 5-10 -> 5");
    TEST_ASSERT_EQ(abs(10 - 5), 5, "abs: 10-5 -> 5");

    TEST_PASS("abs macro");
}

// ============================================================================
// Test clamp function
// ============================================================================

static void test_clamp(void) {
    TEST_INFO("Testing clamp function");

    // Value within range - should remain unchanged
    TEST_ASSERT_EQ(clamp(15, 10, 20), 15, "clamp: 15 in [10,20] -> 15");
    TEST_ASSERT_EQ(clamp(10, 10, 20), 10, "clamp: 10 at min boundary -> 10");
    TEST_ASSERT_EQ(clamp(20, 10, 20), 20, "clamp: 20 at max boundary -> 20");

    // Value below minimum - should clamp to min
    TEST_ASSERT_EQ(clamp(5, 10, 20), 10, "clamp: 5 below [10,20] -> 10");
    TEST_ASSERT_EQ(clamp(-5, 0, 20), 0, "clamp: -5 below [0,20] -> 0");
    TEST_ASSERT_EQ(clamp(-100, -50, 50), -50, "clamp: -100 below [-50,50] -> -50");

    // Value above maximum - should clamp to max
    TEST_ASSERT_EQ(clamp(25, 10, 20), 20, "clamp: 25 above [10,20] -> 20");
    TEST_ASSERT_EQ(clamp(100, 0, 50), 50, "clamp: 100 above [0,50] -> 50");
    TEST_ASSERT_EQ(clamp(100, -50, 50), 50, "clamp: 100 above [-50,50] -> 50");

    // Negative ranges
    TEST_ASSERT_EQ(clamp(-10, -20, -5), -10, "clamp: -10 in [-20,-5] -> -10");
    TEST_ASSERT_EQ(clamp(-25, -20, -5), -20, "clamp: -25 below [-20,-5] -> -20");
    TEST_ASSERT_EQ(clamp(0, -20, -5), -5, "clamp: 0 above [-20,-5] -> -5");

    // Zero-width range (min == max)
    TEST_ASSERT_EQ(clamp(10, 10, 10), 10, "clamp: 10 in [10,10] -> 10");
    TEST_ASSERT_EQ(clamp(5, 10, 10), 10, "clamp: 5 below [10,10] -> 10");
    TEST_ASSERT_EQ(clamp(15, 10, 10), 10, "clamp: 15 above [10,10] -> 10");

    TEST_PASS("clamp function");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
    TEST_RUNNER_BEGIN("Math Test Suite");

    TEST_INFO("Running min macro tests...");
    test_min();

    TEST_INFO("Running max macro tests...");
    test_max();

    TEST_INFO("Running min/max combination tests...");
    test_minmax_combinations();

    TEST_INFO("Running edge case tests...");
    test_edge_cases();

    TEST_INFO("Running abs macro tests...");
    test_abs();

    TEST_INFO("Running clamp function tests...");
    test_clamp();

    TEST_RUNNER_END();
}
