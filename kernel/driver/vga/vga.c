#include "vga.h"
#include "vga_config.h"

static CCOS_VGA internal_vga_instance;

const CCOS_VGA* vga_instance() {
    return &internal_vga_instance;
}

void system_vga_init() {
    internal_vga_instance.height = VGA_HEIGHT;
    internal_vga_instance.width = VGA_WIDTH;
    internal_vga_instance.base_addr =
        (char*)(uintptr_t)VGA_BASE_ADDR; // NOLINT(performance-no-int-to-ptr)
    internal_vga_instance.native_cursor_pos = 0;  // start at (0, 0)
    internal_vga_instance.font_color = 0x0F;      // white font
    internal_vga_instance.background_color = 0x00; // black background
}

// Make VGA entry (character with color attribute)
static inline uint16_t vga_entry(char c, vga_color_t font, vga_color_t background) {
    return (uint16_t)c | ((uint16_t)(background << 4 | font) << 8);
}

void vga_clear(CCOS_VGA* vga, vga_color_t background) {
    if (vga == NULL) return;

    uint16_t blank = vga_entry(' ', 0x0, background);
    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;

    for (vga_sz_t y = 0; y < vga->height; y++) {
        for (vga_sz_t x = 0; x < vga->width; x++) {
            video[y * vga->width + x] = blank;
        }
    }

    // Reset cursor to (0, 0)
    vga->native_cursor_pos = 0;
    vga->background_color = background;
}

vga_cursor_t vga_get_cursor(const CCOS_VGA* vga) {
    if (vga == NULL) return 0;
    return vga->native_cursor_pos;
}

void vga_set_cursor(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y) {
    if (vga == NULL) return;

    // Clamp to screen bounds
    if (x >= vga->width) x = vga->width - 1;
    if (y >= vga->height) y = vga->height - 1;

    // Pack cursor position: X in high byte, Y in low byte
    vga->native_cursor_pos = ((uint16_t)x << 8) | (uint16_t)y;
}

// Put a single character at current cursor position and advance cursor
static void vga_putc(CCOS_VGA* vga, char c) {
    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;
    vga_sz_t x = (vga_sz_t)((vga->native_cursor_pos & 0xFF00) >> 8);
    vga_sz_t y = (vga_sz_t)(vga->native_cursor_pos & 0x00FF);

    if (c == '\n') {
        // Newline: move to start of next line
        x = 0;
        y++;
        if (y >= vga->height) {
            y = vga->height - 1;  // stay on last line
        }
    } else {
        // Print character at current position
        uint16_t entry = vga_entry(c, vga->font_color, vga->background_color);
        video[y * vga->width + x] = entry;

        // Advance cursor
        x++;
        if (x >= vga->width) {
            x = 0;
            y++;
            if (y >= vga->height) {
                y = vga->height - 1;
            }
        }
    }

    // Update packed cursor position
    vga->native_cursor_pos = ((uint16_t)x << 8) | (uint16_t)y;
}

void vga_print_string(CCOS_VGA* vga, const char* string) {
    if (vga == NULL || string == NULL) return;

    while (*string != '\0') {
        vga_putc(vga, *string);
        string++;
    }
}

void vga_print_stringn(CCOS_VGA* vga, const char* string, const vga_sz_t str_sz) {
    if (vga == NULL || string == NULL) return;

    for (vga_sz_t i = 0; i < str_sz; i++) {
        vga_putc(vga, string[i]);
    }
}

void set_vga_property(CCOS_VGA* vga, void* data, vga_property_t what_property) {
    if (vga == NULL || data == NULL) return;

    switch (what_property) {
        case CURSOR_X: {
            vga_sz_t x = *(vga_sz_t*)data;
            vga_sz_t y = (vga_sz_t)(vga->native_cursor_pos & 0x00FF);
            vga_set_cursor(vga, x, y);
            break;
        }
        case CURSOR_Y: {
            vga_sz_t y = *(vga_sz_t*)data;
            vga_sz_t x = (vga_sz_t)((vga->native_cursor_pos & 0xFF00) >> 8);
            vga_set_cursor(vga, x, y);
            break;
        }
        case CURSOR_FONT_COLOR:
            vga->font_color = *(vga_color_t*)data;
            break;
        case CURSOR_BACKGROUND_COLOR:
            vga->background_color = *(vga_color_t*)data;
            break;
        default:
            break;
    }
}
