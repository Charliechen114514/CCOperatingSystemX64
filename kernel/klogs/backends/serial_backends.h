/**
 * @file serial_backends.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Serial port backend for kprintf
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "kprintf_backends.h"

/**
 * @brief Initialize the serial backend
 * @return true on success
 */
bool klog_serial_backend_init(void);

/**
 * @brief Get the serial backend operations table
 * @return pointer to serial backend operations
 */
const KLogBackendOps* klog_serial_backend_get_ops(void);
