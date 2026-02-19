/**
 * @file serial_color.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief ANSI color escape sequences for serial output
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "serial_color.h"

/**
 * @brief ANSI escape sequence prefix and suffix
 */
#define ANSI_ESCAPE "\033["
#define ANSI_RESET "0m"
#define ANSI_COLOR_M "m"

// Static ANSI escape sequences
static const char* const g_ansi_colors[] = {
    "\033[0m",   // RESET
    "\033[30m",  // BLACK
    "\033[31m",  // RED
    "\033[32m",  // GREEN
    "\033[33m",  // YELLOW
    "\033[34m",  // BLUE
    "\033[35m",  // MAGENTA
    "\033[36m",  // CYAN
    "\033[37m",  // WHITE
    "\033[90m",  // GRAY
    "\033[91m",  // BRIGHT_RED
    "\033[92m",  // BRIGHT_GREEN
    "\033[93m",  // BRIGHT_YELLOW
    "\033[94m",  // BRIGHT_BLUE
    "\033[95m",  // BRIGHT_MAGENTA
    "\033[96m",  // BRIGHT_CYAN
    "\033[97m",  // BRIGHT_WHITE
};

const char* serial_color_ansi(serial_color_t color) {
    if (color >= 0 && color < (sizeof(g_ansi_colors) / sizeof(g_ansi_colors[0]))) {
        return g_ansi_colors[color];
    }
    return g_ansi_colors[0];  // Return RESET on invalid color
}

const char* serial_color_for_log_level(int level) {
    switch (level) {
        case 0: return g_ansi_colors[9];  // TRACE  -> GRAY (index 9)
        case 1: return g_ansi_colors[6];  // DEBUG  -> MAGENTA (index 6)
        case 2: return g_ansi_colors[3];  // INFO   -> GREEN (index 3)
        case 3: return g_ansi_colors[4];  // WARN   -> YELLOW (index 4)
        case 4: return g_ansi_colors[2];  // ERROR  -> RED (index 2)
        default: return g_ansi_colors[0]; // -1 or invalid -> RESET (index 0)
    }
}
