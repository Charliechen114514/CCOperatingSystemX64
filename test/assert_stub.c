/**
 * @file assert_stub.c
 * @brief Host-side stub implementation for kernel assert functions
 *
 * This provides a minimal implementation of ccos_assert_impl for static testing
 * on the host system. Unlike the kernel version which outputs to VGA and halts
 * the system, this version simply prints to stderr and continues.
 */

#include <stdio.h>
#include <stdlib.h>

void ccos_assert_impl(bool condition, const char* expr_str, const char* file, int line,
                      const char* func) {
    if (condition) {
        return;
    }

    // Print assertion failure to stderr
    fprintf(stderr, "\n");
    fprintf(stderr, "*** ASSERTION FAILED ***\n");
    fprintf(stderr, "File: %s:%d\n", file ? file : "<unknown>", line);
    fprintf(stderr, "Function: %s\n", func ? func : "<unknown>");
    fprintf(stderr, "Expression: %s\n", expr_str ? expr_str : "<unknown>");
    fprintf(stderr, "\n");

    // In test environment, we abort to make failures visible
    abort();
}
