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
 * 3. Enables interrupts
 *
 * Must be called before any hardware interrupts can be serviced.
 */
void interrupt_init(void);

/**
 * @brief Get the current timer tick count
 * @return Number of timer interrupts since boot
 */
uint64_t timer_get_ticks(void);
