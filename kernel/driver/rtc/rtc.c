/**
 * @file rtc.c
 * @brief CMOS RTC (Real Time Clock) Driver Implementation
 */

#include "rtc.h"
#include "rtc_constants.h"
#include "driver/io/io.h"
#include "driver/pic/pic.h"
#include "interrupt/idt.h"
#include "klogs/kprintf.h"
#include "klogs/ksnprintf.h"

/* ============================================================================
 * RTC State
 * ============================================================================ */

static volatile uint64_t rtc_interrupt_count = 0;
static rtc_periodic_callback_fn rtc_periodic_callback = NULL;
static void* rtc_periodic_context = NULL;
static rtc_alarm_callback_fn rtc_alarm_callback = NULL;
static void* rtc_alarm_context = NULL;

static bool rtc_binary_mode = false;  // false = BCD mode (default), true = binary mode

/* ============================================================================
 * IRQ Descriptor for RTC
 * ============================================================================ */

static irq_descriptor_t rtc_irq_desc = {
    .name = "CMOS RTC",
    .handler = NULL,  // Set at runtime
    .context = NULL,
    .flags = IRQ_FLAG_NONE,
    .invocation_count = 0
};

/* ============================================================================
 * Low-Level CMOS Access Functions
 * ============================================================================ */

/**
 * @brief Read a byte from CMOS register
 *
 * @param reg CMOS register index
 * @return uint8_t Register value
 */
static inline uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDRESS_PORT, reg | CMOS_NMI_DISABLE);
    return inb(CMOS_DATA_PORT);
}

/**
 * @brief Write a byte to CMOS register
 *
 * @param reg CMOS register index
 * @param value Value to write
 */
static inline void cmos_write(uint8_t reg, uint8_t value) {
    outb(CMOS_ADDRESS_PORT, reg | CMOS_NMI_DISABLE);
    outb(CMOS_DATA_PORT, value);
}

/**
 * @brief Convert BCD to binary
 */
