/**
 * @file serial_welcome.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Boot Welcome with ANSI colors for Serial Terminal
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ccos_config.h"
#include "driver/serial/serial.h"
#include "driver/serial/serial_color.h"

// ==================== COOL SERIAL WELCOME SCREEN ====================

// ASCII Art Logo - bigger and cooler
static const char* CCOS_LOGO[] = {
    "  _____  _____ _____    ___    ",    " /  _  \\/  ___//  __ \\  /   |   ",
    " | | | |\\ `--.| /  \\/ / /| |   ",  " | | | | `--. \\ |    / /_| |   ",
    " | |_| |/\\__/ / \\__/\\____  |   ", "  \\___/ \\____/ \\____/    |_|   "};

static const char* CCOS_NAME = "CCOperating System X64";

// ANSI style codes
static const char* ANSI_BOLD = "\033[1m";
static const char* ANSI_DIM = "\033[2m";
static const char* ANSI_RESET = "\033[0m";

// Print colored text
static void sync_serial_puts_color(const char* str, serial_color_t color) {
    sync_serial_puts(serial_color_ansi(color));
    sync_serial_puts(str);
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_RESET));
}

// Print a horizontal line with color
static void print_hline(int width, serial_color_t color) {
    sync_serial_puts(serial_color_ansi(color));
    for (int i = 0; i < width; i++) {
        sync_serial_puts("=");
    }
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_RESET));
    sync_serial_puts("\r\n");
}

// Print empty line with borders
static void print_empty_line(int width, serial_color_t color) {
    sync_serial_puts(serial_color_ansi(color));
    sync_serial_puts("|");
    for (int i = 0; i < width - 2; i++)
        sync_serial_puts(" ");
    sync_serial_puts("|");
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_RESET));
    sync_serial_puts("\r\n");
}

// Main welcome screen for serial
void serial_display_welcome(void) {
    int box_width = 60;

    // Clear screen (ANSI)
    sync_serial_puts("\033[2J\033[H");

    // Print top decoration line
    print_hline(box_width, SERIAL_COLOR_BRIGHT_CYAN);
    sync_serial_puts("\r\n");

    // Top border with stars
    sync_serial_puts_color("/", SERIAL_COLOR_BRIGHT_WHITE);
    for (int i = 0; i < box_width - 2; i++) {
        sync_serial_puts_color("*", SERIAL_COLOR_YELLOW);
    }
    sync_serial_puts_color("\\", SERIAL_COLOR_BRIGHT_WHITE);
    sync_serial_puts("\r\n");

    // Empty line
    print_empty_line(box_width, SERIAL_COLOR_BRIGHT_CYAN);

    // Logo with gradient colors
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_BRIGHT_CYAN));
    sync_serial_puts("|");
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_RESET));

    // Center the logo (6 lines, each ~30 chars)
    int logo_padding = (box_width - 2 - 30) / 2;
    for (int i = 0; i < logo_padding; i++)
        sync_serial_puts(" ");

    serial_color_t logo_colors[] = {SERIAL_COLOR_CYAN,        SERIAL_COLOR_BRIGHT_CYAN,
                                    SERIAL_COLOR_BRIGHT_BLUE, SERIAL_COLOR_BRIGHT_MAGENTA,
                                    SERIAL_COLOR_MAGENTA,     SERIAL_COLOR_BRIGHT_CYAN};

    for (int i = 0; i < 6; i++) {
        sync_serial_puts(serial_color_ansi(logo_colors[i]));
        sync_serial_puts(CCOS_LOGO[i]);
        sync_serial_puts(serial_color_ansi(SERIAL_COLOR_RESET));
        sync_serial_puts("\r\n|");
        if (i < 5) {
            sync_serial_puts(serial_color_ansi(SERIAL_COLOR_RESET));
            for (int j = 0; j < logo_padding; j++)
                sync_serial_puts(" ");
        }
    }
    sync_serial_puts("\r\n");

    // Empty line
    print_empty_line(box_width, SERIAL_COLOR_BRIGHT_CYAN);

    // System name with bold yellow
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_BRIGHT_CYAN));
    sync_serial_puts("|");
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_RESET));

    int name_padding = (box_width - 2 - 24) / 2;
    for (int i = 0; i < name_padding; i++)
        sync_serial_puts(" ");

    sync_serial_puts(ANSI_BOLD);
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_YELLOW));
    sync_serial_puts(CCOS_NAME);
    sync_serial_puts(ANSI_RESET);
    sync_serial_puts("\r\n");

    // Empty line
    print_empty_line(box_width, SERIAL_COLOR_BRIGHT_CYAN);

    // Version info
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_BRIGHT_CYAN));
    sync_serial_puts("|");
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_RESET));

    const char* ver_prefix = "Version ";
    const char* ver_suffix = " [Alpha]";

    int ver_len = 9 + 8 + 8; // "Version " + "0.1.0" + " [Alpha]"
    int ver_padding = (box_width - 2 - ver_len) / 2;
    for (int i = 0; i < ver_padding; i++)
        sync_serial_puts(" ");

    sync_serial_puts_color(ver_prefix, SERIAL_COLOR_BRIGHT_GREEN);
    sync_serial_puts_color(CCOS_VERSION, SERIAL_COLOR_BRIGHT_GREEN);
    sync_serial_puts_color(ver_suffix, SERIAL_COLOR_YELLOW);
    sync_serial_puts("\r\n");

    // Empty line
    print_empty_line(box_width, SERIAL_COLOR_BRIGHT_CYAN);

    // Build info
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_BRIGHT_CYAN));
    sync_serial_puts("|");
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_RESET));

    const char* build_text = "Build: ";
    const char* build_type = CCOS_BUILD_TYPE;

    int build_len = 7 + 10;
    int build_padding = (box_width - 2 - build_len) / 2;
    for (int i = 0; i < build_padding; i++)
        sync_serial_puts(" ");

    sync_serial_puts(ANSI_DIM);
    sync_serial_puts_color(build_text, SERIAL_COLOR_GRAY);
    sync_serial_puts_color(build_type, SERIAL_COLOR_BRIGHT_WHITE);
    sync_serial_puts(ANSI_RESET);
    sync_serial_puts("\r\n");

    // Empty line
    print_empty_line(box_width, SERIAL_COLOR_BRIGHT_CYAN);

    // Fun separator
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_BRIGHT_CYAN));
    sync_serial_puts("|");
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_RESET));

    int sep_padding = (box_width - 2 - 26) / 2;
    for (int i = 0; i < sep_padding; i++)
        sync_serial_puts(" ");

    sync_serial_puts_color("~ ", SERIAL_COLOR_BRIGHT_MAGENTA);
    sync_serial_puts_color("Powered by C & CMake", SERIAL_COLOR_BRIGHT_MAGENTA);
    sync_serial_puts_color(" ~", SERIAL_COLOR_BRIGHT_MAGENTA);
    sync_serial_puts("\r\n");

    // Empty line
    print_empty_line(box_width, SERIAL_COLOR_BRIGHT_CYAN);

    // Bottom border
    sync_serial_puts_color("\\", SERIAL_COLOR_BRIGHT_WHITE);
    for (int i = 0; i < box_width - 2; i++) {
        sync_serial_puts_color("*", SERIAL_COLOR_YELLOW);
    }
    sync_serial_puts_color("/", SERIAL_COLOR_BRIGHT_WHITE);
    sync_serial_puts("\r\n");

    // Bottom decoration
    print_hline(box_width, SERIAL_COLOR_BRIGHT_CYAN);
    sync_serial_puts("\r\n");

    // Extra info line
    sync_serial_puts_color("  >> Serial Console Ready <<", SERIAL_COLOR_BRIGHT_GREEN);
    sync_serial_puts("\r\n");

    // Final reset
    sync_serial_puts(serial_color_ansi(SERIAL_COLOR_RESET));
}
