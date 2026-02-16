#include "assert_action_backend.h"
#include "base/string.h"
#include "driver/vga/vga.h"
#include "driver/vga/vga_helpers.h"

/* Internal helper: convert integer to string */
static int int_to_str(int64_t value, char* buf, int buf_size) {
    if (buf_size < 2) {
        return 0;
    }

    int i = 0;
    bool is_negative = false;

    if (value < 0) {
        is_negative = true;
        value = -value;
    }

    /* Handle zero case */
    if (value == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return i;
    }

    /* Convert digits in reverse order */
    char tmp[24];
    int tmp_idx = 0;
    while (value > 0 && tmp_idx < 23) {
        tmp[tmp_idx++] = (char)('0' + (value % 10));
        value /= 10;
    }

    /* Add sign and copy to buffer in correct order */
    if (is_negative && i < buf_size - 1) {
        buf[i++] = '-';
    }

    while (tmp_idx > 0 && i < buf_size - 1) {
        buf[i++] = tmp[--tmp_idx];
    }
    buf[i] = '\0';

    return i;
}

void assert_backend_to_vga(const char* file, int line, const char* func, const char* expr_str) {
    CCOS_VGA* vga = vga_instance();
    if (vga == NULL) {
        return;
    }

    /* Save current cursor position and colors */
    vga_cursor_t saved_cursor = vga_get_cursor(vga);
    vga_color_t saved_font = vga->font_color;
    vga_color_t saved_bg = vga->background_color;

    /* Set error colors: white text on red background */
    vga->font_color = VGA_COLOR_WHITE;
    vga->background_color = VGA_COLOR_RED;

    // /* Clear screen with red background for visibility */
    // vga_clear(vga, VGA_COLOR_RED);

    // /* Move cursor to top-left */
    // vga_set_cursor(vga, 0, 0);
    const uint8_t next_line_pos = vga_cursor_y(vga);
    vga_set_cursor(vga, 0, next_line_pos + 1);

    /* Print header */
    vga_print_string(vga, "*** ASSERTION FAILED ***\n\n");

    /* Print file location */
    vga_print_string(vga, "File: ");
    if (file != NULL) {
        vga_print_string(vga, file);
    }
    vga_print_string(vga, ":");

    /* Convert and print line number */
    char line_buf[16];
    int_to_str(line, line_buf, sizeof(line_buf));
    vga_print_string(vga, line_buf);
    vga_print_string(vga, "\n");

    /* Print function name */
    vga_print_string(vga, "Function: ");
    if (func != NULL) {
        vga_print_string(vga, func);
    }
    vga_print_string(vga, "\n");

    /* Print failed expression */
    vga_print_string(vga, "Expression: ");
    if (expr_str != NULL) {
        vga_print_string(vga, expr_str);
    }
    vga_print_string(vga, "\n");

    /* Restore original cursor position and colors */
    vga->font_color = saved_font;
    vga->background_color = saved_bg;
    vga_set_cursor(vga, (saved_cursor & 0xFF00) >> 8, saved_cursor & 0x00FF);
}

void assert_failed_action(void) {
#ifndef NDEBUG
    // OK in debug mode
    // in debug mode, int 3 is expected to shut the system once
    // to notify the debugger hang once, and enable to trace the
    // calls...
    // PS: if no idt is registered, then the qemu will sucks to
    // request reboot :), so, if any Reboots occurs in
    // pre-idt-registered stage, these will lead to reboot.
    __asm__ volatile("int3"); // Halt me once!
#endif
    // disable irqs at no conditions...
    __asm__ volatile("cli"); // close the irqs, system dead...
    while (1) {              // Halt the System
        __asm__ volatile("hlt");
    }
}