static inline uint8_t bcd_to_binary(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

/**
 * @brief Convert binary to BCD
 */
static inline uint8_t binary_to_bcd(uint8_t binary) {
    return ((binary / 10) << 4) | (binary % 10);
}

/**
 * @brief Read RTC register, handling BCD conversion
 */
static uint8_t rtc_read_register(uint8_t reg) {
    uint8_t value = cmos_read(reg);
    if (!rtc_binary_mode) {
        value = bcd_to_binary(value);
    }
    return value;
}

/**
 * @brief Write RTC register, handling BCD conversion
 */
static void rtc_write_register(uint8_t reg, uint8_t value) {
    if (!rtc_binary_mode) {
        value = binary_to_bcd(value);
    }
    cmos_write(reg, value);
}

/* ============================================================================
 * RTC Interrupt Handler
 * ============================================================================ */

/**
 * @brief RTC interrupt handler
 *
 * Handles both periodic interrupts and alarm interrupts from the RTC.
 * Reads Register C to clear the interrupt flags.
 */
void rtc_irq_handler(interrupt_frame_t* frame, void* context) {
    (void)frame;
    (void)context;

    rtc_interrupt_count++;

    // Read Register C to clear interrupt flags
    uint8_t reg_c = cmos_read(RTC_REG_C);

    // Check for periodic interrupt
    if (reg_c & RTC_REG_C_PF) {
        if (rtc_periodic_callback != NULL) {
            rtc_periodic_callback(rtc_periodic_context);
        }
    }

    // Check for alarm interrupt
    if (reg_c & RTC_REG_C_AF) {
        if (rtc_alarm_callback != NULL) {
            rtc_alarm_callback(rtc_alarm_context);
        }
    }

    // Send EOI to both PICs (IRQ 8 is on PIC2)
    pic_send_eoi(8);
}

/* ============================================================================
 * Public Functions
 * ============================================================================ */

int rtc_init(void) {
    klog_trace("[RTC] Initializing CMOS RTC driver\n");

    // Check if battery is valid
    if (!rtc_is_valid()) {
        klog_warn("[RTC] Warning: CMOS battery may be dead or RTC not valid\n");
    }

    // Determine data mode (BCD or binary)
    uint8_t reg_b = cmos_read(RTC_REG_B);
    rtc_binary_mode = (reg_b & RTC_REG_B_DM) != 0;

    // Ensure 24-hour mode
    if (!(reg_b & RTC_REG_B_24HR)) {
        klog_trace("[RTC] Enabling 24-hour mode\n");
        reg_b |= RTC_REG_B_24HR;
        cmos_write(RTC_REG_B, reg_b);
    }

    // Disable all interrupts initially
    reg_b &= ~(RTC_REG_B_PIE | RTC_REG_B_AIE | RTC_REG_B_UIE);
    cmos_write(RTC_REG_B, reg_b);

    // Set up IRQ handler
    rtc_irq_desc.handler = rtc_irq_handler;
    int result = irq_register_handler(8, &rtc_irq_desc);  // IRQ 8 for RTC
    if (result != 0) {
        klog_error("[RTC] Failed to register IRQ handler: %d\n", result);
        return result;
    }

    klog_trace("[RTC] Initialized (%s mode, 24-hour format)\n",
               rtc_binary_mode ? "binary" : "BCD");
    return 0;
}

int rtc_enable_periodic(uint8_t rate_div, rtc_periodic_callback_fn callback, void* context) {
    // Validate rate divisor
    if (rate_div > RTC_RATE_0_25Hz) {
        klog_error("[RTC] Invalid rate divisor: 0x%X\n", rate_div);
        return -1;
    }

    // Set callback
    rtc_periodic_callback = callback;
    rtc_periodic_context = context;

    // Disable IRQ 8 during configuration
    pic_disable_irq(8);

    // Read current Register A and B
    uint8_t reg_a = cmos_read(RTC_REG_A);
    uint8_t reg_b = cmos_read(RTC_REG_B);

    // Set rate divisor (clear RS bits first)
    reg_a = (reg_a & ~RTC_REG_A_RS_MASK) | rate_div;

    // Enable periodic interrupt in Register B
    reg_b |= RTC_REG_B_PIE;

    // Write back
    cmos_write(RTC_REG_A, reg_a);
    cmos_write(RTC_REG_B, reg_b);

    // Clear any pending interrupts
    cmos_read(RTC_REG_C);

    // Unmask IRQ 8
    pic_enable_irq(8);

    klog_trace("[RTC] Periodic interrupt enabled (rate=0x%X)\n", rate_div);
    return 0;
}

void rtc_disable_periodic(void) {
    pic_disable_irq(8);

    uint8_t reg_b = cmos_read(RTC_REG_B);
    reg_b &= ~RTC_REG_B_PIE;
    cmos_write(RTC_REG_B, reg_b);

    rtc_periodic_callback = NULL;
    rtc_periodic_context = NULL;

    // Unmask IRQ 8 only if alarm is enabled
    if (rtc_alarm_callback != NULL) {
        pic_enable_irq(8);
    }

    klog_trace("[RTC] Periodic interrupt disabled\n");
}

int rtc_set_alarm(uint8_t hours, uint8_t minutes, uint8_t seconds,
                  rtc_alarm_callback_fn callback, void* context) {
    // Set callback
    rtc_alarm_callback = callback;
    rtc_alarm_context = context;

    // Disable IRQ 8 during configuration
    pic_disable_irq(8);

    // Write alarm values (0xC0 = don't-care)
    rtc_write_register(RTC_HOURS_ALARM, hours);
    rtc_write_register(RTC_MINUTES_ALARM, minutes);
    rtc_write_register(RTC_SECONDS_ALARM, seconds);

    // Enable alarm interrupt in Register B
    uint8_t reg_b = cmos_read(RTC_REG_B);
    reg_b |= RTC_REG_B_AIE;
    cmos_write(RTC_REG_B, reg_b);

    // Clear any pending interrupts
    cmos_read(RTC_REG_C);

    // Unmask IRQ 8
    pic_enable_irq(8);

    klog_trace("[RTC] Alarm set to %02d:%02d:%02d\n", hours, minutes, seconds);
    return 0;
}

void rtc_disable_alarm(void) {
    pic_disable_irq(8);

    uint8_t reg_b = cmos_read(RTC_REG_B);
    reg_b &= ~RTC_REG_B_AIE;
    cmos_write(RTC_REG_B, reg_b);

    rtc_alarm_callback = NULL;
    rtc_alarm_context = NULL;

    // Unmask IRQ 8 only if periodic is enabled
    if (rtc_periodic_callback != NULL) {
        pic_enable_irq(8);
    }

    klog_trace("[RTC] Alarm disabled\n");
}

int rtc_get_time(rtc_time_t* time) {
    if (time == NULL) {
        return -1;
    }

    // Wait until RTC is not updating
    while (rtc_is_updating()) {
        __asm__ volatile("pause");
    }

    // Read time values (order matters to avoid inconsistencies)
    time->seconds = rtc_read_register(RTC_SECONDS);
    time->minutes = rtc_read_register(RTC_MINUTES);
    time->hours = rtc_read_register(RTC_HOURS);
    time->day_of_week = rtc_read_register(RTC_DAY_OF_WEEK);
    time->day_of_month = rtc_read_register(RTC_DAY_OF_MONTH);
    time->month = rtc_read_register(RTC_MONTH);
    time->year = rtc_read_register(RTC_YEAR);

    // Add century (assume 2000s for now, could read from century register)
    time->year += 2000;

    // Sanity checks
    if (time->year < 2000 || time->year > 2099 ||
        time->month < 1 || time->month > 12 ||
        time->day_of_month < 1 || time->day_of_month > 31 ||
        time->hours > 23 ||
        time->minutes > 59 ||
        time->seconds > 59) {
        klog_warn("[RTC] Warning: RTC values may be invalid\n");
    }

    return 0;
}

int rtc_set_time(const rtc_time_t* time) {
    if (time == NULL) {
        return -1;
    }

    // Validate time values
    if (time->year < 2000 || time->year > 2099 ||
        time->month < 1 || time->month > 12 ||
        time->day_of_month < 1 || time->day_of_month > 31 ||
        time->hours > 23 ||
        time->minutes > 59 ||
        time->seconds > 59) {
        klog_error("[RTC] Invalid time values\n");
        return -1;
    }

    // Disable updates during setting
    uint8_t reg_b = cmos_read(RTC_REG_B);
    cmos_write(RTC_REG_B, reg_b | RTC_REG_B_SET);

    // Write time values (exclude century)
    rtc_write_register(RTC_SECONDS, time->seconds);
    rtc_write_register(RTC_MINUTES, time->minutes);
    rtc_write_register(RTC_HOURS, time->hours);
    rtc_write_register(RTC_DAY_OF_WEEK, time->day_of_week);
    rtc_write_register(RTC_DAY_OF_MONTH, time->day_of_month);
    rtc_write_register(RTC_MONTH, time->month);
    rtc_write_register(RTC_YEAR, time->year - 2000);

    // Re-enable updates
    cmos_write(RTC_REG_B, reg_b);

    // Wait for update to complete
    while (rtc_is_updating()) {
        __asm__ volatile("pause");
    }

    klog_trace("[RTC] Time set to %04d-%02d-%02d %02d:%02d:%02d\n",
               time->year, time->month, time->day_of_month,
               time->hours, time->minutes, time->seconds);

    return 0;
}

bool rtc_is_updating(void) {
    return (cmos_read(RTC_REG_A) & RTC_REG_A_UIP) != 0;
}

bool rtc_is_valid(void) {
    return (cmos_read(RTC_REG_D) & RTC_REG_D_VRT) != 0;
}

uint64_t rtc_get_interrupt_count(void) {
    return rtc_interrupt_count;
}

int rtc_format_time(const rtc_time_t* time, char* buffer, size_t size) {
    if (time == NULL || buffer == NULL || size < 20) {
        return -1;
    }

    // Format: YYYY-MM-DD HH:MM:SS
    int written = ksnprintf(buffer, size, "%04u-%02u-%02u %02u:%02u:%02u",
                            time->year, time->month, time->day_of_month,
                            time->hours, time->minutes, time->seconds);

    return (written > 0) ? written : -1;
}
