/**
 * @file shell.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Generic shell core implementation
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "shell.h"
#include "klogs/kprintf.h"
#include "klogs/ksnprintf.h"
#include "string.h"

/* ============================================================================
 * Command Registry
 * ============================================================================ */

#define SHELL_MAX_COMMANDS 32

typedef struct shell_command {
    char name[32];
    char description[64];
    int (*handler)(int argc, char* argv[]);
} shell_command_t;

static shell_command_t g_commands[SHELL_MAX_COMMANDS];
static int g_command_count = 0;

/* ============================================================================
 * Built-in Commands
 * ============================================================================ */

static int cmd_help(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    shell_context_t* ctx = (shell_context_t*)argv[-1]; // Hack: get context from argv
    if (ctx == NULL || ctx->backend == NULL) {
        return -1;
    }

    ctx->backend->puts("Available commands:\n");
    for (int i = 0; i < g_command_count; i++) {
        ctx->backend->puts("  ");
        ctx->backend->puts(g_commands[i].name);

        // Pad for alignment
        int len = strlen(g_commands[i].name);
        for (int j = len; j < 12; j++) {
            ctx->backend->puts(" ");
        }

        ctx->backend->puts("- ");
        ctx->backend->puts(g_commands[i].description);
        ctx->backend->puts("\n");
    }
    return 0;
}

static int cmd_clear(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    shell_context_t* ctx = (shell_context_t*)argv[-1];
    if (ctx && ctx->backend && ctx->backend->clear) {
        ctx->backend->clear();
    }
    return 0;
}

static int cmd_exit(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return 1; // Special return code to exit shell
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static void register_builtin_commands(void) {
    static bool registered = false;
    if (registered) {
        return;
    }

    shell_register_command("help", "Show this help message", cmd_help);
    shell_register_command("clear", "Clear the screen", cmd_clear);
    shell_register_command("exit", "Exit the shell", cmd_exit);
    registered = true;
}

static shell_command_t* find_command(const char* name) {
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, name) == 0) {
            return &g_commands[i];
        }
    }
    return NULL;
}

static int parse_command(char* cmd_line, char* argv[]) {
    int argc = 0;
    char* p = cmd_line;

    // Skip leading whitespace
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    while (*p != '\0' && argc < SHELL_MAX_ARGS) {
        argv[argc++] = p;

        // Find end of token
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        // Null-terminate token
        *p++ = '\0';

        // Skip whitespace
        while (*p == ' ' || *p == '\t') {
            p++;
        }
    }

    return argc;
}

static void execute_command(shell_context_t* ctx) {
    // Null-terminate the command
    ctx->cmd_buffer[ctx->cmd_pos] = '\0';

    if (ctx->cmd_pos == 0) {
        // Empty command, just show prompt
        return;
    }

    // Parse command
    char* argv[SHELL_MAX_ARGS + 1];
    argv[0] = (char*)ctx; // Store context pointer in argv[-1] for commands

    int argc = parse_command(ctx->cmd_buffer, &argv[1]);

    if (argc == 0) {
        ctx->backend->puts(ctx->prompt);
        return;
    }

    // Find and execute command
    shell_command_t* cmd = find_command(argv[1]);
    if (cmd == NULL) {
        ctx->backend->puts("Unknown command: ");
        ctx->backend->puts(argv[1]);
        ctx->backend->puts("\n");
    } else {
        int result = cmd->handler(argc, &argv[1]);
        if (result == 1) {
            // Exit command
            ctx->running = false;
            return;
        }
    }

    // Show prompt
    ctx->backend->puts(ctx->prompt);
}

/* ============================================================================
 * Shell Core API
 * ============================================================================ */

shell_context_t* shell_create(const shell_backend_t* backend) {
    if (backend == NULL) {
        return NULL;
    }

    static shell_context_t g_shell_ctx;
    g_shell_ctx.backend = backend;
    g_shell_ctx.cmd_pos = 0;
    g_shell_ctx.running = true;
    strcpy(g_shell_ctx.prompt, "$ ");

    register_builtin_commands();

    return &g_shell_ctx;
}

bool shell_step(shell_context_t* ctx) {
    if (ctx == NULL || !ctx->running) {
        return false;
    }

    // Check for input
    if (ctx->backend->haschar()) {
        char c = ctx->backend->getchar();

        if (c == '\r' || c == '\n') {
            // End of line
            ctx->backend->puts("\r\n");
            execute_command(ctx);
            ctx->cmd_pos = 0;
        } else if (c == '\b' || c == 127) {
            // Backspace
            if (ctx->cmd_pos > 0) {
                ctx->cmd_pos--;
                ctx->backend->puts("\b \b");
            }
        } else if (c >= 32 && c < 127) {
            // Printable character
            if (ctx->cmd_pos < SHELL_CMD_BUFFER_SIZE - 1) {
                ctx->cmd_buffer[ctx->cmd_pos++] = c;
                ctx->backend->putc(c);
            }
        }
        // Ignore other control characters
    }

    return ctx->running;
}

int shell_run(const shell_backend_t* backend) {
    shell_context_t* ctx = shell_create(backend);
    if (ctx == NULL) {
        return -1;
    }

    // Show welcome message
    backend->puts("\n=== CCOS Shell ===\n");
    backend->puts("Type 'help' for available commands\n");
    backend->puts(ctx->prompt);

    // Main loop
    while (ctx->running) {
        if (!shell_step(ctx)) {
            break;
        }
        // Small pause to reduce CPU usage
        __asm__ volatile("pause");
    }

    backend->puts("\nShell exited.\n");
    return 0;
}

void shell_destroy(shell_context_t* ctx) {
    (void)ctx;
    // Static context, nothing to free
}

/* ============================================================================
 * Command Registration API
 * ============================================================================ */

int shell_register_command(const char* name, const char* description,
                           int (*handler)(int argc, char* argv[])) {
    if (name == NULL || handler == NULL) {
        return -1;
    }

    // Check for duplicate
    if (find_command(name) != NULL) {
        return -1;
    }

    // Check if full
    if (g_command_count >= SHELL_MAX_COMMANDS) {
        return -1;
    }

    // Add command
    strncpy(g_commands[g_command_count].name, name, sizeof(g_commands[g_command_count].name) - 1);
    g_commands[g_command_count].name[sizeof(g_commands[g_command_count].name) - 1] = '\0';

    if (description != NULL) {
        strncpy(g_commands[g_command_count].description, description,
                sizeof(g_commands[g_command_count].description) - 1);
        g_commands[g_command_count]
            .description[sizeof(g_commands[g_command_count].description) - 1] = '\0';
    } else {
        g_commands[g_command_count].description[0] = '\0';
    }

    g_commands[g_command_count].handler = handler;
    g_command_count++;

    return 0;
}

int shell_unregister_command(const char* name) {
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, name) == 0) {
            // Shift remaining commands
            for (int j = i; j < g_command_count - 1; j++) {
                g_commands[j] = g_commands[j + 1];
            }
            g_command_count--;
            return 0;
        }
    }
    return -1;
}
