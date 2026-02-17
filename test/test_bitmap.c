/**
 * @file test_bitmap.c
 * @brief Comprehensive unit tests for bitmap module
 */

// Standard types for hosted environment
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>  // For ssize_t

// Include kernel types and bitmap header
#include "defines/types.h"
#include "bitmap/bitmap.h"
#include "bitmap/bitmap_helper.h"

// Define global test counters
int g_test_passed = 0;
int g_test_failed = 0;

#include "host_support.h"

// ============================================================================
// Test Helper Functions
// ============================================================================

#define BUFFER_SIZE(bytes) (((bytes) + 7) / 8)

// ============================================================================
// 1. Helper Functions Tests
// ============================================================================

static void test_helpers(void) {
    TEST_INFO("Testing helper functions");

    // Test bitCntToByteCnt
    TEST_ASSERT_EQ(bitCntToByteCnt(1), (size_t)1, "bitCntToByteCnt: 1 bit -> 1 byte");
    TEST_ASSERT_EQ(bitCntToByteCnt(8), (size_t)1, "bitCntToByteCnt: 8 bits -> 1 byte");
    TEST_ASSERT_EQ(bitCntToByteCnt(9), (size_t)2, "bitCntToByteCnt: 9 bits -> 2 bytes");
    TEST_ASSERT_EQ(bitCntToByteCnt(16), (size_t)2, "bitCntToByteCnt: 16 bits -> 2 bytes");
    TEST_ASSERT_EQ(bitCntToByteCnt(64), (size_t)8, "bitCntToByteCnt: 64 bits -> 8 bytes");
    TEST_ASSERT_EQ(bitCntToByteCnt(65), (size_t)9, "bitCntToByteCnt: 65 bits -> 9 bytes");
    TEST_ASSERT_EQ(bitCntToByteCnt(10000), (size_t)1250, "bitCntToByteCnt: 10000 bits -> 1250 bytes");

    // Test byte_bitmask
    TEST_ASSERT_EQ(byte_bitmask(0), (uint8_t)0x01, "byte_bitmask: bit 0 -> 0x01");
    TEST_ASSERT_EQ(byte_bitmask(1), (uint8_t)0x02, "byte_bitmask: bit 1 -> 0x02");
    TEST_ASSERT_EQ(byte_bitmask(2), (uint8_t)0x04, "byte_bitmask: bit 2 -> 0x04");
    TEST_ASSERT_EQ(byte_bitmask(7), (uint8_t)0x80, "byte_bitmask: bit 7 -> 0x80");
    TEST_ASSERT_EQ(byte_bitmask(8), (uint8_t)0x01, "byte_bitmask: bit 8 -> 0x01 (wraps)");
    TEST_ASSERT_EQ(byte_bitmask(15), (uint8_t)0x80, "byte_bitmask: bit 15 -> 0x80");
    TEST_ASSERT_EQ(byte_bitmask(100), (uint8_t)0x01, "byte_bitmask: bit 100 -> 0x01");

    // Test bit_byte
    TEST_ASSERT_EQ(bit_byte(0), (size_t)0, "bit_byte: bit 0 -> byte 0");
    TEST_ASSERT_EQ(bit_byte(7), (size_t)0, "bit_byte: bit 7 -> byte 0");
    TEST_ASSERT_EQ(bit_byte(8), (size_t)1, "bit_byte: bit 8 -> byte 1");
    TEST_ASSERT_EQ(bit_byte(15), (size_t)1, "bit_byte: bit 15 -> byte 1");
    TEST_ASSERT_EQ(bit_byte(16), (size_t)2, "bit_byte: bit 16 -> byte 2");
    TEST_ASSERT_EQ(bit_byte(63), (size_t)7, "bit_byte: bit 63 -> byte 7");
    TEST_ASSERT_EQ(bit_byte(64), (size_t)8, "bit_byte: bit 64 -> byte 8");
    TEST_ASSERT_EQ(bit_byte(100), (size_t)12, "bit_byte: bit 100 -> byte 12");

    TEST_PASS("helper functions");
}

// ============================================================================
// 2. Initialization Tests
// ============================================================================

