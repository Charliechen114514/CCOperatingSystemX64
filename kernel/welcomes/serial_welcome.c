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

// Simple wrapper for serial puts
static void sputs(const char* str) {
    sync_serial_puts(str);
}

// Print colored text
static void sputs_color(const char* str, serial_color_t color) {
    sputs(serial_color_ansi(color));
    sputs(str);
    sputs(serial_color_ansi(SERIAL_COLOR_RESET));
}

// Print a horizontal line with color
static void print_hline(int width, serial_color_t color) {
    sputs(serial_color_ansi(color));
    for (int i = 0; i < width; i++) {
        sputs("=");
    }
    sputs(serial_color_ansi(SERIAL_COLOR_RESET));
    sputs("\r\n");
}

// Print empty line with borders
static void print_empty_line(int width, serial_color_t color) {
    sputs(serial_color_ansi(color));
    sputs("|");
    for (int i = 0; i < width - 2; i++)
        sputs(" ");
    sputs("|");
    sputs(serial_color_ansi(SERIAL_COLOR_RESET));
    sputs("\r\n");
}

// Main welcome screen for serial
void serial_display_welcome(void) {
    int box_width = 60;

    // Clear screen (ANSI)
    sputs("\033[2J\033[H");

    // Print top decoration line
    print_hline(box_width, SERIAL_COLOR_BRIGHT_CYAN);
    sputs("\r\n");

    // Top border with stars
    sputs_color("/", SERIAL_COLOR_BRIGHT_WHITE);
    for (int i = 0; i < box_width - 2; i++) {
        sputs_color("*", SERIAL_COLOR_YELLOW);
    }
    sputs_color("\\", SERIAL_COLOR_BRIGHT_WHITE);
    sputs("\r\n");

    // Empty line
    print_empty_line(box_width, SERIAL_COLOR_BRIGHT_CYAN);

    // Logo with gradient colors
    sputs(serial_color_ansi(SERIAL_COLOR_BRIGHT_CYAN));
    sputs("|");
    sputs(serial_color_ansi(SERIAL_COLOR_RESET));

    // Center the logo (6 lines, each ~30 chars)
    int logo_padding = (box_width - 2 - 30) / 2;
    for (int i = 0; i < logo_padding; i++)
        sputs(" ");

    serial_color_t logo_colors[] = {SERIAL_COLOR_CYAN,        SERIAL_COLOR_BRIGHT_CYAN,
                                    SERIAL_COLOR_BRIGHT_BLUE, SERIAL_COLOR_BRIGHT_MAGENTA,
                                    SERIAL_COLOR_MAGENTA,     SERIAL_COLOR_BRIGHT_CYAN};

    for (int i = 0; i < 6; i++) {
        sputs(serial_color_ansi(logo_colors[i]));
        sputs(CCOS_LOGO[i]);
        sputs(serial_color_ansi(SERIAL_COLOR_RESET));
        sputs("\r\n|");
        if (i < 5) {
            sputs(serial_color_ansi(SERIAL_COLOR_RESET));
            for (int j = 0; j < logo_padding; j++)
                sputs(" ");
        }
    }
    sputs("\r\n");

    // Empty line
    print_empty_line(box_width, SERIAL_COLOR_BRIGHT_CYAN);

    // System name with bold yellow
    sputs(serial_color_ansi(SERIAL_COLOR_BRIGHT_CYAN));
    sputs("|");
    sputs(serial_color_ansi(SERIAL_COLOR_RESET));

    int name_padding = (box_width - 2 - 24) / 2;
    for (int i = 0; i < name_padding; i++)
        sputs(" ");

    sputs(ANSI_BOLD);
    sputs(serial_color_ansi(SERIAL_COLOR_YELLOW));
    sputs(CCOS_NAME);
    sputs(ANSI_RESET);
    sputs("\r\n");

    // Empty line
    print_empty_line(box_width, SERIAL_COLOR_BRIGHT_CYAN);

    // Version info
    sputs(serial_color_ansi(SERIAL_COLOR_BRIGHT_CYAN));
    sputs("|");
    sputs(serial_color_ansi(SERIAL_COLOR_RESET));

    const char* ver_prefix = "Version ";
    const char* ver_suffix = " [Alpha]";

    int ver_len = 9 + 8 + 8; // "Version " + "0.1.0" + " [Alpha]"
    int ver_padding = (box_width - 2 - ver_len) / 2;
    for (int i = 0; i < ver_padding; i++)
        sputs(" ");

    sputs_color(ver_prefix, SERIAL_COLOR_BRIGHT_GREEN);
    sputs_color(CCOS_VERSION, SERIAL_COLOR_BRIGHT_GREEN);
    sputs_color(ver_suffix, SERIAL_COLOR_YELLOW);
    sputs("\r\n");

    // Empty line
    print_empty_line(box_width, SERIAL_COLOR_BRIGHT_CYAN);

    // Build info
    sputs(serial_color_ansi(SERIAL_COLOR_BRIGHT_CYAN));
    sputs("|");
    sputs(serial_color_ansi(SERIAL_COLOR_RESET));

    const char* build_text = "Build: ";
    const char* build_type = CCOS_BUILD_TYPE;

    int build_len = 7 + 10;
    int build_padding = (box_width - 2 - build_len) / 2;
    for (int i = 0; i < build_padding; i++)
        sputs(" ");

    sputs(ANSI_DIM);
    sputs_color(build_text, SERIAL_COLOR_GRAY);
    sputs_color(build_type, SERIAL_COLOR_BRIGHT_WHITE);
    sputs(ANSI_RESET);
    sputs("\r\n");

    // Empty line
    print_empty_line(box_width, SERIAL_COLOR_BRIGHT_CYAN);

    // Fun separator
    sputs(serial_color_ansi(SERIAL_COLOR_BRIGHT_CYAN));
    sputs("|");
    sputs(serial_color_ansi(SERIAL_COLOR_RESET));

    int sep_padding = (box_width - 2 - 26) / 2;
    for (int i = 0; i < sep_padding; i++)
        sputs(" ");

    sputs_color("~ ", SERIAL_COLOR_BRIGHT_MAGENTA);
    sputs_color("Powered by C & CMake", SERIAL_COLOR_BRIGHT_MAGENTA);
    sputs_color(" ~", SERIAL_COLOR_BRIGHT_MAGENTA);
    sputs("\r\n");

    // Empty line
    print_empty_line(box_width, SERIAL_COLOR_BRIGHT_CYAN);

    // Bottom border
    sputs_color("\\", SERIAL_COLOR_BRIGHT_WHITE);
    for (int i = 0; i < box_width - 2; i++) {
        sputs_color("*", SERIAL_COLOR_YELLOW);
    }
    sputs_color("/", SERIAL_COLOR_BRIGHT_WHITE);
    sputs("\r\n");

    // Bottom decoration
    print_hline(box_width, SERIAL_COLOR_BRIGHT_CYAN);
    sputs("\r\n");

    // Extra info line
    sputs_color("  >> Serial Console Ready <<", SERIAL_COLOR_BRIGHT_GREEN);
    sputs("\r\n");

    // Final reset
    sputs(serial_color_ansi(SERIAL_COLOR_RESET));
}
