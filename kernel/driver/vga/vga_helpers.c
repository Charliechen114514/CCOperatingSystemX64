/**
 * @file vga_helpers.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VGA Helper Functions
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "vga_helpers.h"
#include "vga.h"

// Simple delay loop (no precise timing in kernel)
void vga_delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm__ volatile("nop");
    }
}

// Put a single character at specified position with colors
void vga_put_char_at(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, char c, vga_color_t font,
                     vga_color_t bg) {
    if (vga == NULL || x >= vga->width || y >= vga->height)
        return;

    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;
    uint16_t entry = (uint16_t)c | ((uint16_t)(bg << 4 | font) << 8);
    video[y * vga->width + x] = entry;
}