static void test_init(void) {
    TEST_INFO("Testing bitmap_init");

    byte_t buffer[128];
    struct bitmap bm;

    // Test basic initialization
    bitmap_init(&bm, buffer, 64);
    TEST_ASSERT_EQ(bm.nbits, (size_t)64, "init: nbits set correctly");
    TEST_ASSERT_EQ(bm.bits, buffer, "init: bits pointer set correctly");

    // Verify all bits are cleared after init
    bool all_zero = true;
    for (size_t i = 0; i < bm.nbits; i++) {
        if (bitmap_test(&bm, i)) {
            all_zero = false;
            break;
        }
    }
    TEST_ASSERT_TRUE(all_zero, "init: all bits cleared");

    // Test different sizes
    bitmap_init(&bm, buffer, 1);
    TEST_ASSERT_EQ(bm.nbits, (size_t)1, "init: 1 bit bitmap");

    bitmap_init(&bm, buffer, 128);
    TEST_ASSERT_EQ(bm.nbits, (size_t)128, "init: 128 bit bitmap");

    bitmap_init(&bm, buffer, 1000);
    TEST_ASSERT_EQ(bm.nbits, (size_t)1000, "init: 1000 bit bitmap");

    // Test cross-byte boundary
    bitmap_init(&bm, buffer, 65);  // Exactly 9 bytes
    TEST_ASSERT_EQ(bm.nbits, (size_t)65, "init: cross-byte boundary");

    TEST_PASS("bitmap_init");
}

// ============================================================================
// 3. Single Bit Operations Tests
// ============================================================================

static void test_single_bit(void) {
    TEST_INFO("Testing single bit operations");

    byte_t buffer[128];
    struct bitmap bm;
    bitmap_init(&bm, buffer, 64);

    // Test bitmap_set
    bitmap_set(&bm, 0);
    TEST_ASSERT_TRUE(bitmap_test(&bm, 0), "set: bit 0 is set");

    bitmap_set(&bm, 7);
    TEST_ASSERT_TRUE(bitmap_test(&bm, 7), "set: bit 7 is set");

    bitmap_set(&bm, 8);
    TEST_ASSERT_TRUE(bitmap_test(&bm, 8), "set: bit 8 is set (second byte)");

    bitmap_set(&bm, 63);
    TEST_ASSERT_TRUE(bitmap_test(&bm, 63), "set: bit 63 is set");

    // Test bitmap_clear
    bitmap_clear(&bm, 0);
    TEST_ASSERT_FALSE(bitmap_test(&bm, 0), "clear: bit 0 is cleared");

    bitmap_clear(&bm, 7);
    TEST_ASSERT_FALSE(bitmap_test(&bm, 7), "clear: bit 7 is cleared");

    // Test clearing already cleared bit (idempotent)
    bitmap_clear(&bm, 10);
    bitmap_clear(&bm, 10);
    TEST_ASSERT_FALSE(bitmap_test(&bm, 10), "clear: idempotent operation");

    // Test bitmap_flip
    bitmap_init(&bm, buffer, 64);
    bitmap_flip(&bm, 5);
    TEST_ASSERT_TRUE(bitmap_test(&bm, 5), "flip: 0 -> 1");

    bitmap_flip(&bm, 5);
    TEST_ASSERT_FALSE(bitmap_test(&bm, 5), "flip: 1 -> 0");

    // Test multiple flips
    bitmap_flip(&bm, 10);
    bitmap_flip(&bm, 10);
    bitmap_flip(&bm, 10);
    TEST_ASSERT_TRUE(bitmap_test(&bm, 10), "flip: odd number of flips");

    // Test setting same bit multiple times
    bitmap_set(&bm, 20);
    bitmap_set(&bm, 20);
    TEST_ASSERT_TRUE(bitmap_test(&bm, 20), "set: multiple sets");

    TEST_PASS("single bit operations");
}

// ============================================================================
// 4. Range Operations Tests
// ============================================================================

