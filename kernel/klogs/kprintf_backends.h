/**
 * @file kprintf_backends.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Backend interface for kprintf - output destination abstraction
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "defines/types.h"

/**
 * @brief Log backend types
 */
typedef enum {
    KLOG_BACKEND_NONE = 0,    /**< No output */
    KLOG_BACKEND_SERIAL = 1,  /**< Serial port (COM1) */
    KLOG_BACKEND_VGA = 2,     /**< VGA text mode (future) */
} klog_backend_t;

/**
 * @brief Backend function table - each backend implements these
 */
typedef struct {
    /**
     * @brief Process a formatted string with log level
     * @param str pre-formatted string to process
     * @param level the log level
     * @note Backend can add colors, formatting, etc. based on level
     */
    void (*process)(const char* str, int level);

    /**
     * @brief Check if backend is ready/available
     * @return true if backend is available
     */
    bool (*is_ready)(void);
} KLogBackendOps;

/**
 * @brief Get the operations for a specific backend
 * @param backend the backend type
 * @return pointer to backend operations, or NULL if invalid
 */
const KLogBackendOps* klog_get_backend_ops(klog_backend_t backend);

/**
 * @brief Set the default backend for klog_* functions
 * @param backend the backend to use as default
 */
void klog_set_default_backend(klog_backend_t backend);

/**
 * @brief Get the current default backend
 * @return current default backend
 */
klog_backend_t klog_get_default_backend(void);

/**
 * @brief Register a custom backend (for future extensibility)
 * @param backend_type backend type identifier
 * @param ops operations table
 * @return true on success
 */
bool klog_register_backend(klog_backend_t backend_type, const KLogBackendOps* ops);
