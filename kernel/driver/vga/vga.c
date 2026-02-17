#include "vga.h"
#include "io/io.h"
#include "vga_config.h"

// Include software cursor config to check if it should be used
// This must be included before the preprocessor check below
#include "shell/backends/vga_shell_cursor_config.h"

// Check if software cursor will be used (for system_vga_init decision)
#define VGA_USE_SOFTWARE_CURSOR VGA_SHELL_USE_SOFTWARE_CURSOR

/* ============================================================================
 * VGA Hardware Cursor Constants
 * ============================================================================ */

#define VGA_CTRL_REGISTER 0x3D4 // VGA control register
#define VGA_DATA_REGISTER 0x3D5 // VGA data register

#define VGA_CURSOR_HIGH 0x0E  // Cursor location high byte
#define VGA_CURSOR_LOW 0x0F   // Cursor location low byte
#define VGA_CURSOR_START 0x0A // Cursor start register
#define VGA_CURSOR_END 0x0B   // Cursor end register

#define VGA_ATTR_INDEX_ADDR 0x3C0  // Attribute controller index address
#define VGA_ATTR_DATA_ADDR 0x3C1   // Attribute controller data address
#define VGA_INPUT_STATUS_1 0x3DA   // Input Status 1 register (for flip-flop reset)
#define VGA_ATTR_CTRL_WRITE 0x3C0  // Attribute controller address for write
#define VGA_ATTR_COLOR_SELECT 0x14 // Color select register (index 0x14)

// Block characters for software cursor
#define VGA_CURSOR_BLOCK_FULL     0xDB  // █ Full block
#define VGA_CURSOR_BLOCK_LOWER    0xDC  // ▄ Lower half block
#define VGA_CURSOR_UNDERLINE      0x5F  // _ Underscore

static CCOS_VGA internal_vga_instance;
static vga_soft_cursor_t soft_cursor_state = {0};

/* ============================================================================
 * Hardware Cursor Functions
 * ============================================================================ */

/**
 * @brief Enable or disable the hardware cursor
 */
void vga_enable_cursor(bool enable) {
    if (enable) {
        // Disable cursor first (bit 5 = 1)
        outb(VGA_CTRL_REGISTER, VGA_CURSOR_START);
        outb(VGA_DATA_REGISTER, 0x20);

        // Small delay to ensure the disable takes effect
        __asm__ volatile("outb %%al, $0x80" : : "a"(0));

        // Enable cursor with solid block style (scan lines 0-15)
        outb(VGA_CTRL_REGISTER, VGA_CURSOR_START);
        outb(VGA_DATA_REGISTER, 0x00); // Start at scan line 14, enable (bit 5 = 0)

        outb(VGA_CTRL_REGISTER, VGA_CURSOR_END);
        outb(VGA_DATA_REGISTER, 0x0F); // End at scan line 15 (bottom line)
    } else {
        // Disable cursor by setting bit 5
        outb(VGA_CTRL_REGISTER, VGA_CURSOR_START);
        outb(VGA_DATA_REGISTER, 0x20); // Bit 5 = 1 disables cursor
    }
}

/**
 * @brief Set the hardware cursor color
 *
 * This function sets the cursor color using the VGA Attribute Controller.
 * The color is set via the Color Select register (index 0x14).
 *
 * @param fg_color Cursor foreground color (0-15)
 * @param bg_color Cursor background color (0-15)
 */
void vga_set_cursor_color(vga_color_t fg_color, vga_color_t bg_color) {
    // VGA Attribute Controller access sequence:
    // 1. Read from VGA_INPUT_STATUS_1 (0x3DA) to reset the index/data flip-flop
    // 2. Write index to VGA_ATTR_INDEX_ADDR (0x3C0)
    // 3. Write data to VGA_ATTR_CTRL_WRITE (0x3C0)

    // Step 1: Reset attribute controller index/data flip-flop
    (void)inb(VGA_INPUT_STATUS_1);

    // Step 2: Write the index (0x14 = Color Select register)
    outb(VGA_ATTR_INDEX_ADDR, VGA_ATTR_COLOR_SELECT);

    // Step 3: Write the color value
    // Bits 0-3: Cursor background color
    // Bits 4-7: Cursor foreground color
    uint8_t color_val = (bg_color & 0x0F) | ((fg_color & 0x0F) << 4);
    outb(VGA_ATTR_CTRL_WRITE, color_val);
}

