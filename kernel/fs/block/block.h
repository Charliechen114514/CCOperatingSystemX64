/**
 * @file block.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Generic Block Device Interface - Abstracts ATA and other block devices
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 *
 * This module provides a generic block device abstraction layer that wraps
 * the ATA driver and provides a unified interface for filesystems to access
 * block devices.
 */

#pragma once

#include "defines/types.h"
#include "list/list.h"

/* Forward declarations */
typedef struct block_device block_device_t;
typedef struct block_operations block_operations_t;

/* ============================================================================
 * Block Device Request Types
 * ============================================================================ */

/**
 * @brief Block device request types
 */
typedef enum block_req_type {
    BLOCK_REQ_READ,                /* Read request */
    BLOCK_REQ_WRITE,               /* Write request */
    BLOCK_REQ_FLUSH,               /* Flush cache */
} block_req_type_t;

/**
 * @brief Block device request flags
 */
#define BLOCK_REQ_SYNC      (1 << 0)   /* Synchronous request */
#define BLOCK_REQ_COMPLETED (1 << 1)   /* Request completed */
#define BLOCK_REQ_ERROR     (1 << 2)   /* Request had an error */

/**
 * @brief Block device request structure
 *
 * Represents a single I/O request to a block device.
 * Used for async I/O (currently simplified to sync).
 */
typedef struct block_request {
    list_head list;                /* Request queue list */

    block_req_type_t type;         /* Request type */
    uint64_t sector;               /* Starting LBA sector */
    uint32_t nsectors;             /* Number of sectors */
    void* buffer;                  /* Data buffer */
    uint32_t flags;                /* Request flags */

    int result;                    /* Result code (0 = success, < 0 = error) */

    block_device_t* dev;           /* Target device */
} block_request_t;

/**
 * @brief Block device geometry information
 */
typedef struct block_geometry {
    uint64_t nsectors;             /* Total sectors */
    uint32_t sector_size;          /* Sector size (typically 512) */
    uint32_t heads;                /* Number of heads (CHS) */
    uint32_t cylinders;            /* Number of cylinders */
    uint32_t sectors_per_track;    /* Sectors per track */
} block_geometry_t;

/**
 * @brief Block device operations
 *
 * Function pointers for device-specific operations.
 */
struct block_operations {
    /**
     * @brief Read blocks from device
     * @param dev Block device
     * @param block Starting block number
     * @param buffer Buffer to store data
     * @param nblocks Number of blocks to read
     * @return 0 on success, negative error code on failure
     */
    int (*read_blocks)(block_device_t* dev, uint64_t block,
                       void* buffer, uint32_t nblocks);

    /**
     * @brief Write blocks to device
     * @param dev Block device
     * @param block Starting block number
     * @param buffer Data to write
     * @param nblocks Number of blocks to write
     * @return 0 on success, negative error code on failure
     */
    int (*write_blocks)(block_device_t* dev, uint64_t block,
                        const void* buffer, uint32_t nblocks);

    /**
     * @brief Flush device cache
     * @param dev Block device
     * @return 0 on success, negative error code on failure
     */
    int (*flush)(block_device_t* dev);

    /**
     * @brief Get device geometry
     * @param dev Block device
     * @param geo Geometry structure to fill
     * @return 0 on success, negative error code on failure
     */
    int (*get_geometry)(block_device_t* dev, block_geometry_t* geo);
};

/**
 * @brief Block device structure
 *
 * Generic block device that wraps specific device implementations.
 */
struct block_device {
    list_head list;                /* List of all block devices */

    int dev_id;                    /* Device ID (0-3 for ATA) */
    char name[32];                 /* Device name (e.g., "sda", "hda") */

    uint64_t size;                 /* Device size in bytes */
    uint32_t block_size;           /* Block size (typically 512) */
    uint64_t nblocks;              /* Number of blocks */

    list_head request_queue;       /* Pending requests */
    bool processing;               /* Currently processing a request */

    uint32_t ref_count;            /* Reference count */

    const block_operations_t* ops; /* Device operations */
    void* private_data;            /* Device-specific private data */
};

/* ============================================================================
 * Block Device API
 * ============================================================================ */

/**
 * @brief Initialize block device subsystem
 *
 * Initializes the block device layer and registers any available ATA devices.
 *
 * @return 0 on success, negative error code on failure
 */
int block_init(void);

/**
 * @brief Register a block device
 *
 * @param dev Block device to register
 * @return 0 on success, negative error code on failure
 */
int block_device_register(block_device_t* dev);

/**
 * @brief Unregister a block device
 *
 * @param dev Block device to unregister
 */
void block_device_unregister(block_device_t* dev);

/**
 * @brief Get block device by ID
 *
 * Returns the block device with the given ID, incrementing its reference count.
 *
 * @param dev_id Device ID
 * @return Block device, or NULL if not found
 */
block_device_t* block_device_get(int dev_id);

/**
 * @brief Put block device reference
 *
 * Decrements the reference count of a block device.
 *
 * @param dev Block device
 */
void block_device_put(block_device_t* dev);

/**
 * @brief Synchronous read from block device
 *
 * Reads the specified number of blocks from the device.
 *
 * @param dev Block device
 * @param block Starting block number
 * @param buffer Buffer to store data
 * @param nblocks Number of blocks to read
 * @return Number of blocks read on success, negative error code on failure
 */
int block_read_sync(block_device_t* dev, uint64_t block,
                    void* buffer, uint32_t nblocks);

/**
 * @brief Synchronous write to block device
 *
 * Writes the specified number of blocks to the device.
 *
 * @param dev Block device
 * @param block Starting block number
 * @param buffer Data to write
 * @param nblocks Number of blocks to write
 * @return Number of blocks written on success, negative error code on failure
 */
int block_write_sync(block_device_t* dev, uint64_t block,
                     const void* buffer, uint32_t nblocks);

/**
 * @brief Get device size in bytes
 *
 * @param dev Block device
 * @return Device size in bytes
 */
static inline uint64_t block_device_size(block_device_t* dev) {
    return dev ? dev->size : 0;
}

/**
 * @brief Get device block size
 *
 * @param dev Block device
 * @return Block size in bytes
 */
static inline uint32_t block_device_block_size(block_device_t* dev) {
    return dev ? dev->block_size : 0;
}

/**
 * @brief Check if device exists
 *
 * @param dev_id Device ID
 * @return true if device exists, false otherwise
 */
bool block_device_exists(int dev_id);

/* ============================================================================
 * ATA Block Device Wrapper
 * ============================================================================ */

/**
 * @brief Create block device from ATA device
 *
 * Wraps an ATA device (0-3) as a generic block device.
 *
 * @param ata_device ATA device number (0-3)
 * @return Block device, or NULL on error
 */
block_device_t* ata_create_block_device(int ata_device);

/**
 * @brief Initialize ATA block devices
 *
 * Probes for ATA devices and creates block devices for them.
 *
 * @return Number of block devices created
 */
int ata_block_init(void);

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define BLOCK_OK              0
#define BLOCK_ERR_INVAL      (-1)  /* Invalid argument */
#define BLOCK_ERR_IO         (-2)  /* I/O error */
#define BLOCK_ERR_NODEV      (-3)  /* No such device */
#define BLOCK_ERR_NOMEM      (-4)  /* Out of memory */
#define BLOCK_ERR_BUSY       (-5)  /* Device busy */
