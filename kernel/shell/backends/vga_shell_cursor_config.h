/**
 * @file vga_shell_cursor_config.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Software cursor configuration for VGA shell
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "driver/vga/vga.h"

/* ============================================================================
 * Software Cursor Configuration
 * ============================================================================ */

/**
 * @brief Enable software cursor (disable hardware cursor)
 *
 * Set to true to use software-rendered cursor instead of hardware cursor.
 * Software cursor allows custom colors and shapes.
 */
#define VGA_SHELL_USE_SOFTWARE_CURSOR true

/**
 * @brief Cursor width in characters
 *
 * Number of character cells the cursor occupies horizontally.
 * Recommended values: 1 or 2
 */
#define VGA_SHELL_CURSOR_WIDTH 1

/**
 * @brief Cursor height in scan lines (not character rows)
 *
 * For a full-block cursor: set to 16 (full character height)
 * For a half-block cursor: set to 8
 * For an underline cursor: set to 2
 *
 * Note: This is implemented by using different block characters.
 */
#define VGA_SHELL_CURSOR_HEIGHT 16

/**
 * @brief Cursor foreground color
 *
 * The color of the cursor itself (the block/shown part).
 * Use values from vga_color_t enum (0-15).
 */
#define VGA_SHELL_CURSOR_FG_COLOR VGA_COLOR_WHITE

/**
 * @brief Cursor background color
 *
 * The color behind the cursor (when using outline style).
 * For solid block cursor, this is not used.
 */
#define VGA_SHELL_CURSOR_BG_COLOR VGA_COLOR_BLACK

/**
 * @brief Cursor style
 *
 * VGA_SHELL_CURSOR_STYLE_BLOCK:      Full block cursor (█) - covers entire character
 * VGA_SHELL_CURSOR_STYLE_UNDERLINE:  Underline cursor (_) - line at bottom
 * VGA_SHELL_CURSOR_STYLE_INVERT:     Inverted colors - swaps fg/bg of character
 * VGA_SHELL_CURSOR_STYLE_OUTLINE:    Outline/bracket cursor ([ ] around character)
 */
typedef enum vga_shell_cursor_style {
    VGA_SHELL_CURSOR_STYLE_BLOCK = 0,     // Solid block cursor
    VGA_SHELL_CURSOR_STYLE_UNDERLINE = 1, // Underline cursor
    VGA_SHELL_CURSOR_STYLE_INVERT = 2,    // Inverted character
    VGA_SHELL_CURSOR_STYLE_OUTLINE = 3,   // Bracket/outline cursor
} vga_shell_cursor_style_t;

/**
 * @brief Default cursor style
 */
#define VGA_SHELL_CURSOR_STYLE VGA_SHELL_CURSOR_STYLE_BLOCK

/**
 * @brief Cursor blink enabled
 *
 * Set to true for blinking cursor, false for steady cursor.
 * Note: Blinking requires a timer callback to update cursor state.
 */
#define VGA_SHELL_CURSOR_BLINK false

/**
 * @brief Cursor blink interval in milliseconds
 *
 * Time between cursor blink state changes (when blink is enabled).
 * Recommended: 500 (half second)
 */
#define VGA_SHELL_CURSOR_BLINK_INTERVAL_MS 500

/* ============================================================================
 * Character Definitions for Software Cursor
 * ============================================================================ */

/* Block characters for cursor rendering */
#define VGA_CURSOR_BLOCK_FULL     0xDB  // █ Full block
#define VGA_CURSOR_BLOCK_UPPER    0xDF  // ▀ Upper half block
#define VGA_CURSOR_BLOCK_LOWER    0xDC  // ▄ Lower half block
#define VGA_CURSOR_BLOCK_QUAD     0xFE  // ■ Small block/quad
#define VGA_CURSOR_UNDERLINE      0x5F  // _ Underscore
