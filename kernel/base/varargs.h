/**
 * @file varargs.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief GCC/Clang built-in va_list for freestanding environment
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

/* GCC/Clang built-in va_list for freestanding environment */
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
