/**
 * @file ata.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief ATA/IDE Disk Driver Public API
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "defines/types.h"
#include "ata_internal.h"

/* ============================================================================
 * Initialization Mode Selection
 * ============================================================================ */

/**
 * @brief ATA initialization mode
 */
typedef enum {
    ATA_MODE_SYNC,   // Polling-based PIO (default)
    ATA_MODE_ASYNC   // Interrupt-driven async I/O
} ata_init_mode_t;

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief Initialize the ATA driver with mode selection
 *
 * Detects and initializes both Primary and Secondary ATA channels.
 * Probes for master and slave devices on each channel.
 *
 * @param mode Initialization mode (SYNC or ASYNC)
 * @return int 0 on success, negative on error
 */
int ata_init_ex(ata_init_mode_t mode);

/**
 * @brief Initialize the ATA driver (default sync mode)
 *
 * Detects and initializes both Primary and Secondary ATA channels.
 * Probes for master and slave devices on each channel.
 *
 * @return int 0 on success, negative on error
 */
int ata_init(void);

/**
 * @brief Initialize async I/O mode for ATA driver
 *
 * This is equivalent to calling ata_init_ex(ATA_MODE_ASYNC).
 * Can also be called after ata_init() to enable async mode.
 *
 * @return ata_result_t ATA_OK or error code
 */
ata_result_t ata_init_async_mode(void);

/**
 * @brief Read sectors from an ATA device
 *
 * Reads the specified number of sectors starting from the given LBA address.
 * Automatically uses LBA48 addressing for addresses above 2^28 sectors if supported.
 *
 * @param device Device to read from (ATA_DEVICE_MASTER or ATA_DEVICE_SLAVE)
 *              Or 0 for Primary Master, 1 for Primary Slave,
 *              2 for Secondary Master, 3 for Secondary Slave
 * @param lba Starting LBA address (64-bit for LBA48 support)
 * @param buffer Buffer to store data (must be at least sectors * 512 bytes)
 * @param sectors Number of sectors to read (1-65536 for LBA48)
 * @return ata_result_t ATA_OK on success, error code otherwise
 */
ata_result_t ata_read(int device, uint64_t lba, void* buffer, uint16_t sectors);

/**
 * @brief Write sectors to an ATA device
 *
 * Writes the specified number of sectors starting from the given LBA address.
 * Automatically uses LBA48 addressing for addresses above 2^28 sectors if supported.
 *
 * @param device Device to write to (ATA_DEVICE_MASTER or ATA_DEVICE_SLAVE)
 *              Or 0 for Primary Master, 1 for Primary Slave,
 *              2 for Secondary Master, 3 for Secondary Slave
 * @param lba Starting LBA address (64-bit for LBA48 support)
 * @param buffer Data to write (must be at least sectors * 512 bytes)
 * @param sectors Number of sectors to write (1-65536 for LBA48)
 * @return ata_result_t ATA_OK on success, error code otherwise
 */
ata_result_t ata_write(int device, uint64_t lba, const void* buffer, uint16_t sectors);

/**
 * @brief Get information about an ATA device
 *
 * Retrieves device information obtained from the IDENTIFY command.
 *
 * @param device Device to query (0-3)
 * @param info Pointer to store device information
 * @return ata_result_t ATA_OK on success, error code otherwise
 */
ata_result_t ata_get_info(int device, ata_device_info_t* info);

/**
 * @brief Check if a device is present
 *
 * @param device Device to check (0-3)
 * @return true if device is present, false otherwise
 */
bool ata_device_exists(int device);

/**
 * @brief Get the number of sectors for a device
 *
 * @param device Device to query (0-3)
 * @return uint64_t Number of sectors, or 0 if device not present
 */
uint64_t ata_get_sector_count(int device);

/**
 * @brief Get human-readable error string
 *
 * @param result ATA result code
 * @return const char* Error description
 */
const char* ata_error_string(ata_result_t result);

/* ============================================================================
 * Async I/O Public API
 * ============================================================================ */

/**
 * @brief Async read completion callback type
 *
 * @param device Device number (0-3)
 * @param lba Starting LBA address
 * @param sectors Number of sectors
 * @param buffer Data buffer
 * @param result ATA_OK on success, error code otherwise
 * @param context User-provided context pointer
 */
typedef void (*ata_read_callback_fn)(int device, uint64_t lba, uint16_t sectors,
                                     void* buffer, ata_result_t result, void* context);

/**
 * @brief Async write completion callback type
 *
 * @param device Device number (0-3)
 * @param lba Starting LBA address
 * @param sectors Number of sectors
 * @param buffer Data buffer
 * @param result ATA_OK on success, error code otherwise
 * @param context User-provided context pointer
 */
typedef void (*ata_write_callback_fn)(int device, uint64_t lba, uint16_t sectors,
                                      const void* buffer, ata_result_t result, void* context);

/**
 * @brief Async read sectors
 *
 * Initiates an async read operation. The callback will be invoked
 * when the operation completes.
 *
 * @param device Device to read from (0-3)
 * @param lba Starting LBA address
 * @param buffer Buffer to store data (must remain valid until callback)
 * @param sectors Number of sectors to read
 * @param callback Completion callback (can be NULL)
 * @param context User context pointer passed to callback
 * @return ata_result_t ATA_OK if queued, error code immediately
 */
ata_result_t ata_read_async(int device, uint64_t lba, void* buffer, uint16_t sectors,
                            ata_read_callback_fn callback, void* context);

/**
 * @brief Async write sectors
 *
 * Initiates an async write operation. The callback will be invoked
 * when the operation completes.
 *
 * @param device Device to write to (0-3)
 * @param lba Starting LBA address
 * @param buffer Data to write (must remain valid until callback)
 * @param sectors Number of sectors to write
 * @param callback Completion callback (can be NULL)
 * @param context User context pointer passed to callback
 * @return ata_result_t ATA_OK if queued, error code immediately
 */
ata_result_t ata_write_async(int device, uint64_t lba, const void* buffer, uint16_t sectors,
                             ata_write_callback_fn callback, void* context);

/**
 * @brief Poll for async operation completion
 *
 * For polling-based completion check instead of callbacks.
 * Returns true if all operations for a device are complete.
 *
 * @param device Device to check
 * @return true if no pending operations, false if operations pending
 */
bool ata_async_is_idle(int device);

/* ============================================================================
 * Convenience Macros
 * ============================================================================ */

// Device selection (0-3)
#define ATA_PRIMARY_MASTER     0
#define ATA_PRIMARY_SLAVE      1
#define ATA_SECONDARY_MASTER   2
#define ATA_SECONDARY_SLAVE    3

// Controller and device extraction
#define ATA_DEV_TO_CONTROLLER(dev)  ((dev) < 2 ? 0 : 1)
#define ATA_DEV_TO_DRIVE(dev)       ((dev) % 2)
