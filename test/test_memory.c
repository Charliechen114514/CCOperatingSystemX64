/**
 * @file test_memory.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

// For hosted test environment, we need size_t before including string.h
// The kernel defines size_t only for freestanding environment
#include <stddef.h>
#include <stdint.h>

// Forward declare string functions to avoid including the full header
// in hosted environment where size_t comes from stddef.h
extern void* memset(void* s, int c, size_t n);
extern void* memcpy(void* dest, const void* src, size_t n);
extern void* memmove(void* dest, const void* src, size_t n);
extern int memcmp(const void* s1, const void* s2, size_t n);
// Define global test counters
int g_test_passed = 0;
int g_test_failed = 0;

#include "host_support.h"

static void test_memset() {
    unsigned char buffer[256];

    // Test 1: Basic memset - fill with zeros
    for (int i = 0; i < 256; i++) buffer[i] = 0xFF;
    void* result = memset(buffer, 0, 256);
    TEST_ASSERT_EQ(result, buffer, "memset returns original pointer");
    TEST_ASSERT_TRUE(buffer[0] == 0 && buffer[255] == 0, "memset fills with zeros");

    // Test 2: Fill with 0xFF (max byte value)
    memset(buffer, 0xFF, 256);
    TEST_ASSERT_TRUE(buffer[0] == 0xFF && buffer[255] == 0xFF, "memset fills with 0xFF");

    // Test 3: Fill with arbitrary value (0x5A)
    memset(buffer, 0x5A, 100);
    TEST_ASSERT_TRUE(buffer[0] == 0x5A && buffer[99] == 0x5A, "memset fills with 0x5A");

    // Test 4: Negative value should be converted to unsigned char
    memset(buffer, -1, 10);
    TEST_ASSERT_TRUE(buffer[0] == 0xFF, "memset converts -1 to 0xFF");

    // Test 5: Zero length should do nothing
    buffer[0] = 0x42;
    memset(buffer, 0xAA, 0);
    TEST_ASSERT_EQ(buffer[0], 0x42, "memset with zero length does nothing");

    // Test 6: Single byte
    memset(buffer, 0x77, 1);
    TEST_ASSERT_EQ(buffer[0], 0x77, "memset sets single byte");

    // Test 7: Various byte values (0x00 to 0xFF)
    int all_values_ok = 1;
    for (int val = 0; val <= 0xFF; val++) {
        memset(buffer, val, 1);
        if (buffer[0] != (unsigned char)val) {
            all_values_ok = 0;
            break;
        }
    }
    TEST_ASSERT_TRUE(all_values_ok, "memset handles all byte values 0x00-0xFF");

    // Test 8: Fill middle portion of buffer
    memset(buffer, 0xAA, 256);
    memset(buffer + 50, 0x55, 50);
    TEST_ASSERT_TRUE(buffer[49] == 0xAA && buffer[50] == 0x55 && buffer[99] == 0x55 && buffer[100] == 0xAA,
                     "memset fills middle portion");

    // Test 9: Large buffer (unaligned sizes)
    memset(buffer, 0x33, 7);
    TEST_ASSERT_TRUE(buffer[0] == 0x33 && buffer[6] == 0x33, "memset handles size 7");

    memset(buffer, 0x44, 13);
    TEST_ASSERT_TRUE(buffer[0] == 0x44 && buffer[12] == 0x44, "memset handles size 13");

    memset(buffer, 0x55, 17);
    TEST_ASSERT_TRUE(buffer[0] == 0x55 && buffer[16] == 0x55, "memset handles size 17");
}

static void test_memcpy() {
    unsigned char src[256];
    unsigned char dest[256];

    // Test 1: Basic memcpy
    for (int i = 0; i < 256; i++) src[i] = (unsigned char)i;
    memset(dest, 0, 256);
    void* result = memcpy(dest, src, 256);
    TEST_ASSERT_EQ(result, dest, "memcpy returns dest pointer");
    TEST_ASSERT_TRUE(dest[0] == 0 && dest[255] == 255, "memcpy copies data correctly");

    // Test 2: Zero length copy
    dest[0] = 0x42;
    memcpy(dest, src, 0);
    TEST_ASSERT_EQ(dest[0], 0x42, "memcpy with zero length does nothing");

    // Test 3: Single byte copy
    src[0] = 0xAB;
    dest[0] = 0x00;
    memcpy(dest, src, 1);
    TEST_ASSERT_EQ(dest[0], 0xAB, "memcpy copies single byte");

    // Test 4: Unaligned sizes
    for (int i = 0; i < 20; i++) src[i] = (unsigned char)(i * 7);
    memcpy(dest, src, 7);
    TEST_ASSERT_TRUE(dest[0] == 0 && dest[6] == 42, "memcpy handles size 7");

    memcpy(dest, src, 13);
    TEST_ASSERT_TRUE(dest[0] == 0 && dest[12] == 84, "memcpy handles size 13");

    memcpy(dest, src, 17);
    TEST_ASSERT_TRUE(dest[0] == 0 && dest[16] == 112, "memcpy handles size 17");

    // Test 5: Copy with special byte values
    src[0] = 0x00; src[1] = 0xFF; src[2] = 0x5A; src[3] = 0xA5;
    memcpy(dest, src, 4);
    TEST_ASSERT_TRUE(dest[0] == 0x00 && dest[1] == 0xFF && dest[2] == 0x5A && dest[3] == 0xA5,
                     "memcpy handles special byte values");

    // Test 6: Small buffers
    unsigned char small_src[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    unsigned char small_dest[10];
    memset(small_dest, 0xFF, 10);
    memcpy(small_dest, small_src, 10);
    int match = 1;
    for (int i = 0; i < 10; i++) {
        if (small_dest[i] != small_src[i]) match = 0;
    }
    TEST_ASSERT_TRUE(match, "memcpy copies small buffer correctly");

    // Test 7: Copy to overlapping offset (forward direction - may work, undefined by standard)
    // Note: Standard says overlapping is undefined, but we test non-overlapping cases
    unsigned char buffer[100];
    for (int i = 0; i < 50; i++) buffer[i] = (unsigned char)i;
    memcpy(buffer + 50, buffer, 30);  // Non-overlapping
    TEST_ASSERT_TRUE(buffer[50] == 0 && buffer[79] == 29, "memcpy copies to non-overlapping region");

    // Test 8: Copy all zeros
    memset(src, 0, 100);
    memcpy(dest, src, 100);
    int all_zero = 1;
    for (int i = 0; i < 100; i++) {
        if (dest[i] != 0) all_zero = 0;
    }
    TEST_ASSERT_TRUE(all_zero, "memcpy copies all zeros");

    // Test 9: Copy all 0xFF
    memset(src, 0xFF, 100);
    memcpy(dest, src, 100);
    int all_ff = 1;
    for (int i = 0; i < 100; i++) {
        if (dest[i] != 0xFF) all_ff = 0;
    }
    TEST_ASSERT_TRUE(all_ff, "memcpy copies all 0xFF");
}

static void test_memmove() {
    unsigned char buffer[512];

    // Test 1: Basic non-overlapping copy (same as memcpy behavior)
    for (int i = 0; i < 256; i++) buffer[i] = (unsigned char)i;
    void* result = memmove(buffer + 256, buffer, 100);
    TEST_ASSERT_EQ(result, buffer + 256, "memmove returns dest pointer");
    TEST_ASSERT_TRUE(buffer[256] == 0 && buffer[355] == 99, "memmove non-overlapping copy");

    // Test 2: Overlapping - dest after src (backward copy)
    for (int i = 0; i < 100; i++) buffer[i] = (unsigned char)i;
    memmove(buffer + 20, buffer, 60);
    // Should copy 0-59 to positions 20-79
    TEST_ASSERT_TRUE(buffer[0] == 0 && buffer[19] == 19, "memmove preserves source prefix");
    TEST_ASSERT_TRUE(buffer[20] == 0 && buffer[79] == 59, "memmove overlaps correctly (forward)");

    // Test 3: Overlapping - dest before src (backward copy needed)
    for (int i = 0; i < 100; i++) buffer[i] = (unsigned char)i;
    memmove(buffer, buffer + 20, 60);
    // Should copy 20-79 to positions 0-59
    TEST_ASSERT_TRUE(buffer[0] == 20 && buffer[59] == 79, "memmove overlaps correctly (backward)");

    // Test 4: Zero length
    buffer[0] = 0x42;
    memmove(buffer + 10, buffer, 0);
    TEST_ASSERT_EQ(buffer[0], 0x42, "memmove with zero length does nothing");

    // Test 5: Single byte
    buffer[0] = 0xAB;
    buffer[1] = 0x00;
    memmove(buffer + 1, buffer, 1);
    TEST_ASSERT_EQ(buffer[1], 0xAB, "memmove copies single byte");

    // Test 6: Same source and destination (should work)
    for (int i = 0; i < 50; i++) buffer[i] = (unsigned char)i;
    memmove(buffer, buffer, 50);
    TEST_ASSERT_TRUE(buffer[0] == 0 && buffer[49] == 49, "memmove handles same src/dest");

    // Test 7: Complete overlap (shift entire content)
    for (int i = 0; i < 100; i++) buffer[i] = (unsigned char)i;
    memmove(buffer + 50, buffer, 50);
    TEST_ASSERT_TRUE(buffer[50] == 0 && buffer[99] == 49, "memmove shifts content forward");

    for (int i = 0; i < 100; i++) buffer[i] = (unsigned char)i;
    memmove(buffer, buffer + 50, 50);
    TEST_ASSERT_TRUE(buffer[0] == 50 && buffer[49] == 99, "memmove shifts content backward");

    // Test 8: Unaligned sizes
    for (int i = 0; i < 100; i++) buffer[i] = (unsigned char)i;
    memmove(buffer + 30, buffer, 7);
    TEST_ASSERT_TRUE(buffer[30] == 0 && buffer[36] == 6, "memmove handles size 7");

    for (int i = 0; i < 100; i++) buffer[i] = (unsigned char)i;
    memmove(buffer + 40, buffer, 13);
    TEST_ASSERT_TRUE(buffer[40] == 0 && buffer[52] == 12, "memmove handles size 13");

    for (int i = 0; i < 100; i++) buffer[i] = (unsigned char)i;
    memmove(buffer + 50, buffer, 17);
    TEST_ASSERT_TRUE(buffer[50] == 0 && buffer[66] == 16, "memmove handles size 17");
}

static void test_memcmp() {
    unsigned char buf1[256];
    unsigned char buf2[256];

    // Test 1: Equal buffers
    memset(buf1, 0x5A, 100);
    memset(buf2, 0x5A, 100);
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 100) == 0, "memcmp returns 0 for equal buffers");

    // Test 2: Different at first byte
    buf1[0] = 0x00;
    buf2[0] = 0x01;
    int result = memcmp(buf1, buf2, 10);
    TEST_ASSERT_TRUE(result < 0, "memcmp returns negative when s1 < s2");

    result = memcmp(buf2, buf1, 10);
    TEST_ASSERT_TRUE(result > 0, "memcmp returns positive when s1 > s2");

    // Test 3: Different at last byte
    memset(buf1, 0x55, 100);
    memset(buf2, 0x55, 100);
    buf1[99] = 0xAA;
    buf2[99] = 0xBB;
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 100) < 0, "memcmp finds difference at end");

    // Test 4: Zero length comparison
    buf1[0] = 0x00;
    buf2[0] = 0xFF;
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 0) == 0, "memcmp with zero length returns 0");

    // Test 5: Single byte comparison
    buf1[0] = 0x42;
    buf2[0] = 0x42;
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 1) == 0, "memcmp single byte equal");

    buf2[0] = 0x43;
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 1) < 0, "memcmp single byte less");

    // Test 6: All zeros vs all FF
    memset(buf1, 0x00, 50);
    memset(buf2, 0xFF, 50);
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 50) < 0, "memcmp all zeros < all FF");

    // Test 7: Difference in middle
    for (int i = 0; i < 100; i++) buf1[i] = buf2[i] = 0x55;
    buf1[50] = 0x10;
    buf2[50] = 0x20;
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 100) < 0, "memcmp finds difference in middle");

    // Test 8: With zero values (not null terminator)
    buf1[0] = 0x00; buf1[1] = 0x00; buf1[2] = 0x01;
    buf2[0] = 0x00; buf2[1] = 0x00; buf2[2] = 0x02;
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 3) < 0, "memcmp handles embedded zeros");

    // Test 9: Same buffer comparison
    memset(buf1, 0x77, 50);
    TEST_ASSERT_TRUE(memcmp(buf1, buf1, 50) == 0, "memcmp same buffer returns 0");

    // Test 10: Large buffer comparison
    for (int i = 0; i < 200; i++) buf1[i] = buf2[i] = (unsigned char)i;
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 200) == 0, "memcmp large equal buffers");

    buf1[150] = 0x00;
    buf2[150] = 0x01;
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 200) < 0, "memcmp large buffer with late difference");

    // Test 11: Sign extension test (unsigned char comparison)
    buf1[0] = 0xFF;  // 255 as unsigned
    buf2[0] = 0x01;  // 1 as unsigned
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 1) > 0, "memcmp uses unsigned comparison");

    // Test 12: Unaligned sizes
    for (int i = 0; i < 20; i++) buf1[i] = buf2[i] = (unsigned char)i;
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 7) == 0, "memcmp size 7 equal");
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 13) == 0, "memcmp size 13 equal");
    TEST_ASSERT_TRUE(memcmp(buf1, buf2, 17) == 0, "memcmp size 17 equal");
}

int main(void) {
    TEST_RUNNER_BEGIN("Memory Operations");

    TEST_INFO("Testing memset...");
    test_memset();

    TEST_INFO("Testing memcpy...");
    test_memcpy();

    TEST_INFO("Testing memmove...");
    test_memmove();

    TEST_INFO("Testing memcmp...");
    test_memcmp();

    TEST_RUNNER_END();
}
