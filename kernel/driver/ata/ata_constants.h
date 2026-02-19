/**
 * @file ata_constants.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief ATA/IDE Hardware Constants and Definitions
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

/* ============================================================================
 * ATA I/O Port Definitions
 * ============================================================================ */

// Primary ATA Channel (IRQ 14)
#define ATA_PRIMARY_IO        0x1F0   // Base I/O port
#define ATA_PRIMARY_CTRL      0x3F6   // Control/Base

// Secondary ATA Channel (IRQ 15)
#define ATA_SECONDARY_IO      0x170   // Base I/O port
#define ATA_SECONDARY_CTRL    0x376   // Control/Base

/* ============================================================================
 * ATA Register Offsets (from I/O Base)
 * ============================================================================ */

#define ATA_REG_DATA          0x00    // Data register (16-bit)
#define ATA_REG_ERROR         0x01    // Error register (read)
#define ATA_REG_FEATURES      0x01    // Features register (write)
#define ATA_REG_SECCOUNT      0x02    // Sector count
#define ATA_REG_LBA_LO        0x03    // LBA low byte (bits 7:0)
#define ATA_REG_LBA_MID       0x04    // LBA mid byte (bits 15:8)
#define ATA_REG_LBA_HI        0x05    // LBA high byte (bits 23:16)
#define ATA_REG_DEVICE        0x06    // Device/Head register
#define ATA_REG_STATUS        0x07    // Status register (read)
#define ATA_REG_COMMAND       0x07    // Command register (write)

/* ============================================================================
 * ATA Control Register Offsets (from Control Base)
 * ============================================================================ */

#define ATA_CTRL_ALT_STATUS   0x00    // Alternate Status (read)
#define ATA_CTRL_DEVICE_CTL   0x00    // Device Control (write)

/* ============================================================================
 * Device Register Bits
 * ============================================================================ */

#define ATA_DEV_MASTER        0x00    // Select master drive
#define ATA_DEV_SLAVE         0x10    // Select slave drive
#define ATA_DEV_LBA           0x40    // Use LBA addressing mode

/* ============================================================================
 * Status Register Bits
 * ============================================================================ */

#define ATA_STATUS_BSY        0x80    // Busy - device is processing
#define ATA_STATUS_DRQ        0x08    // Data Request - ready to transfer data
#define ATA_STATUS_ERR        0x01    // Error - error occurred
#define ATA_STATUS_DF         0x20    // Device Fault
#define ATA_STATUS_DSC        0x10    // Device Seek Complete
#define ATA_STATUS_DRQ        0x08    // Data Request
#define ATA_STATUS_CORR       0x04    // Corrected Data
#define ATA_STATUS_IDX        0x02    // Index

/* ============================================================================
 * Error Register Bits
 * ============================================================================ */

#define ATA_ERR_AMNF          0x01    // Address Mark Not Found
#define ATA_ERR_TK0NF         0x02    // Track 0 Not Found
#define ATA_ERR_ABRT          0x04    // Aborted Command
#define ATA_ERR_MCR           0x08    // Media Change Request
#define ATA_ERR_IDNF          0x10    // ID Not Found
#define ATA_ERR_MC            0x20    // Media Changed
#define ATA_ERR_UNC           0x40    // Uncorrectable Data Error
#define ATA_ERR_BBK           0x80    // Bad Block Detected

/* ============================================================================
 * Device Control Register Bits
 * ============================================================================ */

#define ATA_CTL_SRST          0x04    // Software Reset
#define ATA_CTL_nIEN          0x02    // Disable Interrupts (1 = disable)

/* ============================================================================
 * ATA Commands
 * ============================================================================ */

#define ATA_CMD_IDENTIFY          0xEC    // Identify Device
#define ATA_CMD_READ_SECTORS      0x20    // Read Sectors (with retry)
#define ATA_CMD_READ_SECTORS_NR   0x21    // Read Sectors (no retry)
#define ATA_CMD_WRITE_SECTORS     0x30    // Write Sectors (with retry)
#define ATA_CMD_WRITE_SECTORS_NR  0x31    // Write Sectors (no retry)
#define ATA_CMD_FLUSH_CACHE       0xE7    // Flush Cache
#define ATA_CMD_IDENTIFY_PACKET   0xA1    // Identify Packet Device (ATAPI)

/* ============================================================================
 * LBA48 Extended Commands (for Phase 2)
 * ============================================================================ */

#define ATA_CMD_READ_SECTORS_EXT  0x24    // Read Sectors Extended
#define ATA_CMD_WRITE_SECTORS_EXT 0x34    // Write Sectors Extended
#define ATA_CMD_FLUSH_CACHE_EXT   0xEA    // Flush Cache Extended

/* ============================================================================
 * IDENTIFY Data Offsets (words)
 * ============================================================================ */

#define ATA_IDENT_CONFIG        0       // Configuration word
#define ATA_IDENT_CYLS          1       // Number of cylinders
#define ATA_IDENT_HEADS         3       // Number of heads
#define ATA_IDENT_SECS_PER_TRK  6       // Sectors per track
#define ATA_IDENT_SERIAL        10      // Serial number (words 10-19)
#define ATA_IDENT_FIRMWARE      23      // Firmware revision (words 23-26)
#define ATA_IDENT_MODEL         27      // Model number (words 27-46)
#define ATA_IDENT_CAPABILITIES  49      // Capabilities
#define ATA_IDENT_FIELD_VALID   53      // Fields valid
#define ATA_IDENT_LBA_SECTORS   60      // LBA sectors (words 60-61)
#define ATA_IDENT_DMA_WORD      63      // DMA support
#define ATA_IDENT_LBA48_SECTORS 100     // LBA48 sectors (words 100-103)

/* ============================================================================
 * IDENTIFY Capability Bits
 * ============================================================================ */

#define ATA_CAP_LBA             0x0200  // Bit 9: LBA support
#define ATA_CAP_DMA             0x0100  // Bit 8: DMA support
#define ATA_CAP_LBA48           0x0400  // Bit 10: LBA48 support
#define ATA_CAP_IORDY           0x0800  // Bit 11: IORDY support

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ATA_SECTOR_SIZE         512     // Standard sector size in bytes
#define ATA_MAX_SECTORS_PER_CMD 256     // Max sectors per command (LBA28)
#define ATA_MAX_SECTORS_PER_CMD_EXT 65536  // Max sectors per command (LBA48)
#define ATA_IDENTIFY_WORDS      256     // Number of words in IDENTIFY data

#define ATA_TIMEOUT_MS          5000    // Default timeout in milliseconds
