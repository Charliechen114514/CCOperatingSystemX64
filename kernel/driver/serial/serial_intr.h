/**
 * @file serial_intr.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Interrupt-driven serial communication interface
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
 * @brief Initialize UART in interrupt-driven mode
 *
 * Must be called AFTER interrupt_init() and BEFORE interrupt_finalize().
 *
 * @return 0 on success, negative error code on failure
 */
int uart_init_intr_mode(void);

/* ============================================================================
 * Asynchronous Transmit
 * ============================================================================ */

/**
 * @brief Send a string asynchronously via interrupt
 *
 * @param str String to send
 */
void async_serial_puts(const char* str);

/**
 * @brief Send a character asynchronously via interrupt
 *
 * @param c Character to send
 */
void async_serial_putc(char c);

/* ============================================================================
 * Receive
 * ============================================================================ */

/**
 * @brief Check if data is available
 *
 * @return true if data available
 * @return false if no data
 */
bool uart_haschar(void);

/**
 * @brief Get a character (blocking)
 *
 * @return char Received character
 */
char uart_getchar(void);

/**
 * @brief Try to get a character (non-blocking)
 *
 * @param c Pointer to store character
 * @return 0 on success, -1 if no data
 */
int uart_try_getchar(char* c);

/* ============================================================================
 * Echo Control
 * ============================================================================ */

/**
 * @brief Set echo on/off
 *
 * @param enable true to enable, false to disable
 */
void uart_set_echo(bool enable);

/**
 * @brief Get echo state
 *
 * @return true if enabled
 */
bool uart_get_echo(void);

/* ============================================================================
 * Debug/Diagnostics
 * ============================================================================ */

/**
 * @brief Get the UART interrupt handler invocation count
 *
 * @return uint64_t Number of times the UART interrupt was triggered
 */
uint64_t uart_get_interrupt_count(void);

/**
 * @brief Debug function to check UART registers (using sync output)
 *
 * Prints current UART register values via sync_serial_puts
 */
void uart_dump_registers(void);
