/**
 * @file serial_backends.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Serial port backend implementation for kprintf
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "serial_backends.h"
#include "driver/serial/serial.h"
#include "driver/serial/serial_color.h"

// Serial backend is always ready once initialized
static bool serial_is_ready(void) {
    return true; // believe it!
}

/**
 * @brief Process string with color based on log level
 */
static void serial_process(const char* str, int level) {
    // Send color escape sequence
    sync_serial_puts(serial_color_for_log_level(level));
    // Send the actual string
    sync_serial_puts(str);
    // Reset color
    sync_serial_puts(serial_color_for_log_level(-1));  // -1 returns RESET
}

// Serial backend operations with color support
static const KLogBackendOps g_serial_backend_ops = {
    .process = serial_process,
    .is_ready = serial_is_ready,
};

bool klog_serial_backend_init(void) {
    return serial_init();
}

const KLogBackendOps* klog_serial_backend_get_ops(void) {
    return &g_serial_backend_ops;
}
