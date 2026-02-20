/**
 * @file assert.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Assert backends
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "assert.h"
#include "assert_action_backend.h"
/* GCC/Clang built-in va_list for freestanding environment */
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

void panic(const char* expr_str, const char* file, int line, const char* func) {
    /* Print assertion failure message to VGA */
    assert_backend_to_vga(file, line, func, expr_str);

    /* Halt the system */
    assert_failed_action();
}

void ccos_assert_impl(bool condition, const char* expr_str, const char* file, int line,
                      const char* func) {
    if (condition) {
        return;
    }

    /* Print assertion failure message to VGA */
    panic(expr_str, file, line, func);
}
