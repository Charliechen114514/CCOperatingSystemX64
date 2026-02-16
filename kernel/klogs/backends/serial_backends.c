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
 * @brief Get ANSI color for log level
 */
static serial_color_t level_to_color(int level) {
    switch (level) {
        case 0: return SERIAL_COLOR_GRAY;    // TRACE
        case 1: return SERIAL_COLOR_CYAN;    // DEBUG
        case 2: return SERIAL_COLOR_GREEN;   // INFO
        case 3: return SERIAL_COLOR_YELLOW;  // WARN
        case 4: return SERIAL_COLOR_RED;     // ERROR
        default: return SERIAL_COLOR_WHITE;
    }
}

/**
 * @brief Process string with color based on log level
 */
static void serial_process(const char* str, int level) {
    // Send color escape sequence
    sync_serial_puts(serial_color_ansi(level_to_color(level)));
    // Send the actual string
    sync_serial_puts(str);
    // Reset color
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_RESET));
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
