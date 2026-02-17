/**
 * @file rtc.h
 * @brief CMOS RTC (Real Time Clock) Driver
 *
 * Provides access to the CMOS RTC for:
 * - Reading current date and time
 * - Setting date and time
 * - Periodic interrupt generation
 * - Alarm functionality
 */

#pragma once

#include "defines/types.h"
#include "interrupt/idt.h"

/* ============================================================================
 * RTC Time Structure
 * ============================================================================ */

/**
 * @brief Structure representing date and time
 */
typedef struct {
    uint8_t seconds;      // Seconds (0-59)
    uint8_t minutes;      // Minutes (0-59)
    uint8_t hours;        // Hours (0-23 in 24-hour mode)
    uint8_t day_of_week;  // Day of week (1-7, 1=Sunday)
    uint8_t day_of_month; // Day of month (1-31)
    uint8_t month;        // Month (1-12)
    uint16_t year;        // Full year (e.g., 2024)
} rtc_time_t;

/* ============================================================================
 * RTC Callback Types
 * ============================================================================ */

/**
 * @brief RTC periodic interrupt callback function type
 *
 * Called on each RTC periodic interrupt based on the configured rate.
 *
 * @param context Context pointer provided during callback registration
 */
typedef void (*rtc_periodic_callback_fn)(void* context);

/**
 * @brief RTC alarm callback function type
 *
 * Called when the RTC alarm triggers.
 *
 * @param context Context pointer provided during callback registration
 */
typedef void (*rtc_alarm_callback_fn)(void* context);

/* ============================================================================
 * RTC Initialization and Configuration
 * ============================================================================ */

/**
 * @brief Initialize the RTC subsystem
 *
 * Initializes the RTC driver, configures the RTC for binary/BCD mode,
 * and optionally enables periodic interrupts. By default, periodic
 * interrupts are disabled - use rtc_enable_periodic() to enable them.
 *
 * @return int 0 on success, negative on error
 */
int rtc_init(void);

/**
 * @brief Enable RTC periodic interrupts
 *
 * Enables periodic interrupts at the specified rate. The RTC will
 * generate IRQ 8 at the configured frequency.
 *
 * @param rate_div Rate divisor (use RTC_RATE_* macros from rtc_constants.h)
 *                 Common values: RTC_RATE_1Hz (1 second), RTC_RATE_8Hz (125ms)
 * @param callback Callback function to call on each interrupt (NULL to disable)
 * @param context Context pointer to pass to callback
 * @return int 0 on success, negative on error
 */
int rtc_enable_periodic(uint8_t rate_div, rtc_periodic_callback_fn callback, void* context);

/**
 * @brief Disable RTC periodic interrupts
 */
void rtc_disable_periodic(void);

/**
 * @brief Set the RTC alarm
 *
 * Configures the RTC to generate an interrupt when the time matches
 * the specified alarm values. Setting a value to 0xC0 (binary 11000000)
 * disables that alarm field (don't-care).
 *
 * @param hours Hour alarm (0-23 in 24-hour mode, or 0xC0 for don't-care)
 * @param minutes Minute alarm (0-59, or 0xC0 for don't-care)
 * @param seconds Second alarm (0-59, or 0xC0 for don't-care)
 * @param callback Callback function when alarm triggers
 * @param context Context pointer to pass to callback
 * @return int 0 on success, negative on error
 */
int rtc_set_alarm(uint8_t hours, uint8_t minutes, uint8_t seconds,
                  rtc_alarm_callback_fn callback, void* context);

/**
 * @brief Disable RTC alarm
 */
void rtc_disable_alarm(void);

/* ============================================================================
 * Time Reading and Setting
 * ============================================================================ */

/**
 * @brief Read the current time from RTC
 *
 * Reads the current date and time from the CMOS RTC.
 * Automatically handles BCD conversion and century.
 *
 * @param time Pointer to rtc_time_t structure to fill
 * @return int 0 on success, negative on error
 */
int rtc_get_time(rtc_time_t* time);

/**
 * @brief Set the RTC time
 *
 * Sets the RTC to the specified date and time.
 *
 * @param time Pointer to rtc_time_t structure containing new time
 * @return int 0 on success, negative on error
 */
int rtc_set_time(const rtc_time_t* time);

/* ============================================================================
 * RTC Status and Diagnostics
 * ============================================================================ */

/**
 * @brief Check if RTC is updating
 *
 * The RTC cannot be read while an update is in progress.
 * This function can be used to wait for the update to complete.
 *
 * @return bool true if update is in progress, false otherwise
 */
bool rtc_is_updating(void);

/**
 * @brief Check if RTC battery is OK
 *
 * @return bool true if battery OK (valid time), false if battery dead
 */
bool rtc_is_valid(void);

/**
 * @brief Get RTC interrupt invocation count
 *
 * @return uint64_t Number of RTC interrupts handled
 */
uint64_t rtc_get_interrupt_count(void);

/* ============================================================================
 * RTC String Formatting
 * ============================================================================ */

/**
 * @brief Format time as a string
 *
 * Formats the time as "YYYY-MM-DD HH:MM:SS"
 *
 * @param time Time to format
 * @param buffer Buffer to store string (must be at least 20 bytes)
 * @param size Buffer size
 * @return int Number of characters written (excluding null terminator)
 */
int rtc_format_time(const rtc_time_t* time, char* buffer, size_t size);
