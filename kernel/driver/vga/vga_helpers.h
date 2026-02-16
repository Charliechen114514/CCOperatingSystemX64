/**
 * @file vga_helpers.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VGA Helper Functions
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once
#include "defines/types.h"
#include "vga.h"

// Get the X coordinate from cursor position
static inline vga_sz_t vga_cursor_x(const CCOS_VGA* vga) {
    return (vga_sz_t)((vga->native_cursor_pos & 0xFF00) >> 8);
}

// Get the Y coordinate from cursor position
static inline vga_sz_t vga_cursor_y(const CCOS_VGA* vga) {
    return (vga_sz_t)(vga->native_cursor_pos & 0x00FF);
}

// Make VGA entry (character with color attribute)
static inline uint16_t vga_make_entry(char c, vga_color_t font, vga_color_t background) {
    return (uint16_t)c | ((uint16_t)(background << 4 | font) << 8);
}

// Simple delay loop (no precise timing in kernel)
void vga_delay(uint32_t count);

// Put a single character at specified position with colors
void vga_put_char_at(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, char c, vga_color_t font,
                     vga_color_t bg);