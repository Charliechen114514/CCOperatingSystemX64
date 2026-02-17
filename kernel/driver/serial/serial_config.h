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
#define SERIAL_LSR_DATA_READY 0x01    // Data ready
#define SERIAL_LSR_OVERRUN 0x02       // Overrun error
#define SERIAL_LSR_PARITY 0x04        // Parity error
#define SERIAL_LSR_FRAME 0x08         // Frame error
#define SERIAL_LSR_BREAK 0x10         // Break interrupt
#define SERIAL_LSR_THRE 0x20          // Transmitter hold register empty
#define SERIAL_LSR_TEMT 0x40          // Transmitter empty

// Interrupt identification register (IIR)
#define SERIAL_INT_ID_REG(base) ((base) + 2)
#define SERIAL_IIR_PENDING 0x01       // Interrupt pending (0 = interrupt pending)
#define SERIAL_IIR_SOURCE_MASK 0x0E   // Interrupt source mask
#define SERIAL_IIR_RX_READY 0x04      // Receiver data available
#define SERIAL_IIR_TX_EMPTY 0x02      // Transmitter hold register empty
#define SERIAL_IIR_RX_STATUS 0x06     // Receiver line status

// Interrupt enable register (IER) bits
#define SERIAL_IER_RX_READY 0x01      // Enable receiver data available interrupt
#define SERIAL_IER_TX_EMPTY 0x02      // Enable transmitter hold register empty interrupt
#define SERIAL_IER_RX_STATUS 0x04     // Enable receiver line status interrupt
#define SERIAL_IER_MODEM 0x08         // Enable modem status interrupt

/* ============================================================================
 * Interrupt-Driven UART Configuration
 * ============================================================================ */

#define UART_RX_BUFFER_SIZE 256
#define UART_TX_BUFFER_SIZE 256
#define UART_COM1_IRQ 4