static void test_range_operations(void) {
    TEST_INFO("Testing range operations");

    byte_t buffer[256];
    struct bitmap bm;
    bitmap_init(&bm, buffer, 256);

    // Test bitmap_set_range - basic
    bitmap_set_range(&bm, 0, 8);
    for (size_t i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(bitmap_test(&bm, i), "set_range: bits 0-7 set");
    }
    TEST_ASSERT_FALSE(bitmap_test(&bm, 8), "set_range: bit 8 not set");

    // Test bitmap_set_range - cross byte boundary
    bitmap_init(&bm, buffer, 256);
    bitmap_set_range(&bm, 5, 10);  // Covers bytes 0 and 1
    for (size_t i = 5; i < 15; i++) {
        TEST_ASSERT_TRUE(bitmap_test(&bm, i), "set_range: cross-byte boundary");
    }
    TEST_ASSERT_FALSE(bitmap_test(&bm, 4), "set_range: before range not set");
    TEST_ASSERT_FALSE(bitmap_test(&bm, 15), "set_range: after range not set");

    // Test bitmap_set_range - cross word boundary (64-bit)
    bitmap_init(&bm, buffer, 256);
    bitmap_set_range(&bm, 60, 10);  // Crosses 64-bit boundary
    for (size_t i = 60; i < 70; i++) {
        TEST_ASSERT_TRUE(bitmap_test(&bm, i), "set_range: cross-word boundary");
    }

    // Test bitmap_set_range - entire bitmap
    bitmap_init(&bm, buffer, 64);
    bitmap_set_range(&bm, 0, 64);
    for (size_t i = 0; i < 64; i++) {
        TEST_ASSERT_TRUE(bitmap_test(&bm, i), "set_range: entire bitmap");
    }

    // Test bitmap_clear_range
    bitmap_init(&bm, buffer, 64);
    bitmap_set_range(&bm, 0, 64);
    bitmap_clear_range(&bm, 10, 20);
    for (size_t i = 10; i < 30; i++) {
        TEST_ASSERT_FALSE(bitmap_test(&bm, i), "clear_range: bits cleared");
    }
    TEST_ASSERT_TRUE(bitmap_test(&bm, 9), "clear_range: before range preserved");
    TEST_ASSERT_TRUE(bitmap_test(&bm, 30), "clear_range: after range preserved");

    // Test bitmap_clear_range - entire bitmap
    bitmap_set_range(&bm, 0, 64);
    bitmap_clear_range(&bm, 0, 64);
    for (size_t i = 0; i < 64; i++) {
        TEST_ASSERT_FALSE(bitmap_test(&bm, i), "clear_range: entire bitmap");
    }

    // Test clearing already cleared bits (idempotent)
    bitmap_init(&bm, buffer, 64);
    bitmap_clear_range(&bm, 10, 10);
    bitmap_clear_range(&bm, 10, 10);
    TEST_ASSERT_TRUE(bitmap_empty(&bm), "clear_range: idempotent on empty");

    // Test range operations on boundaries
    bitmap_init(&bm, buffer, 100);
    bitmap_set_range(&bm, 0, 1);
    TEST_ASSERT_TRUE(bitmap_test(&bm, 0), "set_range: single bit at start");

    bitmap_set_range(&bm, 99, 1);
    TEST_ASSERT_TRUE(bitmap_test(&bm, 99), "set_range: single bit at end");

    TEST_PASS("range operations");
}

// ============================================================================
// 5. Bit Scanning Tests
// ============================================================================

