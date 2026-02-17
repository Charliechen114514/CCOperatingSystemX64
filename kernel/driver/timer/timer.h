/**
 * @file timer.h
 * @brief x86_64 PIT (8253/8254) Timer Driver
 */

#pragma once

#include "defines/types.h"
#include "interrupt/idt.h"

/**
 * @brief Default timer frequency in Hz (1ms interval)
 */
#define TIMER_DEFAULT_FREQUENCY 1000

/**
 * @brief Timer callback function type
 *
 * Called on each timer interrupt. Used by scheduler and other
 * subsystems that need periodic notifications.
 *
 * @param ticks Current timer tick count
 */
typedef void (*timer_callback_fn)(uint64_t ticks);

/**
 * @brief Initialize the timer subsystem
 *
 * Configures the PIT at the specified frequency and registers
 * the interrupt handler for IRQ 0.
 *
 * @param frequency Timer frequency in Hz (0 for default)
 * @return int 0 on success, negative on error
 */
int timer_init(uint32_t frequency);

/**
 * @brief Get the current timer tick count
 *
 * @return uint64_t Number of timer interrupts since boot
 */
uint64_t timer_get_ticks(void);

/**
 * @brief Busy-wait for specified milliseconds
 *
 * @param milliseconds Number of milliseconds to wait
 */
void timer_mdelay(uint32_t milliseconds);

/**
 * @brief Set a callback function to be called on each timer interrupt
 *
 * @param callback Function to call, or NULL to disable
 */
void timer_set_callback(timer_callback_fn callback);

/**
 * @brief Set the timer frequency at runtime
 *
 * Allows dynamic reconfiguration of the PIT frequency.
 * Note: This changes the tick rate, so timer_mdelay() calculations
 * will use the new frequency.
 *
 * @param frequency New timer frequency in Hz (18-1193180 Hz valid range)
 * @return int 0 on success, negative on error
 */
int timer_set_frequency(uint32_t frequency);

/**
 * @brief Timer interrupt handler (internal)
 *
 * @param frame Interrupt stack frame
 * @param context Context pointer (unused)
 */
void timer_irq_handler(interrupt_frame_t* frame, void* context);
