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

// Scroll the screen up by n lines (positive: up, negative: down)
// Positive lines: content moves up, top lines are lost, bottom cleared
// Negative lines: content moves down, top cleared, bottom lines are lost
void vga_scroll(CCOS_VGA* vga, int lines);

void vga_print_string(CCOS_VGA* vga, const char* string);
void vga_print_stringn(CCOS_VGA* vga, const char* string, const vga_sz_t str_sz);

// Enable or disable the hardware cursor
void vga_enable_cursor(bool enable);

// Set the hardware cursor color (foreground and background)
// fg_color: cursor foreground color (0-15)
// bg_color: cursor background color (0-15)
// Note: This affects the Attribute Controller palette, not just cursor.
// For text mode, consider using software cursor instead.
void vga_set_cursor_color(vga_color_t fg_color, vga_color_t bg_color);

/* ============================================================================
 * Software Cursor Functions
 * ============================================================================ */

/**
 * @brief Software cursor state structure
 */
typedef struct vga_soft_cursor {
    bool enabled;           // Whether software cursor is active
    bool visible;           // Current visibility state (for blinking)
    vga_sz_t x;             // Cursor X position
    vga_sz_t y;             // Cursor Y position
    vga_color_t fg_color;   // Cursor foreground color
    vga_color_t bg_color;   // Cursor background color
    uint16_t saved_char;    // Saved character at cursor position
    bool has_saved_char;    // Whether saved_char is valid
} vga_soft_cursor_t;

/**
 * @brief Initialize software cursor
 *
 * Disables hardware cursor and enables software cursor with specified colors.
 *
 * @param vga VGA instance
 * @param fg_color Cursor foreground color (0-15)
 * @param bg_color Cursor background color (0-15)
 */
void vga_soft_cursor_init(CCOS_VGA* vga, vga_color_t fg_color, vga_color_t bg_color);

/**
 * @brief Enable or disable software cursor
 *
 * @param vga VGA instance
 * @param enable true to enable, false to disable
 */
void vga_soft_cursor_enable(CCOS_VGA* vga, bool enable);

/**
 * @brief Update software cursor position
 *
 * Removes cursor from old position and draws at new position.
 *
 * @param vga VGA instance
 * @param x New X position
 * @param y New Y position
 */
void vga_soft_cursor_update(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y);

/**
 * @brief Draw software cursor at current position
 *
 * @param vga VGA instance
 */
void vga_soft_cursor_draw(CCOS_VGA* vga);

/**
 * @brief Remove software cursor from current position (restore original char)
 *
 * @param vga VGA instance
 */
void vga_soft_cursor_hide(CCOS_VGA* vga);

/**
 * @brief Get software cursor state
 *
 * @param vga VGA instance
 * @return Pointer to software cursor state, or NULL if not initialized
 */
vga_soft_cursor_t* vga_soft_cursor_get(CCOS_VGA* vga);
