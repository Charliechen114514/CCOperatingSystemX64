/**
 * @file serial_color.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief ANSI color codes for serial output
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "defines/types.h"

/**
 * @brief ANSI color codes for serial terminal output
 */
typedef enum {
    SERIAL_COLOR_RESET  = 0,   /**< Reset to default */
    SERIAL_COLOR_BLACK = 30,   /**< Black */
    SERIAL_COLOR_RED   = 31,   /**< Red */
    SERIAL_COLOR_GREEN = 32,   /**< Green */
    SERIAL_COLOR_YELLOW= 33,   /**< Yellow */
    SERIAL_COLOR_BLUE  = 34,   /**< Blue */
    SERIAL_COLOR_MAGENTA=35,   /**< Magenta */
    SERIAL_COLOR_CYAN  = 36,   /**< Cyan */
    SERIAL_COLOR_WHITE = 37,   /**< White */
    SERIAL_COLOR_GRAY  = 90,   /**< Bright Black (Gray) */
    SERIAL_COLOR_BRIGHT_RED   = 91, /**< Bright Red */
    SERIAL_COLOR_BRIGHT_GREEN = 92, /**< Bright Green */
    SERIAL_COLOR_BRIGHT_YELLOW= 93, /**< Bright Yellow */
    SERIAL_COLOR_BRIGHT_BLUE  = 94, /**< Bright Blue */
    SERIAL_COLOR_BRIGHT_MAGENTA=95, /**< Bright Magenta */
    SERIAL_COLOR_BRIGHT_CYAN  = 96, /**< Bright Cyan */
    SERIAL_COLOR_BRIGHT_WHITE = 97, /**< Bright White */
} serial_color_t;

/**
 * @brief Get ANSI escape sequence for a color
 * @param color the color code
 * @return static string containing ANSI escape sequence
 */
const char* serial_color_ansi(serial_color_t color);
