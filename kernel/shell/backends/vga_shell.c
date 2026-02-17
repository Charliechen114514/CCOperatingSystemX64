/**
 * @file vga_shell.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VGA backend implementation for shell
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "vga_shell.h"
#include "vga_shell_cursor_config.h"
#include "driver/keyboard/keyboard.h"
#include "driver/rtc/rtc.h"
#include "driver/timer/timer.h"
#include "driver/vga/vga.h"
#include "string.h"
#include "base/strhelpers.h"
#include "klogs/ksnprintf.h"

/* ============================================================================
 * Shell Initialization
 * ============================================================================ */

/**
 * @brief Initialize the VGA shell
 *
 * Sets the cursor color to ensure it remains stable during printing.
 * Cursor is set to light gray foreground on black background for visibility.
 */
void vga_shell_init(void) {
    // Initialize software cursor with configured colors
#if VGA_SHELL_USE_SOFTWARE_CURSOR
    vga_soft_cursor_init(vga_instance(), VGA_SHELL_CURSOR_FG_COLOR, VGA_SHELL_CURSOR_BG_COLOR);
#else
    // Use hardware cursor (no color control available in text mode)
    vga_enable_cursor(true);
#endif
}

/* ============================================================================
 * Backend Implementation
 * ============================================================================ */

static void vga_puts(const char* str) {
    if (str == NULL) {
        return;
    }
    vga_print_string(vga_instance(), str);
}

static void vga_putc(char c) {
    char str[2] = {c, '\0'};
    vga_print_string(vga_instance(), str);
}

static bool vga_haschar(void) {
    return keyboard_haschar();
}

static char vga_getchar(void) {
    return keyboard_getchar();
}

static void vga_clear_screen(void) {
    vga_clear(vga_instance(), VGA_COLOR_BLACK);
}

const shell_backend_t g_vga_backend = {
    .name = "vga",
    .puts = vga_puts,
    .putc = vga_putc,
    .haschar = vga_haschar,
    .getchar = vga_getchar,
    .clear = vga_clear_screen,
};

const shell_backend_t* vga_shell_backend_get(void) {
    return &g_vga_backend;
}

int vga_shell_run(void) {
    // Initialize VGA shell settings (cursor color, etc.)
    vga_shell_init();
    // Run the shell
    return shell_run(&g_vga_backend);
}

/* ============================================================================
 * VGA-Specific Commands
 * ============================================================================ */

/**
 * @brief Clear the VGA screen
 */
static int cmd_cls(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    shell_context_t* ctx = (shell_context_t*)argv[-1];
    if (ctx == NULL) {
        return -1;
    }

    vga_clear(vga_instance(), VGA_COLOR_BLACK);
    return 0;
}

/**
 * @brief Change font or background color
 */
static int cmd_color(int argc, char* argv[]) {
    shell_context_t* ctx = (shell_context_t*)argv[-1];
    if (ctx == NULL) {
        return -1;
    }

    if (argc < 2) {
        ctx->backend->puts("Usage: color [fg|bg] [0-15]\n");
        ctx->backend->puts("Colors: 0=Black, 1=Blue, 2=Green, 3=Cyan, 4=Red\n");
        ctx->backend->puts("       5=Magenta, 6=Brown, 7=LightGray, 8=DarkGray\n");
        ctx->backend->puts("       9=BrightBlue, 10=BrightGreen, 11=BrightCyan\n");
        ctx->backend->puts("       12=BrightRed, 13=BrightMagenta, 14=Yellow, 15=White\n");
        return 0;
    }

    if (argc < 3) {
        ctx->backend->puts("Usage: color [fg|bg] [0-15]\n");
        return 0;
    }

    CCOS_VGA* vga = vga_instance();
    int color_val = atoi(argv[2]);

    if (color_val < 0 || color_val > 15) {
        ctx->backend->puts("Invalid color value. Must be 0-15.\n");
        return 0;
    }

    vga_color_t color = (vga_color_t)color_val;

    if (strcmp(argv[1], "fg") == 0) {
        vga->font_color = color;
        ctx->backend->puts("Font color changed.\n");
    } else if (strcmp(argv[1], "bg") == 0) {
        vga->background_color = color;
        ctx->backend->puts("Background color changed.\n");
    } else {
        ctx->backend->puts("Usage: color [fg|bg] [0-15]\n");
    }

    return 0;
}

/**
 * @brief Move cursor to position
 */
static int cmd_goto(int argc, char* argv[]) {
    shell_context_t* ctx = (shell_context_t*)argv[-1];
    if (ctx == NULL) {
        return -1;
    }

    if (argc < 3) {
        ctx->backend->puts("Usage: goto <x> <y>\n");
        return 0;
    }

    int x = atoi(argv[1]);
    int y = atoi(argv[2]);

    if (x < 0 || y < 0) {
        ctx->backend->puts("Invalid coordinates.\n");
        return 0;
    }

    vga_set_cursor(vga_instance(), (vga_sz_t)x, (vga_sz_t)y);
    return 0;
}

/**
 * @brief Show keyboard interrupt count
 */
static int cmd_keyboard(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    shell_context_t* ctx = (shell_context_t*)argv[-1];
    if (ctx == NULL) {
        return -1;
    }

    char buffer[64];
    uint64_t intr_count = keyboard_get_interrupt_count();
    ksnprintf(buffer, sizeof(buffer), "Keyboard interrupt count: %lu\n", intr_count);
    ctx->backend->puts(buffer);

    return 0;
}

/**
 * @brief Show current RTC time
 */
static int cmd_time(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    shell_context_t* ctx = (shell_context_t*)argv[-1];
    if (ctx == NULL) {
        return -1;
    }

    rtc_time_t time;
    if (rtc_get_time(&time) == 0) {
        char buffer[32];
        ksnprintf(buffer, sizeof(buffer), "%04u-%02u-%02u %02u:%02u:%02u\n",
                  time.year, time.month, time.day_of_month,
                  time.hours, time.minutes, time.seconds);
        ctx->backend->puts(buffer);
    } else {
        ctx->backend->puts("Failed to read RTC time\n");
    }

    return 0;
}

/**
 * @brief Show timer tick count
 */
static int cmd_ticks(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    shell_context_t* ctx = (shell_context_t*)argv[-1];
    if (ctx == NULL) {
        return -1;
    }

    char buffer[32];
    ksnprintf(buffer, sizeof(buffer), "Timer ticks: %lu\n", timer_get_ticks());
    ctx->backend->puts(buffer);

    return 0;
}

/* ============================================================================
 * Initialize VGA-Specific Commands
 * ============================================================================ */

void vga_shell_init_commands(void) {
    shell_register_command("cls", "Clear the VGA screen", cmd_cls);
    shell_register_command("color", "Change font/background color", cmd_color);
    shell_register_command("goto", "Move cursor to position", cmd_goto);
    shell_register_command("keyboard", "Show keyboard interrupt count", cmd_keyboard);
    shell_register_command("time", "Show current RTC time", cmd_time);
    shell_register_command("ticks", "Show timer tick count", cmd_ticks);
}
