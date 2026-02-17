/**
 * @file keyboard.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief PS/2 Keyboard driver interface
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "defines/types.h"

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * @brief Initialize the keyboard driver in interrupt mode
 *
 * Must be called AFTER interrupt_init() and BEFORE interrupt_finalize().
 *
 * @return 0 on success, negative error code on failure
 */
int keyboard_init(void);

/* ============================================================================
 * Input Functions
 * ============================================================================ */

/**
 * @brief Check if a character is available
 *
 * @return true if at least one character is available
 */
bool keyboard_haschar(void);

/**
 * @brief Get a character (blocking)
 *
 * Waits until a character is available.
 *
 * @return char The character read
 */
char keyboard_getchar(void);

/**
 * @brief Try to get a character (non-blocking)
 *
 * @param c Pointer to store character
 * @return 0 on success, -1 if no data available
 */
int keyboard_try_getchar(char* c);

/* ============================================================================
 * Debug/Diagnostics
 * ============================================================================ */

/**
 * @brief Get the keyboard interrupt handler invocation count
 *
 * @return uint64_t Number of times the keyboard interrupt was triggered
 */
uint64_t keyboard_get_interrupt_count(void);
