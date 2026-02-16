/**
 * @file serial.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Serial port driver for kernel debug output (UART 16550)
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "defines/types.h"
#include "serial_config.h"

/**
 * @brief Initialize the serial port (COM1) for communication.
 *        Configured to 115200 baud, 8 data bits, no parity, 1 stop bit (8N1).
 *
 * @return true  - if serial port was successfully initialized
 * @return false - if serial port is not available or initialization failed
 */
bool serial_init(void);

/**
 * @brief Send a null-terminated string through the serial port (blocking).
 *
 * @param str - string to send
 */
void sync_serial_puts(const char* str);