/* ============================================================================
 * Software Cursor Functions
 * ============================================================================ */

vga_soft_cursor_t* vga_soft_cursor_get(CCOS_VGA* vga) {
    (void)vga;  // We use a global state for simplicity
    return &soft_cursor_state;
}

void vga_soft_cursor_init(CCOS_VGA* vga, vga_color_t fg_color, vga_color_t bg_color) {
    if (vga == NULL) {
        return;
    }

    // Disable hardware cursor
    vga_enable_cursor(false);

    // Initialize software cursor state
    soft_cursor_state.enabled = true;
    soft_cursor_state.visible = true;
    soft_cursor_state.x = 0;
    soft_cursor_state.y = 0;
    soft_cursor_state.fg_color = fg_color;
    soft_cursor_state.bg_color = bg_color;
    soft_cursor_state.has_saved_char = false;

    // Draw initial cursor
    vga_soft_cursor_draw(vga);
}

void vga_soft_cursor_enable(CCOS_VGA* vga, bool enable) {
    if (vga == NULL) {
        return;
    }

    if (enable && !soft_cursor_state.enabled) {
        // Enable: disable hardware cursor and enable software cursor
        vga_enable_cursor(false);
        soft_cursor_state.enabled = true;
        soft_cursor_state.visible = true;
        vga_soft_cursor_draw(vga);
    } else if (!enable && soft_cursor_state.enabled) {
        // Disable: hide cursor and enable hardware cursor
        vga_soft_cursor_hide(vga);
        soft_cursor_state.enabled = false;
        vga_enable_cursor(true);
        // Hardware cursor position will be synced on next vga_set_cursor call
    }
}

void vga_soft_cursor_hide(CCOS_VGA* vga) {
    if (vga == NULL || !soft_cursor_state.enabled || !soft_cursor_state.has_saved_char) {
        return;
    }

    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;
    uint16_t pos = soft_cursor_state.y * vga->width + soft_cursor_state.x;

    // Restore the original character
    video[pos] = soft_cursor_state.saved_char;
    soft_cursor_state.has_saved_char = false;
}

void vga_soft_cursor_draw(CCOS_VGA* vga) {
    if (vga == NULL || !soft_cursor_state.enabled) {
        return;
    }

    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;
    uint16_t pos = soft_cursor_state.y * vga->width + soft_cursor_state.x;

    // Save current character if not already saved
    if (!soft_cursor_state.has_saved_char) {
        soft_cursor_state.saved_char = video[pos];
        soft_cursor_state.has_saved_char = true;
    }

    // Draw cursor as a solid block with specified foreground color
    // Create cursor entry: use solid block character with cursor colors
    uint16_t cursor_entry = (uint16_t)VGA_CURSOR_BLOCK_FULL |
                           ((uint16_t)((soft_cursor_state.bg_color << 4) | soft_cursor_state.fg_color) << 8);

    video[pos] = cursor_entry;
}

void vga_soft_cursor_update(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y) {
    if (vga == NULL || !soft_cursor_state.enabled) {
        return;
    }

    // Clamp to screen bounds
    if (x >= vga->width) {
        x = vga->width - 1;
    }
    if (y >= vga->height) {
        y = vga->height - 1;
    }

    // If position hasn't changed, nothing to do
    if (x == soft_cursor_state.x && y == soft_cursor_state.y) {
        return;
    }

    // Hide cursor from old position
    vga_soft_cursor_hide(vga);

    // Update position
    soft_cursor_state.x = x;
    soft_cursor_state.y = y;

    // Draw cursor at new position
    vga_soft_cursor_draw(vga);
}

/* ============================================================================
 * Hardware Cursor Functions
 * ============================================================================ */

/**
 * @brief Update the hardware cursor position and ensure it's visible
 *
 * Note: This function does NOT enable the hardware cursor if software cursor
 * is enabled, to avoid conflicts.
 */
