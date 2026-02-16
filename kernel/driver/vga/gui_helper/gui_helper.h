/**
 * @file gui_helper.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VGA GUI Helper Functions - Common GUI drawing primitives
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once
#include "vga/vga.h"

// Draw a rectangle border at specified position
void vga_draw_rect(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, vga_sz_t width, vga_sz_t height,
                   vga_color_t color);

// Draw a filled rectangle with specified color
void vga_draw_fill_rect(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, vga_sz_t width, vga_sz_t height,
                        char fill_char, vga_color_t font, vga_color_t bg);

// Draw a horizontal line
void vga_draw_hline(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, vga_sz_t length, char line_char,
                    vga_color_t color);

// Draw a vertical line
void vga_draw_vline(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, vga_sz_t length, char line_char,
                    vga_color_t color);

// Panel structure for organized GUI layout
typedef struct {
    vga_sz_t x;
    vga_sz_t y;
    vga_sz_t width;
    vga_sz_t height;
    vga_color_t border_color;
    vga_color_t bg_color;
    vga_color_t text_color;
    const char* title;
} vga_panel_t;

// Draw a panel with border and optional title
void vga_draw_panel(CCOS_VGA* vga, const vga_panel_t* panel);

// Draw text centered in a rectangular area
void vga_draw_text_centered(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, vga_sz_t width, const char* text,
                            vga_color_t font, vga_color_t bg);

// Draw a horizontal bar (useful for progress bars)
void vga_draw_bar(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, vga_sz_t width, vga_sz_t filled,
                  char fill_char, vga_color_t fill_color, vga_color_t empty_color);