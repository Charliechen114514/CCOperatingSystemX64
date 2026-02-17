/**
 * @file keyboard_config.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief PS/2 Keyboard driver configuration
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

/* ============================================================================
 * Keyboard I/O Ports
 * ============================================================================ */

#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64

/* ============================================================================
 * Keyboard Status Register Bits
 * ============================================================================ */

#define KBD_STATUS_OUTPUT_FULL  0x01  // Output buffer full (data available)
#define KBD_STATUS_INPUT_FULL   0x02  // Input buffer full (wait before sending)
#define KBD_STATUS_SYSFLAG      0x04  // System flag (0 = power-on reset)
#define KBD_STATUS_CMD_DATA     0x08  // 1 = data for command, 0 = data

/* ============================================================================
 * Keyboard Commands
 * ============================================================================ */

#define KBD_CMD_READ_MODE       0x20  // Read command byte
#define KBD_CMD_WRITE_MODE      0x60  // Write command byte
#define KBD_CMD_SELF_TEST       0xAA  // Self-test
#define KBD_CMD_ENABLE          0xF4  // Enable scanning
#define KBD_CMD_DISABLE         0xF5  // Disable scanning
#define KBD_CMD_RESET           0xFF  // Reset

/* ============================================================================
 * Keyboard Mode Bits (for command byte)
 * ============================================================================ */

#define KBD_MODE_ENABLE_IRQ     0x01  // Enable interrupt

/* ============================================================================
 * Scancode Constants (Set 1)
 * ============================================================================ */

#define SCANCODE_BREAK_MASK     0x80  // Bit 7 set = key release (break code)

// Special key scan codes
#define SCANCODE_LSHIFT         0x2A
#define SCANCODE_RSHIFT         0x36
#define SCANCODE_CAPSLOCK       0x3A
#define SCANCODE_ENTER          0x1C
#define SCANCODE_BACKSPACE      0x0E
#define SCANCODE_TAB            0x0F
#define SCANCODE_ESC            0x01
#define SCANCODE_SPACE          0x39
#define SCANCODE_LCTRL          0x1D
#define SCANCODE_LALT           0x38

/* ============================================================================
 * Buffer Configuration
 * ============================================================================ */

#define KEYBOARD_BUFFER_SIZE    256

/* ============================================================================
 * IRQ Configuration
 * ============================================================================ */

// Keyboard uses IRQ 1 (mapped to IDT vector 33 after PIC remap)
#define KEYBOARD_IRQ            1