static void vga_update_hardware_cursor_position(CCOS_VGA* vga) {
    if (vga == NULL) {
        return;
    }

    // Don't touch hardware cursor if software cursor is enabled
    if (soft_cursor_state.enabled) {
        return;
    }

    // First, ensure cursor is enabled and visible
    // Read current cursor start register, preserve bits, clear bit 5 (disable bit)
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_START);
    uint8_t cursor_start = inb(VGA_DATA_REGISTER);

    // Clear bit 5 to enable cursor, keep scan line value
    cursor_start = cursor_start & 0x1F;  // Clear bit 5 (00011111)
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_START);
    outb(VGA_DATA_REGISTER, cursor_start);

    // Get current position
    vga_sz_t x = (vga_sz_t)((vga->native_cursor_pos & 0xFF00) >> 8);
    vga_sz_t y = (vga_sz_t)(vga->native_cursor_pos & 0x00FF);

    // Convert 2D position to linear offset
    uint16_t pos = y * vga->width + x;

    // Write cursor location to VGA hardware
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_HIGH);
    outb(VGA_DATA_REGISTER, (uint8_t)((pos >> 8) & 0xFF));
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_LOW);
    outb(VGA_DATA_REGISTER, (uint8_t)(pos & 0xFF));
}

CCOS_VGA* vga_instance() {
    return &internal_vga_instance;
}

void system_vga_init() {
    internal_vga_instance.height = VGA_HEIGHT;
    internal_vga_instance.width = VGA_WIDTH;
    internal_vga_instance.base_addr =
        (char*)(uintptr_t)VGA_BASE_ADDR;           // NOLINT(performance-no-int-to-ptr)
    internal_vga_instance.native_cursor_pos = 0;   // start at (0, 0)
    internal_vga_instance.font_color = 0x0F;       // white font
    internal_vga_instance.background_color = 0x00; // black background

    // Enable hardware cursor by default (will be disabled if software cursor is used)
    // Note: vga_shell_init() will disable hardware cursor when software cursor is initialized
#if !VGA_USE_SOFTWARE_CURSOR
    vga_enable_cursor(true);
    vga_update_hardware_cursor_position(&internal_vga_instance);
#else
    // Software cursor will be enabled later in vga_shell_init()
    vga_enable_cursor(false);
#endif
}

// Make VGA entry (character with color attribute)
static inline uint16_t vga_entry(char c, vga_color_t font, vga_color_t background) {
    return (uint16_t)c | ((uint16_t)(background << 4 | font) << 8);
}

void vga_clear(CCOS_VGA* vga, vga_color_t background) {
    if (vga == NULL)
        return;

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

    // Update hardware cursor
    vga_update_hardware_cursor_position(vga);
}

vga_cursor_t vga_get_cursor(const CCOS_VGA* vga) {
    if (vga == NULL)
        return 0;
    return vga->native_cursor_pos;
}

void vga_set_cursor(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y) {
    if (vga == NULL)
        return;

    // Clamp to screen bounds
    if (x >= vga->width)
        x = vga->width - 1;
    if (y >= vga->height)
        y = vga->height - 1;

    // Pack cursor position: X in high byte, Y in low byte
    vga->native_cursor_pos = ((uint16_t)x << 8) | (uint16_t)y;

    // Update hardware cursor
    vga_update_hardware_cursor_position(vga);
}

