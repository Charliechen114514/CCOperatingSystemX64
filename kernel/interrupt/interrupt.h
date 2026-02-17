/**
 * @file interrupt.h
 * @brief Interrupt subsystem initialization and management
 */

#pragma once

#include "idt.h"

/**
 * @brief Initialize the interrupt subsystem
 *
 * This function initializes:
 * 1. The PIC (Programmable Interrupt Controller)
 * 2. The IDT (Interrupt Descriptor Table)
 *
 * Note: Interrupts are NOT enabled yet. Call interrupt_finalize()
 * after all interrupt handlers have been registered.
 */
void interrupt_init(void);

/**
 * @brief Finalize interrupt initialization and enable interrupts
 *
 * This function should be called AFTER all interrupt handlers
 * have been registered (e.g., after timer_init(), keyboard_init(), etc.)
 *
 * It will:
 * 1. Enable all IRQ lines
 * 2. Enable CPU interrupts (sti)
 */
void interrupt_finalize(void);
