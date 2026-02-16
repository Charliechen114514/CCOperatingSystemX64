/**
 * @file kprintf.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief printf in kernels, like linux kprintf
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "base/varargs.h"
#include "kprintf_backends.h"

/**
 * @brief Log levels for klog_* functions
 */
typedef enum {
    KLOG_LEVEL_TRACE = 0,
    KLOG_LEVEL_DEBUG = 1,
    KLOG_LEVEL_INFO  = 2,
    KLOG_LEVEL_WARN  = 3,
    KLOG_LEVEL_ERROR = 4,
} klog_level_t;

/**
 * @brief Initialize the klog subsystem with specified backend
 * @param backend the default backend to use
 * @return true on success
 */
bool klog_init(klog_backend_t backend);

/**
 * @brief Kernel printf with specified backend
 * @param backend output backend
 * @param format printf-style format string
 */
void kprintf(klog_backend_t backend, const char* format, ...);

/**
 * @brief Kernel printf with va_list
 * @param backend output backend
 * @param format printf-style format string
 * @param args variable arguments
 */
void kvprintf(klog_backend_t backend, const char* format, va_list args);

/**
 * @brief Set the minimum log level
 * @param level minimum level to display
 */
void klog_set_level(klog_level_t level);

/**
 * @brief Get the current minimum log level
 * @return current minimum level
 */
klog_level_t klog_get_level(void);

/**
 * @brief Get log level name as string
 * @param level log level
 * @return string representation
 */
const char* klog_level_name(klog_level_t level);

// Log level macros
#define klog_trace(format, ...) klog_log(KLOG_LEVEL_TRACE, format, ##__VA_ARGS__)
#define klog_debug(format, ...) klog_log(KLOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define klog_info(format, ...)  klog_log(KLOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define klog_warn(format, ...)  klog_log(KLOG_LEVEL_WARN, format, ##__VA_ARGS__)
#define klog_error(format, ...) klog_log(KLOG_LEVEL_ERROR, format, ##__VA_ARGS__)

// Internal function for macros
void klog_log(klog_level_t level, const char* format, ...);
