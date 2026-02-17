/**
 * @file vga_shell.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VGA backend for shell interface
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "shell/shell.h"

/* ============================================================================
 * Backend Access
 * ============================================================================ */

/**
 * @brief Get the VGA shell backend
 *
 * @return Pointer to the VGA backend implementation
 */
const shell_backend_t* vga_shell_backend_get(void);

/* ============================================================================
 * Shell Execution
 * ============================================================================ */

/**
 * @brief Run the shell on VGA console
 *
 * Convenience function that calls shell_run() with the VGA backend.
 * This will block until the shell exits.
 *
 * @return Exit status (0 for normal exit)
 */
int vga_shell_run(void);

/* ============================================================================
 * Shell Initialization
 * ============================================================================ */

/**
 * @brief Initialize the VGA shell
 *
 * Initializes the VGA shell by setting the cursor color and other
 * VGA-specific settings. This should be called once during system
 * initialization before running the VGA shell.
 */
void vga_shell_init(void);

/* ============================================================================
 * Command Initialization
 * ============================================================================ */

/**
 * @brief Initialize VGA-specific shell commands
 *
 * Registers commands that are specific to the VGA backend:
 * - cls: Clear the VGA screen
 * - color: Change font/background color
 * - goto: Move cursor to position
 * - keyboard: Show keyboard interrupt count
 * - time: Show RTC time
 * - ticks: Show timer tick count
 */
void vga_shell_init_commands(void);