static void test_bit_scanning(void) {
    TEST_INFO("Testing bit scanning");

    byte_t buffer[256];
    struct bitmap bm;
    bitmap_init(&bm, buffer, 64);

    // Test bitmap_find_first_zero on empty bitmap
    ssize_t result = bitmap_find_first_zero(&bm);
    TEST_ASSERT_EQ(result, (ssize_t)0, "find_first_zero: empty bitmap returns 0");

    // Test bitmap_find_first_set on empty bitmap
    result = bitmap_find_first_set(&bm);
    TEST_ASSERT_EQ(result, (ssize_t)-1, "find_first_set: empty bitmap returns -1");

    // Set some bits and test
    bitmap_set(&bm, 5);
    result = bitmap_find_first_set(&bm);
    TEST_ASSERT_EQ(result, (ssize_t)5, "find_first_set: finds first set bit");

    bitmap_set(&bm, 0);
    result = bitmap_find_first_set(&bm);
    TEST_ASSERT_EQ(result, (ssize_t)0, "find_first_set: bit 0 is first");

    // Test bitmap_find_first_zero with some bits set
    bitmap_init(&bm, buffer, 64);
    bitmap_set_range(&bm, 0, 10);
    result = bitmap_find_first_zero(&bm);
    TEST_ASSERT_EQ(result, (ssize_t)10, "find_first_zero: after range");

    // Test find_next_zero
    bitmap_init(&bm, buffer, 64);
    bitmap_set_range(&bm, 0, 10);
    bitmap_set_range(&bm, 15, 5);
    result = bitmap_find_next_zero(&bm, 0);
    TEST_ASSERT_EQ(result, (ssize_t)10, "find_next_zero: from position 0");

    result = bitmap_find_next_zero(&bm, 10);
    TEST_ASSERT_EQ(result, (ssize_t)10, "find_next_zero: from position 10");

    result = bitmap_find_next_zero(&bm, 11);
    TEST_ASSERT_EQ(result, (ssize_t)20, "find_next_zero: from position 11");

    // Test find_next_set
    bitmap_init(&bm, buffer, 64);
    bitmap_set(&bm, 5);
    bitmap_set(&bm, 15);
    bitmap_set(&bm, 25);

    result = bitmap_find_next_set(&bm, 0);
    TEST_ASSERT_EQ(result, (ssize_t)5, "find_next_set: from 0 finds 5");

    result = bitmap_find_next_set(&bm, 6);
    TEST_ASSERT_EQ(result, (ssize_t)15, "find_next_set: from 6 finds 15");

    result = bitmap_find_next_set(&bm, 16);
    TEST_ASSERT_EQ(result, (ssize_t)25, "find_next_set: from 16 finds 25");

    result = bitmap_find_next_set(&bm, 26);
    TEST_ASSERT_EQ(result, (ssize_t)-1, "find_next_set: no more set bits");

    // Test full bitmap
    bitmap_init(&bm, buffer, 64);
    bitmap_set_range(&bm, 0, 64);
    result = bitmap_find_first_zero(&bm);
    TEST_ASSERT_EQ(result, (ssize_t)-1, "find_first_zero: full bitmap returns -1");

    result = bitmap_find_first_set(&bm);
    TEST_ASSERT_EQ(result, (ssize_t)0, "find_first_set: full bitmap returns 0");

    // Test edge case - searching from last position
    bitmap_init(&bm, buffer, 64);
    bitmap_set(&bm, 63);
    result = bitmap_find_next_zero(&bm, 63);
    TEST_ASSERT_EQ(result, (ssize_t)-1, "find_next_zero: from last position");

    TEST_PASS("bit scanning");
}

// ============================================================================
// 6. Logical Operations Tests
// ============================================================================

