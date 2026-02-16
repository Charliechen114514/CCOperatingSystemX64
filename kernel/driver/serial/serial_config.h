/**
 * @file serial_config.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Configures of the serials
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

// COM1 base I/O port (standard for x86)
#define SERIAL_COM1_BASE 0x3F8

// UART 16550 register offsets
#define SERIAL_DATA_REG(base) ((base) + 0)        // Data register (read/write)
#define SERIAL_INT_ENABLE_REG(base) ((base) + 1)  // Interrupt enable register
#define SERIAL_FIFO_CTRL_REG(base) ((base) + 2)   // FIFO control register
#define SERIAL_LINE_CTRL_REG(base) ((base) + 3)   // Line control register
#define SERIAL_MODEM_CTRL_REG(base) ((base) + 4)  // Modem control register
#define SERIAL_LINE_STATUS_REG(base) ((base) + 5) // Line status register

// Line status register bits
#define SERIAL_LSR_READY_TO_SEND 0x20 // Transmit hold register empty
