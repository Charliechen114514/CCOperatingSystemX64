/**
 * @file shell.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Generic shell interface and core functionality
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "defines/types.h"

/* ============================================================================
 * Shell Backend Interface
 * ============================================================================ */

/**
 * @brief Shell backend operations
 *
 * All backends must implement these functions to provide I/O for the shell.
 */
typedef struct shell_backend {
    const char* name;

    /**
     * @brief Write a string to the output
     */
    void (*puts)(const char* str);

    /**
     * @brief Write a single character to the output
     */
    void (*putc)(char c);

    /**
     * @brief Check if input is available
     * @return true if at least one character is available
     */
    bool (*haschar)(void);

    /**
     * @brief Get a character from input (blocking)
     * @return The character read
     */
    char (*getchar)(void);

    /**
     * @brief Optional: Clear the screen
     */
    void (*clear)(void);

} shell_backend_t;

/* ============================================================================
 * Shell Configuration
 * ============================================================================ */

#define SHELL_CMD_BUFFER_SIZE 128
#define SHELL_MAX_ARGS 16

typedef struct shell_context {
    const shell_backend_t* backend;
    char cmd_buffer[SHELL_CMD_BUFFER_SIZE];
    int cmd_pos;
    char prompt[32];
    bool running;
} shell_context_t;

/* ============================================================================
 * Shell Core API
 * ============================================================================ */

/**
 * @brief Initialize and run the shell with the given backend
 *
 * This function initializes the shell context and enters the main loop.
 * It will block until the shell exits.
 *
 * @param backend Pointer to the backend implementation
 * @return Exit status (0 for normal exit)
 */
int shell_run(const shell_backend_t* backend);

/**
 * @brief Create a shell context for external control
 *
 * @param backend Pointer to the backend implementation
 * @return Pointer to the created context
 */
shell_context_t* shell_create(const shell_backend_t* backend);

/**
 * @brief Process one iteration of the shell
 *
 * This function checks for input and processes it.
 * Returns false when the shell should exit.
 *
 * @param ctx Shell context
 * @return true if shell should continue, false if it should exit
 */
bool shell_step(shell_context_t* ctx);

/**
 * @brief Destroy a shell context
 *
 * @param ctx Shell context to destroy
 */
void shell_destroy(shell_context_t* ctx);

/* ============================================================================
 * Built-in Commands
 * ============================================================================ */

/**
 * @brief Register a custom command
 *
 * @param name Command name
 * @param description Help text
 * @param handler Function to handle the command
 * @return 0 on success, -1 on failure (duplicate or full)
 */
int shell_register_command(const char* name, const char* description,
                           int (*handler)(int argc, char* argv[]));

/**
 * @brief Unregister a command
 *
 * @param name Command name to unregister
 * @return 0 on success, -1 if not found
 */
int shell_unregister_command(const char* name);
