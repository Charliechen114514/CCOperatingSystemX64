/**
 * @file test_bits.c
 * @brief Unit tests for bits module (bit manipulation functions)
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>

// Include kernel types and bits header
#include "defines/types.h"
#include "math/bits.h"

// Define global test counters
int g_test_passed = 0;
int g_test_failed = 0;

#include "host_support.h"

// ============================================================================
// Test is_power_of_2
// ============================================================================

static void test_is_power_of_2(void) {
    TEST_INFO("Testing is_power_of_2");

    // Powers of 2
    TEST_ASSERT_TRUE(is_power_of_2(1UL), "is_power_of_2: 1 is power of 2");
    TEST_ASSERT_TRUE(is_power_of_2(2UL), "is_power_of_2: 2 is power of 2");
    TEST_ASSERT_TRUE(is_power_of_2(4UL), "is_power_of_2: 4 is power of 2");
    TEST_ASSERT_TRUE(is_power_of_2(8UL), "is_power_of_2: 8 is power of 2");
    TEST_ASSERT_TRUE(is_power_of_2(16UL), "is_power_of_2: 16 is power of 2");
    TEST_ASSERT_TRUE(is_power_of_2(32UL), "is_power_of_2: 32 is power of 2");
    TEST_ASSERT_TRUE(is_power_of_2(64UL), "is_power_of_2: 64 is power of 2");
    TEST_ASSERT_TRUE(is_power_of_2(128UL), "is_power_of_2: 128 is power of 2");
    TEST_ASSERT_TRUE(is_power_of_2(256UL), "is_power_of_2: 256 is power of 2");
    TEST_ASSERT_TRUE(is_power_of_2(512UL), "is_power_of_2: 512 is power of 2");
    TEST_ASSERT_TRUE(is_power_of_2(1024UL), "is_power_of_2: 1024 is power of 2");
    TEST_ASSERT_TRUE(is_power_of_2(0x80000000UL), "is_power_of_2: 0x80000000 is power of 2");

    // Not powers of 2
    TEST_ASSERT_FALSE(is_power_of_2(0UL), "is_power_of_2: 0 is NOT power of 2");
    TEST_ASSERT_FALSE(is_power_of_2(3UL), "is_power_of_2: 3 is NOT power of 2");
    TEST_ASSERT_FALSE(is_power_of_2(5UL), "is_power_of_2: 5 is NOT power of 2");
    TEST_ASSERT_FALSE(is_power_of_2(6UL), "is_power_of_2: 6 is NOT power of 2");
    TEST_ASSERT_FALSE(is_power_of_2(7UL), "is_power_of_2: 7 is NOT power of 2");
    TEST_ASSERT_FALSE(is_power_of_2(9UL), "is_power_of_2: 9 is NOT power of 2");
    TEST_ASSERT_FALSE(is_power_of_2(15UL), "is_power_of_2: 15 is NOT power of 2");
    TEST_ASSERT_FALSE(is_power_of_2(100UL), "is_power_of_2: 100 is NOT power of 2");
    TEST_ASSERT_FALSE(is_power_of_2(0xFFFFFFFFUL), "is_power_of_2: 0xFFFFFFFF is NOT power of 2");

    TEST_PASS("is_power_of_2");
}

// ============================================================================
// Test round_up_to_power_of_2
// ============================================================================

static void test_round_up_to_power_of_2(void) {
    TEST_INFO("Testing round_up_to_power_of_2");

    // Already powers of 2
    TEST_ASSERT_EQ(round_up_to_power_of_2(1UL), 1UL, "round_up_pow2: 1 -> 1");
    TEST_ASSERT_EQ(round_up_to_power_of_2(2UL), 2UL, "round_up_pow2: 2 -> 2");
    TEST_ASSERT_EQ(round_up_to_power_of_2(4UL), 4UL, "round_up_pow2: 4 -> 4");
    TEST_ASSERT_EQ(round_up_to_power_of_2(8UL), 8UL, "round_up_pow2: 8 -> 8");
    TEST_ASSERT_EQ(round_up_to_power_of_2(16UL), 16UL, "round_up_pow2: 16 -> 16");
    TEST_ASSERT_EQ(round_up_to_power_of_2(32UL), 32UL, "round_up_pow2: 32 -> 32");
    TEST_ASSERT_EQ(round_up_to_power_of_2(64UL), 64UL, "round_up_pow2: 64 -> 64");
    TEST_ASSERT_EQ(round_up_to_power_of_2(128UL), 128UL, "round_up_pow2: 128 -> 128");
    TEST_ASSERT_EQ(round_up_to_power_of_2(256UL), 256UL, "round_up_pow2: 256 -> 256");
    TEST_ASSERT_EQ(round_up_to_power_of_2(512UL), 512UL, "round_up_pow2: 512 -> 512");
    TEST_ASSERT_EQ(round_up_to_power_of_2(1024UL), 1024UL, "round_up_pow2: 1024 -> 1024");

    // Round up to next power of 2
    TEST_ASSERT_EQ(round_up_to_power_of_2(3UL), 4UL, "round_up_pow2: 3 -> 4");
    TEST_ASSERT_EQ(round_up_to_power_of_2(5UL), 8UL, "round_up_pow2: 5 -> 8");
    TEST_ASSERT_EQ(round_up_to_power_of_2(6UL), 8UL, "round_up_pow2: 6 -> 8");
    TEST_ASSERT_EQ(round_up_to_power_of_2(7UL), 8UL, "round_up_pow2: 7 -> 8");
    TEST_ASSERT_EQ(round_up_to_power_of_2(9UL), 16UL, "round_up_pow2: 9 -> 16");
    TEST_ASSERT_EQ(round_up_to_power_of_2(15UL), 16UL, "round_up_pow2: 15 -> 16");
    TEST_ASSERT_EQ(round_up_to_power_of_2(17UL), 32UL, "round_up_pow2: 17 -> 32");
    TEST_ASSERT_EQ(round_up_to_power_of_2(31UL), 32UL, "round_up_pow2: 31 -> 32");
    TEST_ASSERT_EQ(round_up_to_power_of_2(33UL), 64UL, "round_up_pow2: 33 -> 64");
    TEST_ASSERT_EQ(round_up_to_power_of_2(100UL), 128UL, "round_up_pow2: 100 -> 128");
    TEST_ASSERT_EQ(round_up_to_power_of_2(1000UL), 1024UL, "round_up_pow2: 1000 -> 1024");

    // Edge cases
    TEST_ASSERT_EQ(round_up_to_power_of_2(0UL), 1UL, "round_up_pow2: 0 -> 1");

    TEST_PASS("round_up_to_power_of_2");
}

// ============================================================================
// Test round_down_to_power_of_2
// ============================================================================

static void test_round_down_to_power_of_2(void) {
    TEST_INFO("Testing round_down_to_power_of_2");

    // Already powers of 2
    TEST_ASSERT_EQ(round_down_to_power_of_2(1UL), 1UL, "round_down_pow2: 1 -> 1");
    TEST_ASSERT_EQ(round_down_to_power_of_2(2UL), 2UL, "round_down_pow2: 2 -> 2");
    TEST_ASSERT_EQ(round_down_to_power_of_2(4UL), 4UL, "round_down_pow2: 4 -> 4");
    TEST_ASSERT_EQ(round_down_to_power_of_2(8UL), 8UL, "round_down_pow2: 8 -> 8");
    TEST_ASSERT_EQ(round_down_to_power_of_2(16UL), 16UL, "round_down_pow2: 16 -> 16");
    TEST_ASSERT_EQ(round_down_to_power_of_2(32UL), 32UL, "round_down_pow2: 32 -> 32");
    TEST_ASSERT_EQ(round_down_to_power_of_2(64UL), 64UL, "round_down_pow2: 64 -> 64");
    TEST_ASSERT_EQ(round_down_to_power_of_2(128UL), 128UL, "round_down_pow2: 128 -> 128");
    TEST_ASSERT_EQ(round_down_to_power_of_2(256UL), 256UL, "round_down_pow2: 256 -> 256");
    TEST_ASSERT_EQ(round_down_to_power_of_2(512UL), 512UL, "round_down_pow2: 512 -> 512");
    TEST_ASSERT_EQ(round_down_to_power_of_2(1024UL), 1024UL, "round_down_pow2: 1024 -> 1024");

    // Round down to previous power of 2
    TEST_ASSERT_EQ(round_down_to_power_of_2(3UL), 2UL, "round_down_pow2: 3 -> 2");
    TEST_ASSERT_EQ(round_down_to_power_of_2(5UL), 4UL, "round_down_pow2: 5 -> 4");
    TEST_ASSERT_EQ(round_down_to_power_of_2(6UL), 4UL, "round_down_pow2: 6 -> 4");
    TEST_ASSERT_EQ(round_down_to_power_of_2(7UL), 4UL, "round_down_pow2: 7 -> 4");
    TEST_ASSERT_EQ(round_down_to_power_of_2(9UL), 8UL, "round_down_pow2: 9 -> 8");
    TEST_ASSERT_EQ(round_down_to_power_of_2(15UL), 8UL, "round_down_pow2: 15 -> 8");
    TEST_ASSERT_EQ(round_down_to_power_of_2(17UL), 16UL, "round_down_pow2: 17 -> 16");
    TEST_ASSERT_EQ(round_down_to_power_of_2(31UL), 16UL, "round_down_pow2: 31 -> 16");
    TEST_ASSERT_EQ(round_down_to_power_of_2(33UL), 32UL, "round_down_pow2: 33 -> 32");
    TEST_ASSERT_EQ(round_down_to_power_of_2(100UL), 64UL, "round_down_pow2: 100 -> 64");
    TEST_ASSERT_EQ(round_down_to_power_of_2(1000UL), 512UL, "round_down_pow2: 1000 -> 512");

    // Edge cases
    TEST_ASSERT_EQ(round_down_to_power_of_2(0UL), 0UL, "round_down_pow2: 0 -> 0");

    TEST_PASS("round_down_to_power_of_2");
}

// ============================================================================
// Test align_up
// ============================================================================

static void test_align_up(void) {
    TEST_INFO("Testing align_up");

    // Already aligned
    TEST_ASSERT_EQ(align_up(0UL, 8UL), 0UL, "align_up: 0 to 8 -> 0");
    TEST_ASSERT_EQ(align_up(8UL, 8UL), 8UL, "align_up: 8 to 8 -> 8");
    TEST_ASSERT_EQ(align_up(16UL, 8UL), 16UL, "align_up: 16 to 8 -> 16");
    TEST_ASSERT_EQ(align_up(1024UL, 1024UL), 1024UL, "align_up: 1024 to 1024 -> 1024");

    // Need alignment
    TEST_ASSERT_EQ(align_up(1UL, 8UL), 8UL, "align_up: 1 to 8 -> 8");
    TEST_ASSERT_EQ(align_up(5UL, 8UL), 8UL, "align_up: 5 to 8 -> 8");
    TEST_ASSERT_EQ(align_up(7UL, 8UL), 8UL, "align_up: 7 to 8 -> 8");
    TEST_ASSERT_EQ(align_up(9UL, 8UL), 16UL, "align_up: 9 to 8 -> 16");
    TEST_ASSERT_EQ(align_up(10UL, 16UL), 16UL, "align_up: 10 to 16 -> 16");
    TEST_ASSERT_EQ(align_up(15UL, 16UL), 16UL, "align_up: 15 to 16 -> 16");
    TEST_ASSERT_EQ(align_up(17UL, 16UL), 32UL, "align_up: 17 to 16 -> 32");

    // Page alignment (4096)
    TEST_ASSERT_EQ(align_up(0UL, 4096UL), 0UL, "align_up: 0 to 4096 -> 0");
    TEST_ASSERT_EQ(align_up(1UL, 4096UL), 4096UL, "align_up: 1 to 4096 -> 4096");
    TEST_ASSERT_EQ(align_up(4095UL, 4096UL), 4096UL, "align_up: 4095 to 4096 -> 4096");
    TEST_ASSERT_EQ(align_up(4096UL, 4096UL), 4096UL, "align_up: 4096 to 4096 -> 4096");
    TEST_ASSERT_EQ(align_up(4097UL, 4096UL), 8192UL, "align_up: 4097 to 4096 -> 8192");

    TEST_PASS("align_up");
}

// ============================================================================
// Test align_down
// ============================================================================

static void test_align_down(void) {
    TEST_INFO("Testing align_down");

    // Already aligned
    TEST_ASSERT_EQ(align_down(0UL, 8UL), 0UL, "align_down: 0 to 8 -> 0");
    TEST_ASSERT_EQ(align_down(8UL, 8UL), 8UL, "align_down: 8 to 8 -> 8");
    TEST_ASSERT_EQ(align_down(16UL, 8UL), 16UL, "align_down: 16 to 8 -> 16");
    TEST_ASSERT_EQ(align_down(1024UL, 1024UL), 1024UL, "align_down: 1024 to 1024 -> 1024");

    // Need alignment
    TEST_ASSERT_EQ(align_down(1UL, 8UL), 0UL, "align_down: 1 to 8 -> 0");
    TEST_ASSERT_EQ(align_down(5UL, 8UL), 0UL, "align_down: 5 to 8 -> 0");
    TEST_ASSERT_EQ(align_down(7UL, 8UL), 0UL, "align_down: 7 to 8 -> 0");
    TEST_ASSERT_EQ(align_down(9UL, 8UL), 8UL, "align_down: 9 to 8 -> 8");
    TEST_ASSERT_EQ(align_down(10UL, 16UL), 0UL, "align_down: 10 to 16 -> 0");
    TEST_ASSERT_EQ(align_down(15UL, 16UL), 0UL, "align_down: 15 to 16 -> 0");
    TEST_ASSERT_EQ(align_down(17UL, 16UL), 16UL, "align_down: 17 to 16 -> 16");
    TEST_ASSERT_EQ(align_down(31UL, 16UL), 16UL, "align_down: 31 to 16 -> 16");

    TEST_PASS("align_down");
}

// ============================================================================
// Test is_aligned
// ============================================================================

static void test_is_aligned(void) {
    TEST_INFO("Testing is_aligned");

    // Aligned values
    TEST_ASSERT_TRUE(is_aligned(0UL, 8UL), "is_aligned: 0 to 8 -> true");
    TEST_ASSERT_TRUE(is_aligned(8UL, 8UL), "is_aligned: 8 to 8 -> true");
    TEST_ASSERT_TRUE(is_aligned(16UL, 8UL), "is_aligned: 16 to 8 -> true");
    TEST_ASSERT_TRUE(is_aligned(1024UL, 1024UL), "is_aligned: 1024 to 1024 -> true");

    // Not aligned
    TEST_ASSERT_FALSE(is_aligned(1UL, 8UL), "is_aligned: 1 to 8 -> false");
    TEST_ASSERT_FALSE(is_aligned(5UL, 8UL), "is_aligned: 5 to 8 -> false");
    TEST_ASSERT_FALSE(is_aligned(7UL, 8UL), "is_aligned: 7 to 8 -> false");
    TEST_ASSERT_FALSE(is_aligned(9UL, 8UL), "is_aligned: 9 to 8 -> false");

    // Different alignments
    TEST_ASSERT_TRUE(is_aligned(0UL, 1UL), "is_aligned: anything to 1 -> true");
    TEST_ASSERT_TRUE(is_aligned(1024UL, 4UL), "is_aligned: 1024 to 4 -> true");
    TEST_ASSERT_FALSE(is_aligned(1026UL, 4UL), "is_aligned: 1026 to 4 -> false");

    TEST_PASS("is_aligned");
}

// ============================================================================
// Test div_round_up
// ============================================================================

static void test_div_round_up(void) {
    TEST_INFO("Testing div_round_up");

    // Exact division
    TEST_ASSERT_EQ(div_round_up(0UL, 8UL), 0UL, "div_round_up: 0/8 -> 0");
    TEST_ASSERT_EQ(div_round_up(8UL, 8UL), 1UL, "div_round_up: 8/8 -> 1");
    TEST_ASSERT_EQ(div_round_up(16UL, 8UL), 2UL, "div_round_up: 16/8 -> 2");
    TEST_ASSERT_EQ(div_round_up(100UL, 10UL), 10UL, "div_round_up: 100/10 -> 10");

    // Rounding up needed
    TEST_ASSERT_EQ(div_round_up(1UL, 8UL), 1UL, "div_round_up: 1/8 -> 1");
    TEST_ASSERT_EQ(div_round_up(9UL, 8UL), 2UL, "div_round_up: 9/8 -> 2");
    TEST_ASSERT_EQ(div_round_up(15UL, 8UL), 2UL, "div_round_up: 15/8 -> 2");
    TEST_ASSERT_EQ(div_round_up(17UL, 8UL), 3UL, "div_round_up: 17/8 -> 3");
    TEST_ASSERT_EQ(div_round_up(101UL, 10UL), 11UL, "div_round_up: 101/10 -> 11");

    // Large values
    TEST_ASSERT_EQ(div_round_up(4095UL, 4096UL), 1UL, "div_round_up: 4095/4096 -> 1");
    TEST_ASSERT_EQ(div_round_up(4096UL, 4096UL), 1UL, "div_round_up: 4096/4096 -> 1");
    TEST_ASSERT_EQ(div_round_up(4097UL, 4096UL), 2UL, "div_round_up: 4097/4096 -> 2");

    TEST_PASS("div_round_up");
}

// ============================================================================
// Test div_round_down
// ============================================================================

static void test_div_round_down(void) {
    TEST_INFO("Testing div_round_down");

    // Should behave same as regular division
    TEST_ASSERT_EQ(div_round_down(0UL, 8UL), 0UL, "div_round_down: 0/8 -> 0");
    TEST_ASSERT_EQ(div_round_down(8UL, 8UL), 1UL, "div_round_down: 8/8 -> 1");
    TEST_ASSERT_EQ(div_round_down(16UL, 8UL), 2UL, "div_round_down: 16/8 -> 2");
    TEST_ASSERT_EQ(div_round_down(1UL, 8UL), 0UL, "div_round_down: 1/8 -> 0");
    TEST_ASSERT_EQ(div_round_down(9UL, 8UL), 1UL, "div_round_down: 9/8 -> 1");
    TEST_ASSERT_EQ(div_round_down(15UL, 8UL), 1UL, "div_round_down: 15/8 -> 1");
    TEST_ASSERT_EQ(div_round_down(17UL, 8UL), 2UL, "div_round_down: 17/8 -> 2");

    TEST_PASS("div_round_down");
}

// ============================================================================
// Test div_round_nearest
// ============================================================================

static void test_div_round_nearest(void) {
    TEST_INFO("Testing div_round_nearest");

    // Exact division
    TEST_ASSERT_EQ(div_round_nearest(0UL, 8UL), 0UL, "div_round_nearest: 0/8 -> 0");
    TEST_ASSERT_EQ(div_round_nearest(8UL, 8UL), 1UL, "div_round_nearest: 8/8 -> 1");
    TEST_ASSERT_EQ(div_round_nearest(16UL, 8UL), 2UL, "div_round_nearest: 16/8 -> 2");

    // Round down (closer to lower)
    TEST_ASSERT_EQ(div_round_nearest(3UL, 8UL), 0UL, "div_round_nearest: 3/8 -> 0");
    TEST_ASSERT_EQ(div_round_nearest(11UL, 8UL), 1UL, "div_round_nearest: 11/8 -> 1");

    // Round up (closer to higher)
    TEST_ASSERT_EQ(div_round_nearest(5UL, 8UL), 1UL, "div_round_nearest: 5/8 -> 1");
    TEST_ASSERT_EQ(div_round_nearest(13UL, 8UL), 2UL, "div_round_nearest: 13/8 -> 2");

    // Tie-breaker (exact halves round up)
    TEST_ASSERT_EQ(div_round_nearest(4UL, 8UL), 1UL, "div_round_nearest: 4/8 -> 1 (half rounds up)");
    TEST_ASSERT_EQ(div_round_nearest(12UL, 8UL), 2UL, "div_round_nearest: 12/8 -> 2 (half rounds up)");

    TEST_PASS("div_round_nearest");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
    TEST_RUNNER_BEGIN("Bits Test Suite");

    TEST_INFO("Running is_power_of_2 tests...");
    test_is_power_of_2();

    TEST_INFO("Running round_up_to_power_of_2 tests...");
    test_round_up_to_power_of_2();

    TEST_INFO("Running round_down_to_power_of_2 tests...");
    test_round_down_to_power_of_2();

    TEST_INFO("Running align_up tests...");
    test_align_up();

    TEST_INFO("Running align_down tests...");
    test_align_down();

    TEST_INFO("Running is_aligned tests...");
    test_is_aligned();

    TEST_INFO("Running div_round_up tests...");
    test_div_round_up();

    TEST_INFO("Running div_round_down tests...");
    test_div_round_down();

    TEST_INFO("Running div_round_nearest tests...");
    test_div_round_nearest();

    TEST_RUNNER_END();
}
