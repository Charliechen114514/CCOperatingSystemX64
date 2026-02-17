/**
 * @file rtc_demo.c
 * @brief RTC Driver Demo - Demonstrates RTC functionality
 */

#include "driver/rtc/rtc.h"
#include "driver/rtc/rtc_constants.h"
#include "klogs/kprintf.h"
#include "klogs/ksnprintf.h"

/* ============================================================================
 * Demo State
 * ============================================================================ */

static volatile uint64_t demo_tick_count = 0;
static volatile bool alarm_triggered = false;

/* ============================================================================
 * Callback Functions
 * ============================================================================ */

/**
 * @brief Callback for RTC periodic interrupts
 */
static void demo_periodic_callback(void* context) {
    (void)context;
    demo_tick_count++;

    // Print a message every 8 ticks (8 seconds at 1Hz rate)
    if ((demo_tick_count % 8) == 0) {
        rtc_time_t current_time;
        if (rtc_get_time(&current_time) == 0) {
            char time_str[32];
            rtc_format_time(&current_time, time_str, sizeof(time_str));
            klog_info("[RTC Demo] Tick %lu - Current time: %s\n",
                     demo_tick_count, time_str);
        }
    }
}

/**
 * @brief Callback for RTC alarm
 */
static void demo_alarm_callback(void* context) {
    (void)context;
    alarm_triggered = true;

    rtc_time_t current_time;
    if (rtc_get_time(&current_time) == 0) {
        char time_str[32];
        rtc_format_time(&current_time, time_str, sizeof(time_str));
        klog_info("[RTC Demo] ALARM TRIGGERED at %s!\n", time_str);
    }
}

/* ============================================================================
 * Demo Functions
 * ============================================================================ */

/**
 * @brief Demo 1: Read and display current time
 */
static void demo_read_time(void) {
    klog_trace("\n=== Demo 1: Reading Current Time ===\n");

    rtc_time_t time;
    if (rtc_get_time(&time) != 0) {
        klog_error("[RTC Demo] Failed to read time!\n");
        return;
    }

    // Display individual components
    klog_trace("Year: %u\n", time.year);
    klog_trace("Month: %u\n", time.month);
    klog_trace("Day: %u\n", time.day_of_month);
    klog_trace("Hour: %u\n", time.hours);
    klog_trace("Minute: %u\n", time.minutes);
    klog_trace("Second: %u\n", time.seconds);
    klog_trace("Day of Week: %u\n", time.day_of_week);

    // Use formatted string
    char time_str[32];
    rtc_format_time(&time, time_str, sizeof(time_str));
    klog_info("Current time: %s\n", time_str);
}

/**
 * @brief Demo 2: Display RTC status
 */
static void demo_status(void) {
    klog_trace("\n=== Demo 2: RTC Status ===\n");

    // Check if RTC is valid (battery OK)
    if (rtc_is_valid()) {
        klog_trace("Battery: OK\n");
    } else {
        klog_warn("Battery: LOW or DEAD!\n");
    }

    // Check if RTC is updating
    if (rtc_is_updating()) {
        klog_trace("Status: Updating...\n");
    } else {
        klog_trace("Status: Idle\n");
    }

    // Show interrupt count
    uint64_t irq_count = rtc_get_interrupt_count();
    klog_trace("Interrupt count: %lu\n", irq_count);
}

/**
 * @brief Demo 3: Enable periodic interrupts
 */
static void demo_periodic_interrupt(void) {
    klog_trace("\n=== Demo 3: Enabling Periodic Interrupt (1 Hz) ===\n");
    klog_trace("The RTC will now generate a tick every second.\n");
    klog_trace("A time update will be shown every 8 seconds.\n");

    // Enable 1 Hz periodic interrupt
    if (rtc_enable_periodic(RTC_RATE_1Hz, demo_periodic_callback, NULL) != 0) {
        klog_error("[RTC Demo] Failed to enable periodic interrupt!\n");
        return;
    }

    klog_info("Periodic interrupt enabled at 1 Hz\n");
}

/**
 * @brief Demo 4: Set an alarm
 */
static void demo_set_alarm(void) {
    klog_trace("\n=== Demo 4: Setting Alarm ===\n");

    rtc_time_t current_time;
    if (rtc_get_time(&current_time) != 0) {
        klog_error("[RTC Demo] Failed to read time for alarm!\n");
        return;
    }

    // Calculate alarm time: 10 seconds from now
    uint8_t alarm_seconds = (current_time.seconds + 10) % 60;
    uint8_t alarm_minutes = current_time.minutes;
    uint8_t alarm_hours = current_time.hours;

    // Handle rollover
    if (alarm_seconds < current_time.seconds) {
        alarm_minutes++;
        if (alarm_minutes > 59) {
            alarm_minutes = 0;
            alarm_hours = (alarm_hours + 1) % 24;
        }
    }

    char time_str[32];
    rtc_format_time(&current_time, time_str, sizeof(time_str));
    klog_trace("Current time: %s\n", time_str);

    // Format alarm time string manually
    char alarm_str[32];
    ksnprintf(alarm_str, sizeof(alarm_str), "%02u:%02u:%02u",
              alarm_hours, alarm_minutes, alarm_seconds);
    klog_trace("Setting alarm for: %s (in ~10 seconds)\n", alarm_str);

    // Set the alarm (hours, minutes, seconds)
    if (rtc_set_alarm(alarm_hours, alarm_minutes, alarm_seconds,
                      demo_alarm_callback, NULL) != 0) {
        klog_error("[RTC Demo] Failed to set alarm!\n");
        return;
    }

    klog_info("Alarm set! Will trigger in ~10 seconds...\n");
}

/**
 * @brief Demo 5: Disable alarm
 */
static void demo_disable_alarm(void) {
    klog_trace("\n=== Demo 5: Disabling Alarm ===\n");
    rtc_disable_alarm();
    klog_info("Alarm disabled\n");
}

/**
 * @brief Demo 6: Disable periodic interrupt
 */
static void demo_disable_periodic(void) {
    klog_trace("\n=== Demo 6: Disabling Periodic Interrupt ===\n");
    rtc_disable_periodic();
    klog_info("Periodic interrupt disabled\n");
    klog_trace("Total ticks counted: %lu\n", demo_tick_count);
}

/* ============================================================================
 * Main Demo Entry Point
 * ============================================================================ */

/**
 * @brief Run all RTC demos
 *
 * This function demonstrates the various RTC capabilities:
 * 1. Reading current time
 * 2. Checking RTC status
 * 3. Enabling periodic interrupts
 * 4. Setting an alarm
 *
 * Call this function after RTC initialization to see the demos.
 */
void rtc_run_demo(void) {
    klog_trace("\n");
    klog_trace("========================================\n");
    klog_trace("   RTC Driver Demo\n");
    klog_trace("========================================\n");

    // Demo 1: Read time
    demo_read_time();

    // Demo 2: Show status
    demo_status();

    // Demo 3: Enable periodic interrupts
    demo_periodic_interrupt();

    // Demo 4: Set an alarm
    demo_set_alarm();

    klog_trace("\n=== Demo Running ===\n");
    klog_trace("The demo is now active. Periodic ticks and alarm will be shown.\n");
    klog_trace("Total RTC interrupts: %lu\n", rtc_get_interrupt_count());
}

/**
 * @brief Stop the RTC demo
 *
 * Disables periodic interrupts and alarm.
 */
void rtc_stop_demo(void) {
    demo_disable_alarm();
    demo_disable_periodic();
    klog_trace("\n=== RTC Demo Stopped ===\n");
}
