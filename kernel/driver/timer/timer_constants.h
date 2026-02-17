/**
 * @file timer_constants.h
 * @brief PIT 8253/8254 Timer Constants
 */

#pragma once

/* ============================================================================
 * PIT I/O Ports
 * ============================================================================ */

#define PIT_CMD_PORT        0x43    // Command port
#define PIT_CHANNEL_0_DATA  0x40    // Channel 0 data port (system timer)
#define PIT_CHANNEL_1_DATA  0x41    // Channel 1 data port (unused)
#define PIT_CHANNEL_2_DATA  0x42    // Channel 2 data port (PC speaker)

/* ============================================================================
 * PIT Command Register Bits
 * ============================================================================ */

// Channel selection
#define PIT_CHANNEL_0       0x00    // Channel 0
#define PIT_CHANNEL_1       0x40    // Channel 1
#define PIT_CHANNEL_2       0x80    // Channel 2
#define PIT_READ_BACK_CMD   0xC0    // Read-back command

// Access mode
#define PIT_ACCESS_LATCH    0x00    // Latch count value
#define PIT_ACCESS_BYTE     0x10    // Read/write low byte only
#define PIT_ACCESS_WORD     0x30    // Read/write low byte then high byte

// Operating modes
#define PIT_MODE_0          0x00    // Interrupt on terminal count
#define PIT_MODE_1          0x02    // Hardware re-triggerable one-shot
#define PIT_MODE_2          0x04    // Rate generator
#define PIT_MODE_3          0x06    // Square wave generator (common for timer)
#define PIT_MODE_4          0x08    // Software triggered strobe
#define PIT_MODE_5          0x0A    // Hardware triggered strobe

// Binary/BCD mode
#define PIT_MODE_BINARY     0x00    // Binary counting (16-bit)
#define PIT_MODE_BCD        0x01    // BCD counting (4-digit decimal)

/* ============================================================================
 * PIT Frequency Constants
 * ============================================================================ */

#define PIT_BASE_FREQUENCY  1193180 // PIT input clock frequency (~1.19318 MHz)

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

#define TIMER_DEFAULT_FREQUENCY 1000 // Default timer frequency (1kHz = 1ms tick)
