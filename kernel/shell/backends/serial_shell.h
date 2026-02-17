/**
 * @file serial_shell.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Serial backend for shell
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "shell/shell.h"

/**
 * @brief Get the serial shell backend
 *
 * @return Pointer to the serial backend implementation
 */
const shell_backend_t* serial_shell_backend_get(void);

/**
 * @brief Run the shell on serial port
 *
 * Convenience function that calls shell_run() with the serial backend.
 *
 * @return Exit status
 */
int serial_shell_run(void);

/**
 * @brief Initialize serial-specific shell commands
 *
 * Registers commands like 'time', 'ticks', 'echo' that are specific
 * to the serial backend.
 */
void serial_shell_init_commands(void);
