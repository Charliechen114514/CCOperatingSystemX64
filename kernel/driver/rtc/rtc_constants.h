/**
 * @file rtc_constants.h
 * @brief CMOS RTC (Real Time Clock) Constants
 *
 * The CMOS RTC is accessed via ports 0x70 and 0x71:
 * - Port 0x70: CMOS address register (also contains NMI disable bit)
 * - Port 0x71: CMOS data register
 *
 * The RTC uses IRQ 8 for periodic and alarm interrupts.
 */

#pragma once

/* ============================================================================
 * CMOS/RTC I/O Ports
 * ============================================================================ */

#define CMOS_ADDRESS_PORT  0x70    // CMOS address register (also disables NMI)
#define CMOS_DATA_PORT     0x71    // CMOS data register

/* ============================================================================
 * CMOS Register Indexes
 * ============================================================================ */

// Time and Date registers (RTC)
#define RTC_SECONDS        0x00    // Seconds (BCD, 0-59)
#define RTC_SECONDS_ALARM  0x01    // Seconds alarm
#define RTC_MINUTES        0x02    // Minutes (BCD, 0-59)
#define RTC_MINUTES_ALARM  0x03    // Minutes alarm
#define RTC_HOURS          0x04    // Hours (BCD, mode dependent)
#define RTC_HOURS_ALARM    0x05    // Hours alarm
#define RTC_DAY_OF_WEEK    0x06    // Day of week (1-7, 1=Sunday)
#define RTC_DAY_OF_MONTH   0x07    // Day of month (1-31)
#define RTC_MONTH          0x08    // Month (1-12)
#define RTC_YEAR           0x09    // Year (0-99, add century for full year)

// Status and Control registers
#define RTC_REG_A          0x0A    // Register A: Status A
#define RTC_REG_B          0x0B    // Register B: Status B
#define RTC_REG_C          0x0C    // Register C: Status C (read only)
#define RTC_REG_D          0x0D    // Register D: Status D (read only)

// Century extension (not present on all systems)
#define RTC_CENTURY        0x32    // Century register (common extension)

/* ============================================================================
 * Register A Bits (Status A)
 * ============================================================================ */

#define RTC_REG_A_UIP      0x80    // Update in Progress (read-only)

// Rate selector bits (divisor for periodic interrupt)
#define RTC_REG_A_RS_SHIFT 4       // Rate selector shift
#define RTC_REG_A_RS_MASK  0x0F    // Rate selector mask

// Periodic interrupt rates (RS bits values)
#define RTC_RATE_8192Hz    0x00    // 8192 Hz (122 us)
#define RTC_RATE_4096Hz    0x01    // 4096 Hz (244 us)
#define RTC_RATE_2048Hz    0x02    // 2048 Hz (488 us)
#define RTC_RATE_1024Hz    0x03    // 1024 Hz (976 us)
#define RTC_RATE_512Hz     0x04    // 512 Hz (1.95 ms)
#define RTC_RATE_256Hz     0x05    // 256 Hz (3.9 ms)
#define RTC_RATE_128Hz     0x06    // 128 Hz (7.8 ms)
#define RTC_RATE_64Hz      0x07    // 64 Hz (15.6 ms)
#define RTC_RATE_32Hz      0x08    // 32 Hz (31.3 ms)
#define RTC_RATE_16Hz      0x09    // 16 Hz (62.5 ms)
#define RTC_RATE_8Hz       0x0A    // 8 Hz (125 ms)
#define RTC_RATE_4Hz       0x0B    // 4 Hz (250 ms)
#define RTC_RATE_2Hz       0x0C    // 2 Hz (500 ms)
#define RTC_RATE_1Hz       0x0D    // 1 Hz (1 s) - most common
#define RTC_RATE_0_5Hz     0x0E    // 0.5 Hz (2 s)
#define RTC_RATE_0_25Hz    0x0F    // 0.25 Hz (4 s)

// Divider control
#define RTC_REG_A_DV_SHIFT 6       // Divider control shift
#define RTC_REG_A_DV_MASK  0xC0    // Divider control mask

/* ============================================================================
 * Register B Bits (Status B - Read/Write)
 * ============================================================================ */

#define RTC_REG_B_SET      0x80    // Set mode (1 = disable updates)
#define RTC_REG_B_PIE      0x40    // Periodic Interrupt Enable
#define RTC_REG_B_AIE      0x20    // Alarm Interrupt Enable
#define RTC_REG_B_UIE      0x10    // Update-ended Interrupt Enable
#define RTC_REG_B_SQWE     0x08    // Square Wave Enable
#define RTC_REG_B_DM       0x04    // Data Mode (0=BCD, 1=binary)
#define RTC_REG_B_24HR     0x02    // 24-hour mode (0=12hr, 1=24hr)
#define RTC_REG_B_DSE      0x01    // Daylight Saving Enable

/* ============================================================================
 * Register C Bits (Status C - Read Only)
 * ============================================================================ */

#define RTC_REG_C_IRQF     0x80    // Interrupt Request Flag
#define RTC_REG_C_PF       0x40    // Periodic Interrupt Flag
#define RTC_REG_C_AF       0x20    // Alarm Interrupt Flag
#define RTC_REG_C_UF       0x10    // Update-ended Interrupt Flag

/* ============================================================================
 * Register D Bits (Status D - Read Only)
 * ============================================================================ */

#define RTC_REG_D_VRT      0x80    // Valid RAM and Time (battery OK)

/* ============================================================================
 * NMI Control
 * ============================================================================ */

#define CMOS_NMI_DISABLE   0x80    // Disable NMI when writing to address port
#define CMOS_NMI_ENABLE    0x00    // Enable NMI

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

#define RTC_DEFAULT_RATE   RTC_RATE_1Hz    // Default: 1 Hz periodic interrupt
#define RTC_DEFAULT_CENTURY 20             // Default century (for 2000-2099)
