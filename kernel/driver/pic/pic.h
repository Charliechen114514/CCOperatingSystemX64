/**
 * @file pic.h
 * @brief 8259 Programmable Interrupt Controller (PIC) driver
 *
 * The x86 architecture uses two 8259A PIC chips cascaded together:
 * - Master PIC: handles IRQs 0-7
 * - Slave PIC: handles IRQs 8-15
 *
 * The PIC must be remapped because the first 32 IRQ vectors (0-31) are
 * reserved for CPU exceptions. We remap IRQs 0-15 to vectors 32-47.
 */

#pragma once

#include "defines/types.h"

/* ============================================================================
 * PIC I/O Ports
 * ============================================================================ */

// Master PIC ports
#define PIC1_CMD  0x20    // Master PIC command port
#define PIC1_DATA 0x21    // Master PIC data port

// Slave PIC ports
#define PIC2_CMD  0xA0    // Slave PIC command port
#define PIC2_DATA 0xA1    // Slave PIC data port

/* ============================================================================
 * PIC Commands
 * ============================================================================ */

#define PIC_EOI       0x20    // End of Interrupt command
#define PIC_INIT      0x11    // Initialize command
#define PIC_ICW4_8086 0x01    // 8086 mode

/* ============================================================================
 * PIC Initialization
 * ============================================================================ */

/**
 * @brief Initialize and remap the PIC
 *
 * Remaps IRQs 0-15 to IDT vectors 32-47 to avoid conflict with
 * CPU exceptions (vectors 0-31).
 *
 * @param offset1 Base vector for master PIC IRQs (typically 32)
 * @param offset2 Base vector for slave PIC IRQs (typically 40)
 */
void pic_init(uint8_t offset1, uint8_t offset2);

/**
 * @brief Send End of Interrupt (EOI) to the PIC
 *
 * This must be called at the end of an IRQ handler to acknowledge
 * the interrupt and allow further interrupts.
 *
 * @param irq The IRQ number (0-15)
 */
void pic_send_eoi(uint8_t irq);

/* ============================================================================
 * IRQ Masking
 * ============================================================================ */

/**
 * @brief Disable an IRQ line (mask it)
 *
 * @param irq The IRQ number (0-15) to disable
 */
void pic_disable_irq(uint8_t irq);

/**
 * @brief Enable an IRQ line (unmask it)
 *
 * @param irq The IRQ number (0-15) to enable
 */
void pic_enable_irq(uint8_t irq);

/**
 * @brief Get the current IRQ mask
 *
 * @param irq The IRQ number (0-15)
 * @return true if IRQ is masked (disabled), false if enabled
 */
bool pic_is_irq_masked(uint8_t irq);

/**
 * @brief Disable all IRQs (mask all interrupt lines)
 */
void pic_disable_all(void);

/**
 * @brief Enable all IRQs (unmask all interrupt lines)
 */
void pic_enable_all(void);