static void test_logical_operations(void) {
    TEST_INFO("Testing logical operations");

    byte_t buf1[64], buf2[64], buf3[64];
    struct bitmap bm1, bm2, dst;

    // Test bitmap_and
    bitmap_init(&bm1, buf1, 32);
    bitmap_init(&bm2, buf2, 32);
    bitmap_init(&dst, buf3, 32);

    // Set bits in bm1: 0, 1, 2, 3
    bitmap_set_range(&bm1, 0, 4);
    // Set bits in bm2: 2, 3, 4, 5
    bitmap_set_range(&bm2, 2, 4);

    bitmap_and(&dst, &bm1, &bm2);
    // Result should have bits 2, 3 set
    TEST_ASSERT_FALSE(bitmap_test(&dst, 0), "and: bit 0 not in result");
    TEST_ASSERT_FALSE(bitmap_test(&dst, 1), "and: bit 1 not in result");
    TEST_ASSERT_TRUE(bitmap_test(&dst, 2), "and: bit 2 in result");
    TEST_ASSERT_TRUE(bitmap_test(&dst, 3), "and: bit 3 in result");
    TEST_ASSERT_FALSE(bitmap_test(&dst, 4), "and: bit 4 not in result");

    // Test bitmap_or
    bitmap_init(&bm1, buf1, 32);
    bitmap_init(&bm2, buf2, 32);
    bitmap_init(&dst, buf3, 32);
    bitmap_set(&bm1, 5);
    bitmap_set(&bm2, 10);
    bitmap_or(&dst, &bm1, &bm2);
    TEST_ASSERT_TRUE(bitmap_test(&dst, 5), "or: bit 5 in result");
    TEST_ASSERT_TRUE(bitmap_test(&dst, 10), "or: bit 10 in result");
    TEST_ASSERT_FALSE(bitmap_test(&dst, 0), "or: bit 0 not in result");

    // Test bitmap_xor
    bitmap_init(&bm1, buf1, 32);
    bitmap_init(&bm2, buf2, 32);
    bitmap_init(&dst, buf3, 32);
    bitmap_set_range(&bm1, 0, 8);
    bitmap_set_range(&bm2, 4, 8);
    bitmap_xor(&dst, &bm1, &bm2);
    // bits 0-3 should be set (only in bm1)
    // bits 4-7 should be clear (in both)
    // bits 8-11 should be set (only in bm2)
    TEST_ASSERT_TRUE(bitmap_test(&dst, 0), "xor: bit 0 set");
    TEST_ASSERT_TRUE(bitmap_test(&dst, 3), "xor: bit 3 set");
    TEST_ASSERT_FALSE(bitmap_test(&dst, 4), "xor: bit 4 clear");
    TEST_ASSERT_FALSE(bitmap_test(&dst, 7), "xor: bit 7 clear");
    TEST_ASSERT_TRUE(bitmap_test(&dst, 8), "xor: bit 8 set");
    TEST_ASSERT_TRUE(bitmap_test(&dst, 11), "xor: bit 11 set");

    // Test bitmap_andnot
    bitmap_init(&bm1, buf1, 32);
    bitmap_init(&bm2, buf2, 32);
    bitmap_init(&dst, buf3, 32);
    bitmap_set_range(&bm1, 0, 16);
    bitmap_set_range(&bm2, 8, 8);
    bitmap_andnot(&dst, &bm1, &bm2);
    // Should have bits 0-7 set, bits 8-15 clear
    TEST_ASSERT_TRUE(bitmap_test(&dst, 0), "andnot: bit 0 set");
    TEST_ASSERT_TRUE(bitmap_test(&dst, 7), "andnot: bit 7 set");
    TEST_ASSERT_FALSE(bitmap_test(&dst, 8), "andnot: bit 8 clear");
    TEST_ASSERT_FALSE(bitmap_test(&dst, 15), "andnot: bit 15 clear");

    // Test bitmap_complement
    bitmap_init(&bm1, buf1, 32);
    bitmap_init(&dst, buf3, 32);
    bitmap_set(&bm1, 0);
    bitmap_set(&bm1, 5);
    bitmap_set(&bm1, 10);
    bitmap_complement(&dst, &bm1);
    TEST_ASSERT_FALSE(bitmap_test(&dst, 0), "complement: bit 0 inverted");
    TEST_ASSERT_TRUE(bitmap_test(&dst, 1), "complement: bit 1 inverted");
    TEST_ASSERT_FALSE(bitmap_test(&dst, 5), "complement: bit 5 inverted");
    TEST_ASSERT_TRUE(bitmap_test(&dst, 6), "complement: bit 6 inverted");
    TEST_ASSERT_FALSE(bitmap_test(&dst, 10), "complement: bit 10 inverted");

    // Test double complement (should return original)
    bitmap_init(&bm1, buf1, 32);
    bitmap_set_range(&bm1, 0, 10);
    bitmap_complement(&dst, &bm1);
    bitmap_complement(&bm1, &dst);
    TEST_ASSERT_TRUE(bitmap_test(&bm1, 0), "complement: double complement restores bit 0");
    TEST_ASSERT_TRUE(bitmap_test(&bm1, 9), "complement: double complement restores bit 9");

    // Test self-xor (should result in all zeros)
    bitmap_init(&bm1, buf1, 32);
    bitmap_set_range(&bm1, 0, 16);
    bitmap_xor(&dst, &bm1, &bm1);
    TEST_ASSERT_TRUE(bitmap_empty(&dst), "xor: self-xor results in empty");

    TEST_PASS("logical operations");
}

// ============================================================================
// 7. Compare and Copy Tests
// ============================================================================

