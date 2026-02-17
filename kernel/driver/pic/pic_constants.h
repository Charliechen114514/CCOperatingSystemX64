#pragma once

/* ============================================================================
 * PIC I/O Ports
 * ============================================================================ */

// Master PIC ports
#define PIC1_CMD 0x20  // Master PIC command port
#define PIC1_DATA 0x21 // Master PIC data port

// Slave PIC ports
#define PIC2_CMD 0xA0  // Slave PIC command port
#define PIC2_DATA 0xA1 // Slave PIC data port

/* ============================================================================
 * PIC Commands
 * ============================================================================ */

#define PIC_EOI 0x20       // End of Interrupt command
#define PIC_INIT 0x11      // Initialize command
#define PIC_ICW4_8086 0x01 // 8086 mode