/**
 * @file serial_shell.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Serial backend implementation for shell
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "serial_shell.h"
#include "driver/serial/serial_intr.h"
#include "driver/rtc/rtc.h"
#include "driver/timer/timer.h"
#include "string.h"
#include "klogs/ksnprintf.h"

/* ============================================================================
 * Backend Implementation
 * ============================================================================ */

static void serial_puts(const char* str) {
    async_serial_puts(str);
}

static void serial_putc(char c) {
    async_serial_putc(c);
}

static bool serial_haschar(void) {
    return uart_haschar();
}

static char serial_getchar(void) {
    return uart_getchar();
}

static void serial_clear(void) {
    async_serial_puts("\033[2J\033[H");  // ANSI clear screen and home cursor
}

const shell_backend_t g_serial_backend = {
    .name = "serial",
    .puts = serial_puts,
    .putc = serial_putc,
    .haschar = serial_haschar,
    .getchar = serial_getchar,
    .clear = serial_clear,
};

const shell_backend_t* serial_shell_backend_get(void) {
    return &g_serial_backend;
}

int serial_shell_run(void) {
    return shell_run(&g_serial_backend);
}

/* ============================================================================
 * Serial-Specific Commands
 * ============================================================================ */

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

static int cmd_echo(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    shell_context_t* ctx = (shell_context_t*)argv[-1];
    if (ctx == NULL) {
        return -1;
    }

    if (argc >= 2) {
        if (strcmp(argv[1], "on") == 0) {
            uart_set_echo(true);
            ctx->backend->puts("Echo enabled\n");
        } else if (strcmp(argv[1], "off") == 0) {
            uart_set_echo(false);
            ctx->backend->puts("Echo disabled\n");
        } else {
            ctx->backend->puts("Usage: echo [on|off]\n");
        }
    } else {
        bool echo = uart_get_echo();
        ctx->backend->puts(echo ? "Echo is on\n" : "Echo is off\n");
    }

    return 0;
}

static int cmd_uart(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    shell_context_t* ctx = (shell_context_t*)argv[-1];
    if (ctx == NULL) {
        return -1;
    }

    char buffer[64];
    uint64_t intr_count = uart_get_interrupt_count();
    ksnprintf(buffer, sizeof(buffer), "UART interrupt count: %lu\n", intr_count);
    ctx->backend->puts(buffer);

    return 0;
}

/* ============================================================================
 * Initialize Serial-Specific Commands
 * ============================================================================ */

void serial_shell_init_commands(void) {
    shell_register_command("time", "Show current RTC time", cmd_time);
    shell_register_command("ticks", "Show timer tick count", cmd_ticks);
    shell_register_command("echo", "Control echo [on|off]", cmd_echo);
    shell_register_command("uart", "Show UART interrupt count", cmd_uart);
}
