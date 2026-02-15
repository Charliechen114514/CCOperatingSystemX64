/**
 * @file test_static_assert.c
 * @brief Tests for kernel static assertion macros
 */

#include "static_assert.h"
#include "host_support.h"
#include <stdio.h>

// Test the STATIC_ASSERT macro with various expressions
STATIC_ASSERT(1 == 1, basic_true);
STATIC_ASSERT(sizeof(int) >= 2, int_size);
STATIC_ASSERT(sizeof(long) >= 4, long_size);
STATIC_ASSERT(sizeof(char) == 1, char_size);

// Test that negative assertions would fail at compile time
// STATIC_ASSERT(0 == 1, should_fail);  // Uncomment to verify compile-time failure

int main(void) {
    printf("Static assertion tests compiled successfully!\n");
    printf("All compile-time checks passed.\n");
    return 0;
}
