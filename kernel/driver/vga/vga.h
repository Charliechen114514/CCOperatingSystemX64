/**
 * @file vga.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief drivers for VGA Supports
 * @version 0.1
 * @date 2026-02-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once
#include "defines/types.h"

typedef struct CCOS_VGA CCOS_VGA;
typedef uint8_t vga_sz_t;
typedef uint16_t vga_cursor_t;

typedef enum vga_color_t {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_BRIGHT_BLUE = 9,
    VGA_COLOR_BRIGHT_GREEN = 10,
    VGA_COLOR_BRIGHT_CYAN = 11,
    VGA_COLOR_BRIGHT_RED = 12,
    VGA_COLOR_BRIGHT_MAGENTA = 13,
    VGA_COLOR_YELLOW = 14,
    VGA_COLOR_WHITE = 15
} vga_color_t;

typedef enum VGA_PROPERTY {
    CURSOR_X,
    CURSOR_Y,
    CURSOR_FONT_COLOR,
    CURSOR_BACKGROUND_COLOR
} vga_property_t;

typedef struct CCOS_VGA {
    // internal handles
    volatile char* base_addr;
    vga_sz_t width;
    vga_sz_t height;
    vga_cursor_t native_cursor_pos; // packed cursor position (X in high byte, Y in low byte)
    vga_color_t font_color;         // current font color (0-15)
    vga_color_t background_color;   // current background color (0-15)
} CCOS_VGA;

void system_vga_init(); // to enable vgas, one must call these

CCOS_VGA* vga_instance(); // Will Get NULL if vga sucks

// Clear screen with specific background color
void vga_clear(CCOS_VGA* vga, vga_color_t background);

// Get cursor position (returns packed value: X in high byte, Y in low byte)
vga_cursor_t vga_get_cursor(const CCOS_VGA* vga);

// Set cursor position (x: column, y: row)
void vga_set_cursor(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y);

// Only vga_property_t are accept to filled
void set_vga_property(CCOS_VGA* vga, void* data, vga_property_t what_property);
void vga_print_string(CCOS_VGA* vga, const char* string);
void vga_print_stringn(CCOS_VGA* vga, const char* string, const vga_sz_t str_sz);