static void test_compare_copy(void) {
    TEST_INFO("Testing compare and copy");

    byte_t buf1[64], buf2[64];
    struct bitmap bm1, bm2;

    // Test bitmap_equal - same bits
    bitmap_init(&bm1, buf1, 32);
    bitmap_init(&bm2, buf2, 32);
    TEST_ASSERT_TRUE(bitmap_equal(&bm1, &bm2), "equal: both empty");

    bitmap_set(&bm1, 5);
    TEST_ASSERT_FALSE(bitmap_equal(&bm1, &bm2), "equal: different bits");

    bitmap_set(&bm2, 5);
    TEST_ASSERT_TRUE(bitmap_equal(&bm1, &bm2), "equal: same bits set");

    // Test bitmap_equal - different sizes
    bitmap_init(&bm1, buf1, 32);
    bitmap_init(&bm2, buf2, 64);
    TEST_ASSERT_FALSE(bitmap_equal(&bm1, &bm2), "equal: different sizes");

    // Test bitmap_copy
    bitmap_init(&bm1, buf1, 32);
    bitmap_init(&bm2, buf2, 32);
    bitmap_set_range(&bm1, 0, 16);
    bitmap_copy(&bm2, &bm1);
    TEST_ASSERT_TRUE(bitmap_equal(&bm1, &bm2), "copy: bitmaps equal after copy");
    for (size_t i = 0; i < 16; i++) {
        TEST_ASSERT_TRUE(bitmap_test(&bm2, i), "copy: bits copied correctly");
    }

    // Test copy overwrites destination
    bitmap_init(&bm1, buf1, 32);
    bitmap_init(&bm2, buf2, 32);

    bitmap_set_range(&bm2, 0, 32);
    bitmap_set(&bm1, 5);
    bitmap_copy(&bm2, &bm1);

    TEST_ASSERT_TRUE(bitmap_test(&bm2, 5), "copy: overwrite works");
    TEST_ASSERT_FALSE(bitmap_test(&bm2, 0), "copy: old bits cleared");

    TEST_PASS("compare and copy");
}

// ============================================================================
// 8. Utility Functions Tests
// ============================================================================

static void test_utility_functions(void) {
    TEST_INFO("Testing utility functions");

    byte_t buffer[256];
    struct bitmap bm;
    char str_buf[256];

    // Test bitmap_weight
    bitmap_init(&bm, buffer, 64);
    TEST_ASSERT_EQ(bitmap_weight(&bm), (size_t)0, "weight: empty bitmap");

    bitmap_set(&bm, 0);
    TEST_ASSERT_EQ(bitmap_weight(&bm), (size_t)1, "weight: 1 bit set");

    bitmap_set(&bm, 5);
    bitmap_set(&bm, 10);
    TEST_ASSERT_EQ(bitmap_weight(&bm), (size_t)3, "weight: 3 bits set");

    bitmap_set_range(&bm, 0, 32);
    TEST_ASSERT_EQ(bitmap_weight(&bm), (size_t)32, "weight: 32 bits set");

    bitmap_set_range(&bm, 0, 64);
    TEST_ASSERT_EQ(bitmap_weight(&bm), (size_t)64, "weight: full bitmap");

    // Test bitmap_full
    bitmap_init(&bm, buffer, 64);
    TEST_ASSERT_FALSE(bitmap_full(&bm), "full: empty bitmap not full");

    bitmap_set_range(&bm, 0, 63);
    TEST_ASSERT_FALSE(bitmap_full(&bm), "full: almost full not full");

    bitmap_set(&bm, 63);
    TEST_ASSERT_TRUE(bitmap_full(&bm), "full: all bits set is full");

    // Test bitmap_empty
    bitmap_init(&bm, buffer, 64);
    TEST_ASSERT_TRUE(bitmap_empty(&bm), "empty: newly initialized is empty");

    bitmap_set(&bm, 0);
    TEST_ASSERT_FALSE(bitmap_empty(&bm), "empty: with bit set not empty");

    bitmap_clear(&bm, 0);
    TEST_ASSERT_TRUE(bitmap_empty(&bm), "empty: after clear is empty");

    // Test bitmap_to_string
    bitmap_init(&bm, buffer, 16);
    bitmap_set(&bm, 0);
    bitmap_set(&bm, 3);
    bitmap_set(&bm, 7);
    bitmap_set(&bm, 15);
    bitmap_to_string(str_buf, &bm);

    TEST_ASSERT_EQ(strlen(str_buf), (size_t)16, "to_string: correct length");
    TEST_ASSERT_EQ(str_buf[0], '1', "to_string: bit 0 is '1'");
    TEST_ASSERT_EQ(str_buf[1], '0', "to_string: bit 1 is '0'");
    TEST_ASSERT_EQ(str_buf[3], '1', "to_string: bit 3 is '1'");
    TEST_ASSERT_EQ(str_buf[7], '1', "to_string: bit 7 is '1'");
    TEST_ASSERT_EQ(str_buf[15], '1', "to_string: bit 15 is '1'");
    TEST_ASSERT_EQ(str_buf[16], '\0', "to_string: null terminator");

    // Test to_string with all zeros
    bitmap_init(&bm, buffer, 8);
    bitmap_to_string(str_buf, &bm);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQ(str_buf[i], '0', "to_string: all zeros");
    }

    // Test to_string with all ones
    bitmap_set_range(&bm, 0, 8);
    bitmap_to_string(str_buf, &bm);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQ(str_buf[i], '1', "to_string: all ones");
    }

    // Test with 1-bit bitmap
    bitmap_init(&bm, buffer, 1);
    bitmap_to_string(str_buf, &bm);
    TEST_ASSERT_EQ(strlen(str_buf), (size_t)1, "to_string: 1-bit bitmap length");

    TEST_PASS("utility functions");
}

