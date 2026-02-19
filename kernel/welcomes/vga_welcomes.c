/**
 * @file vga_welcomes.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Boot Welcome with in VGAS
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ccos_config.h"
#include "driver/vga/vga.h"
#include "driver/vga/vga_helpers.h"

// ==================== COOL WELCOME SCREEN ====================

// ASCII Art Logo
static const char* const CCOS_LOGO[] = {
    "  _____  _____ _____    ___    ",    " /  _  \\/  ___//  __ \\  /   |   ",
    " | | | |\\ `--.| /  \\/ / /| |   ",  " | | | | `--. \\ |    / /_| |   ",
    " | |_| |/\\__/ / \\__/\\____  |   ", "  \\___/ \\____/ \\____/    |_|   "};

static const char* const CCOS_NAME = "CCOperating System X64";

// Draw a gradient border with animation effect
static void draw_animated_border(CCOS_VGA* vga) {
    vga_color_t colors[] = {VGA_COLOR_BRIGHT_CYAN, VGA_COLOR_BRIGHT_BLUE, VGA_COLOR_BRIGHT_MAGENTA,
                            VGA_COLOR_BRIGHT_RED,  VGA_COLOR_YELLOW,      VGA_COLOR_BRIGHT_GREEN};
    int num_colors = 6;

    vga_sz_t x = 5, y = 3;
    vga_sz_t width = 70, height = 19;

    // Animate the border colors cycling
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int i = 0; i < num_colors; i++) {
            // Draw top and bottom borders
            for (vga_sz_t col = 0; col < width; col++) {
                vga_color_t color = colors[(i + col / 5) % num_colors];

                // Top border
                vga_put_char_at(vga, x + col, y, '=', color, VGA_COLOR_BLACK);
                // Bottom border
                vga_put_char_at(vga, x + col, y + height - 1, '=', color, VGA_COLOR_BLACK);
            }

            // Draw left and right borders
            for (vga_sz_t row = 0; row < height; row++) {
                vga_color_t color = colors[(i + row / 2) % num_colors];

                // Left border
                vga_put_char_at(vga, x, y + row, '|', color, VGA_COLOR_BLACK);
                // Right border
                vga_put_char_at(vga, x + width - 1, y + row, '|', color, VGA_COLOR_BLACK);
            }

            // Draw corners
            vga_put_char_at(vga, x, y, '+', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_put_char_at(vga, x + width - 1, y, '+', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_put_char_at(vga, x, y + height - 1, '+', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_put_char_at(vga, x + width - 1, y + height - 1, '+', VGA_COLOR_WHITE,
                            VGA_COLOR_BLACK);
        }
    }
}

// Draw starfield background
static void draw_starfield(CCOS_VGA* vga, vga_sz_t start_x, vga_sz_t start_y, vga_sz_t width,
                           vga_sz_t height) {
    const char stars[] = {'*', '.', '+', '\'', '^'};

    for (int i = 0; i < 100; i++) {
        vga_sz_t x = start_x + (vga_sz_t)(i * 73) % width;
        vga_sz_t y = start_y + (vga_sz_t)(i * 137) % height;
        char star = stars[(i / 20) % 5];
        vga_color_t color = (vga_color_t)(VGA_COLOR_WHITE - (i % 4));
        vga_put_char_at(vga, x, y, star, color, VGA_COLOR_BLACK);
    }
}

// Typewriter effect for text
static void typewriter_print(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, const char* text,
                             vga_color_t color) {
    set_vga_property(vga, &color, CURSOR_FONT_COLOR);

    for (int i = 0; text[i] != '\0'; i++) {
        vga_put_char_at(vga, x + i, y, text[i], color, VGA_COLOR_BLACK);
    }
}

// Main welcome screen
void vga_display_welcome(void) {
    CCOS_VGA* vga_ins = vga_instance();

    // Start with black screen
    vga_clear(vga_ins, VGA_COLOR_BLACK);

    // Draw starfield background
    draw_starfield(vga_ins, 5, 3, 70, 19);

    // Draw animated border
    draw_animated_border(vga_ins);

    // Draw CCOS Logo with animation
    vga_sz_t logo_start_x = (80 - 30) / 2;
    vga_sz_t logo_start_y = 6;
    vga_color_t logo_color = VGA_COLOR_BRIGHT_CYAN;
    set_vga_property(vga_ins, &logo_color, CURSOR_FONT_COLOR);

    for (int i = 0; i < 6; i++) {
        vga_set_cursor(vga_ins, logo_start_x, logo_start_y + i);
        vga_print_string(vga_ins, CCOS_LOGO[i]);
    }

    // Draw system name with typewriter effect
    vga_sz_t name_x = (80 - 24) / 2;
    typewriter_print(vga_ins, name_x, 13, CCOS_NAME, VGA_COLOR_YELLOW);

    // Draw version info
    vga_sz_t ver_x = (80 - 20) / 2;
    vga_set_cursor(vga_ins, ver_x, 15);
    vga_color_t ver_color = VGA_COLOR_BRIGHT_GREEN;
    set_vga_property(vga_ins, &ver_color, CURSOR_FONT_COLOR);
    vga_print_string(vga_ins, "Version " CCOS_VERSION " [Alpha]");
}
