/**
 * @file gui_helper.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VGA GUI Helper Functions - Common GUI drawing primitives implementation
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "gui_helper.h"
#include "vga/vga_helpers.h"

// Draw a rectangle border at specified position
void vga_draw_rect(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, vga_sz_t width, vga_sz_t height,
                   vga_color_t color) {
    if (vga == NULL || width < 2 || height < 2)
        return;

    // Draw corners
    vga_put_char_at(vga, x, y, '+', color, VGA_COLOR_BLACK);
    vga_put_char_at(vga, x + width - 1, y, '+', color, VGA_COLOR_BLACK);
    vga_put_char_at(vga, x, y + height - 1, '+', color, VGA_COLOR_BLACK);
    vga_put_char_at(vga, x + width - 1, y + height - 1, '+', color, VGA_COLOR_BLACK);

    // Draw horizontal borders
    for (vga_sz_t i = 1; i < width - 1; i++) {
        vga_put_char_at(vga, x + i, y, '-', color, VGA_COLOR_BLACK);
        vga_put_char_at(vga, x + i, y + height - 1, '-', color, VGA_COLOR_BLACK);
    }

    // Draw vertical borders
    for (vga_sz_t i = 1; i < height - 1; i++) {
        vga_put_char_at(vga, x, y + i, '|', color, VGA_COLOR_BLACK);
        vga_put_char_at(vga, x + width - 1, y + i, '|', color, VGA_COLOR_BLACK);
    }
}

// Draw a filled rectangle with specified color
void vga_draw_fill_rect(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, vga_sz_t width, vga_sz_t height,
                        char fill_char, vga_color_t font, vga_color_t bg) {
    if (vga == NULL)
        return;

    for (vga_sz_t j = 0; j < height && (y + j) < vga->height; j++) {
        for (vga_sz_t i = 0; i < width && (x + i) < vga->width; i++) {
            vga_put_char_at(vga, x + i, y + j, fill_char, font, bg);
        }
    }
}

// Draw a horizontal line
void vga_draw_hline(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, vga_sz_t length, char line_char,
                    vga_color_t color) {
    if (vga == NULL)
        return;

    for (vga_sz_t i = 0; i < length && (x + i) < vga->width; i++) {
        vga_put_char_at(vga, x + i, y, line_char, color, VGA_COLOR_BLACK);
    }
}

// Draw a vertical line
void vga_draw_vline(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, vga_sz_t length, char line_char,
                    vga_color_t color) {
    if (vga == NULL)
        return;

    for (vga_sz_t i = 0; i < length && (y + i) < vga->height; i++) {
        vga_put_char_at(vga, x, y + i, line_char, color, VGA_COLOR_BLACK);
    }
}

// Draw a panel with border and optional title
void vga_draw_panel(CCOS_VGA* vga, const vga_panel_t* panel) {
    if (vga == NULL || panel == NULL)
        return;

    // Draw border
    vga_draw_rect(vga, panel->x, panel->y, panel->width, panel->height, panel->border_color);

    // Draw title if provided
    if (panel->title != NULL) {
        vga_sz_t title_len = 0;
        const char* p = panel->title;
        while (*p != '\0') {
            title_len++;
            p++;
        }

        if (title_len > 0 && title_len < panel->width - 2) {
            vga_sz_t title_x = panel->x + (panel->width - title_len) / 2;
            vga_put_char_at(vga, panel->x, panel->y, ' ', panel->border_color, VGA_COLOR_BLACK);
            vga_put_char_at(vga, panel->x + 1, panel->y, '[', panel->border_color, VGA_COLOR_BLACK);

            for (vga_sz_t i = 0; i < title_len && (title_x + i + 2) < (panel->x + panel->width - 1);
                 i++) {
                vga_put_char_at(vga, title_x + i + 2, panel->y, panel->title[i], panel->text_color,
                                VGA_COLOR_BLACK);
            }

            vga_put_char_at(vga, title_x + title_len + 2, panel->y, ']', panel->border_color,
                            VGA_COLOR_BLACK);
        }
    }
}

// Draw text centered in a rectangular area
void vga_draw_text_centered(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, vga_sz_t width, const char* text,
                            vga_color_t font, vga_color_t bg) {
    if (vga == NULL || text == NULL)
        return;

    vga_sz_t text_len = 0;
    const char* p = text;
    while (*p != '\0') {
        text_len++;
        p++;
    }

    vga_sz_t start_x = x;
    if (text_len < width) {
        start_x = x + (width - text_len) / 2;
    }

    for (vga_sz_t i = 0; i < text_len && (start_x + i) < vga->width; i++) {
        vga_put_char_at(vga, start_x + i, y, text[i], font, bg);
    }
}

// Draw a horizontal bar (useful for progress bars)
void vga_draw_bar(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, vga_sz_t width, vga_sz_t filled,
                  char fill_char, vga_color_t fill_color, vga_color_t empty_color) {
    if (vga == NULL)
        return;

    for (vga_sz_t i = 0; i < width; i++) {
        vga_color_t color = (i < filled) ? fill_color : empty_color;
        vga_put_char_at(vga, x + i, y, fill_char, color, VGA_COLOR_BLACK);
    }
}