// ============================================================================
// 9. Edge Cases and Boundary Tests
// ============================================================================

static void test_edge_cases(void) {
    TEST_INFO("Testing edge cases");

    byte_t buffer[256];
    struct bitmap bm;

    // Test 1-bit bitmap
    bitmap_init(&bm, buffer, 1);
    TEST_ASSERT_FALSE(bitmap_test(&bm, 0), "edge: 1-bit init");
    bitmap_set(&bm, 0);
    TEST_ASSERT_TRUE(bitmap_test(&bm, 0), "edge: 1-bit set");
    TEST_ASSERT_TRUE(bitmap_full(&bm), "edge: 1-bit full");
    TEST_ASSERT_EQ(bitmap_weight(&bm), (size_t)1, "edge: 1-bit weight");

    // Test 63-bit bitmap (not byte-aligned)
    bitmap_init(&bm, buffer, 63);
    bitmap_set(&bm, 62);
    TEST_ASSERT_TRUE(bitmap_test(&bm, 62), "edge: last bit of unaligned size");
    TEST_ASSERT_FALSE(bitmap_test(&bm, 0), "edge: other bits not set");

    // Test 65-bit bitmap (exactly 9 bytes)
    bitmap_init(&bm, buffer, 65);
    bitmap_set(&bm, 64);
    TEST_ASSERT_TRUE(bitmap_test(&bm, 64), "edge: bit 64 set");

    // Test alternating pattern
    bitmap_init(&bm, buffer, 64);
    for (size_t i = 0; i < 64; i += 2) {
        bitmap_set(&bm, i);
    }
    size_t weight = bitmap_weight(&bm);
    TEST_ASSERT_EQ(weight, (size_t)32, "edge: alternating pattern weight");

    // Test find operations on alternating pattern
    ssize_t found = bitmap_find_next_set(&bm, 0);
    TEST_ASSERT_EQ(found, (ssize_t)0, "edge: find in alternating pattern");

    found = bitmap_find_next_zero(&bm, 0);
    TEST_ASSERT_EQ(found, (ssize_t)1, "edge: find zero in alternating");

    // Test large range set
    bitmap_init(&bm, buffer, 200);
    bitmap_set_range(&bm, 50, 100);
    for (size_t i = 50; i < 150; i++) {
        TEST_ASSERT_TRUE(bitmap_test(&bm, i), "edge: large range set");
    }
    TEST_ASSERT_FALSE(bitmap_test(&bm, 49), "edge: before large range");
    TEST_ASSERT_FALSE(bitmap_test(&bm, 150), "edge: after large range");

    TEST_PASS("edge cases");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
    TEST_RUNNER_BEGIN("Bitmap Test Suite");

    TEST_INFO("Running helper function tests...");
    test_helpers();

    TEST_INFO("Running initialization tests...");
    test_init();

    TEST_INFO("Running single bit operation tests...");
    test_single_bit();

    TEST_INFO("Running range operation tests...");
    test_range_operations();

    TEST_INFO("Running bit scanning tests...");
    test_bit_scanning();

    TEST_INFO("Running logical operation tests...");
    test_logical_operations();

    TEST_INFO("Running compare and copy tests...");
    test_compare_copy();

    TEST_INFO("Running utility function tests...");
    test_utility_functions();

    TEST_INFO("Running edge case tests...");
    test_edge_cases();

    TEST_RUNNER_END();
}
