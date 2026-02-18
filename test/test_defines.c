/**
 * @file test_defines.c
 * @brief Tests for kernel type definitions and compile-time constants
 */

#include "types.h"
#include "static_assert.h"
#include "host_support.h"

// ============================================================================
// Type Size Tests - Compile Time Validation
// ============================================================================

// Signed integer types
STATIC_ASSERT(sizeof(int8_t)   == 1, int8_size_must_be_1_byte);
STATIC_ASSERT(sizeof(int16_t)  == 2, int16_size_must_be_2_bytes);
STATIC_ASSERT(sizeof(int32_t)  == 4, int32_size_must_be_4_bytes);
STATIC_ASSERT(sizeof(int64_t)  == 8, int64_size_must_be_8_bytes);

// Unsigned integer types
STATIC_ASSERT(sizeof(uint8_t)  == 1, uint8_size_must_be_1_byte);
STATIC_ASSERT(sizeof(uint16_t) == 2, uint16_size_must_be_2_bytes);
STATIC_ASSERT(sizeof(uint32_t) == 4, uint32_size_must_be_4_bytes);
STATIC_ASSERT(sizeof(uint64_t) == 8, uint64_size_must_be_8_bytes);

// Pointer-sized types (on x86_64)
STATIC_ASSERT(sizeof(intptr_t)  == 8, intptr_size_on_x86_64);
STATIC_ASSERT(sizeof(uintptr_t) == 8, uintptr_size_on_x86_64);
// Note: size_t is from stdlib in test environment, defined in kernel for freestanding
STATIC_ASSERT(sizeof(ptrdiff_t) == 8, ptrdiff_t_size_on_x86_64);

// ============================================================================
// Type Limit Tests - Compile Time Validation
// ============================================================================

STATIC_ASSERT(INT8_MAX   == 0x7F,    int8_max_correct);
STATIC_ASSERT(INT8_MIN   == -0x80,   int8_min_correct);
STATIC_ASSERT(UINT8_MAX  == 0xFF,    uint8_max_correct);

STATIC_ASSERT(INT16_MAX  == 0x7FFF,  int16_max_correct);
STATIC_ASSERT(INT16_MIN  == -0x8000, int16_min_correct);
STATIC_ASSERT(UINT16_MAX == 0xFFFF,  uint16_max_correct);

STATIC_ASSERT(INT32_MAX  == 0x7FFFFFFF,  int32_max_correct);
STATIC_ASSERT(INT32_MIN  == -2147483647 - 1, int32_min_correct);
STATIC_ASSERT(UINT32_MAX == 0xFFFFFFFFU, uint32_max_correct);

STATIC_ASSERT(INT64_MAX  == 0x7FFFFFFFFFFFFFFFLL,  int64_max_correct);
STATIC_ASSERT(INT64_MIN  == -9223372036854775807LL - 1, int64_min_correct);
STATIC_ASSERT(UINT64_MAX == 0xFFFFFFFFFFFFFFFFULL, uint64_max_correct);

// ============================================================================
// Utility Type Tests
// ============================================================================

STATIC_ASSERT(sizeof(physical_addr_t) == 8, physical_addr_is_64bit);
STATIC_ASSERT(sizeof(virtual_addr_t)  == 8, virtual_addr_is_64bit);

// ============================================================================
// Runtime Verification (for completeness)
// ============================================================================
int main(void) {
    TEST_RUNNER_BEGIN("Type Definitions");

    TEST_INFO("Verifying integer type sizes...");
    TEST_PASS("int8_t size verification");
    TEST_PASS("int16_t size verification");
    TEST_PASS("int32_t size verification");
    TEST_PASS("int64_t size verification");

    TEST_INFO("Verifying type limits...");
    TEST_PASS("Signed integer limits");
    TEST_PASS("Unsigned integer limits");

    TEST_INFO("Verifying pointer-sized types on x86_64...");
    TEST_PASS("Pointer-sized type verification");

    TEST_RUNNER_END();
}