// Put a single character at current cursor position and advance cursor
static void vga_putc(CCOS_VGA* vga, char c) {
    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;
    vga_sz_t x = (vga_sz_t)((vga->native_cursor_pos & 0xFF00) >> 8);
    vga_sz_t y = (vga_sz_t)(vga->native_cursor_pos & 0x00FF);

    // Hide software cursor before drawing (if enabled)
    bool soft_cursor_enabled = soft_cursor_state.enabled;
    if (soft_cursor_enabled) {
        vga_soft_cursor_hide(vga);
    }

    if (c == '\n' || c == '\r') {
        // Newline/Carriage Return: move to start of next line
        x = 0;
        y++;
        if (y >= vga->height) {
            // Scroll up by 1 line and stay on last line
            vga_scroll(vga, 1);
            y = vga->height - 1;
        }
        // Update packed cursor position for newline
        vga->native_cursor_pos = ((uint16_t)x << 8) | (uint16_t)y;
    } else if (c == '\t') {
        // Tab: move to next 8-column boundary
        vga_sz_t next_tab = ((x / 8) + 1) * 8;
        if (next_tab >= vga->width) {
            x = 0;
            y++;
            if (y >= vga->height) {
                vga_scroll(vga, 1);
                y = vga->height - 1;
            }
        } else {
            x = next_tab;
        }
        // Update packed cursor position for tab
        vga->native_cursor_pos = ((uint16_t)x << 8) | (uint16_t)y;
    } else if (c == '\b') {
        // Backspace: move cursor back one position (don't erase, shell handles that)
        if (x > 0) {
            x--;
        } else if (y > 0) {
            y--;
            x = vga->width - 1;
        }
        // Update packed cursor position for backspace
        vga->native_cursor_pos = ((uint16_t)x << 8) | (uint16_t)y;
    } else if (c >= 32 && c < 127) {
        // Printable character only - skip control characters
        uint16_t entry = vga_entry(c, vga->font_color, vga->background_color);
        video[y * vga->width + x] = entry;

        // Advance cursor
        x++;
        if (x >= vga->width) {
            x = 0;
            y++;
            if (y >= vga->height) {
                // Scroll up by 1 line and stay on last line
                vga_scroll(vga, 1);
                y = vga->height - 1;
            }
        }

        // Update packed cursor position
        vga->native_cursor_pos = ((uint16_t)x << 8) | (uint16_t)y;
    }

    // Update cursor position (hardware or software)
    if (soft_cursor_enabled) {
        vga_soft_cursor_update(vga, x, y);
    } else {
        vga_update_hardware_cursor_position(vga);
    }
}

void vga_print_string(CCOS_VGA* vga, const char* string) {
    if (vga == NULL || string == NULL)
        return;

    while (*string != '\0') {
        vga_putc(vga, *string);
        string++;
    }
}

void vga_print_stringn(CCOS_VGA* vga, const char* string, const vga_sz_t str_sz) {
    if (vga == NULL || string == NULL)
        return;

    for (vga_sz_t i = 0; i < str_sz; i++) {
        vga_putc(vga, string[i]);
    }
}

void vga_scroll(CCOS_VGA* vga, int lines) {
    if (vga == NULL || lines == 0)
        return;

    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;
    uint16_t blank = vga_entry(' ', 0x0, vga->background_color);
    vga_sz_t width = vga->width;
    vga_sz_t height = vga->height;

    if (lines > 0) {
        // Scroll up (content moves up)
        vga_sz_t scroll_lines = (vga_sz_t)lines;
        if (scroll_lines >= height) {
            // Clear entire screen if scrolling more than height
            for (vga_sz_t i = 0; i < width * height; i++) {
                video[i] = blank;
            }
        } else {
            // Move content up
            for (vga_sz_t y = 0; y < height - scroll_lines; y++) {
                for (vga_sz_t x = 0; x < width; x++) {
                    video[y * width + x] = video[(y + scroll_lines) * width + x];
                }
            }
            // Clear bottom lines
            for (vga_sz_t y = height - scroll_lines; y < height; y++) {
                for (vga_sz_t x = 0; x < width; x++) {
                    video[y * width + x] = blank;
                }
            }
        }
    } else {
        // Scroll down (content moves down)
        vga_sz_t scroll_lines = (vga_sz_t)(-lines);
        if (scroll_lines >= height) {
            // Clear entire screen if scrolling more than height
            for (vga_sz_t i = 0; i < width * height; i++) {
                video[i] = blank;
            }
        } else {
            // Move content down
            for (vga_sz_t y = height - 1; y >= scroll_lines; y--) {
                for (vga_sz_t x = 0; x < width; x++) {
                    video[y * width + x] = video[(y - scroll_lines) * width + x];
                }
            }
            // Clear top lines
            for (vga_sz_t y = 0; y < scroll_lines; y++) {
                for (vga_sz_t x = 0; x < width; x++) {
                    video[y * width + x] = blank;
                }
            }
        }
    }
}

void set_vga_property(CCOS_VGA* vga, void* data, vga_property_t what_property) {
    if (vga == NULL || data == NULL)
        return;

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
