/**
 * @file block.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Generic Block Device Interface Implementation
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 */

#include "block.h"
#include "driver/ata/ata.h"
#include "klogs/kprintf.h"
#include "mm/heap/heap.h"
#include "base/string.h"
#include "base/memory.h"

/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief ATA block device private data
 */
typedef struct ata_block_dev {
    int ata_device;               /* ATA device number (0-3) */
    bool initialized;             /* Device initialized flag */
} ata_block_dev_t;

/**
 * @brief Block device registry
 */
static struct {
    block_device_t* devices[4];   /* Maximum 4 ATA devices */
    uint32_t count;               /* Number of registered devices */
} block_registry = {0};

/* ============================================================================
 * ATA Block Device Operations
 * ============================================================================ */

/**
 * @brief Read blocks from ATA device
 */
static int ata_block_read(block_device_t* dev, uint64_t block,
                          void* buffer, uint32_t nblocks) {
    if (!dev || !dev->private_data || !buffer) {
        return BLOCK_ERR_INVAL;
    }

    ata_block_dev_t* ata_dev = (ata_block_dev_t*)dev->private_data;

    /* ATA uses sectors (512 bytes), convert if needed */
    ata_result_t result = ata_read(ata_dev->ata_device, block, buffer, nblocks);

    if (result == ATA_OK) {
        return (int)nblocks;
    }

    klog_error("block: ATA read failed on device %d: %s\n",
               ata_dev->ata_device, ata_error_string(result));
    return BLOCK_ERR_IO;
}

/**
 * @brief Write blocks to ATA device
 */
static int ata_block_write(block_device_t* dev, uint64_t block,
                           const void* buffer, uint32_t nblocks) {
    if (!dev || !dev->private_data || !buffer) {
        return BLOCK_ERR_INVAL;
    }

    ata_block_dev_t* ata_dev = (ata_block_dev_t*)dev->private_data;

    ata_result_t result = ata_write(ata_dev->ata_device, block, buffer, nblocks);

    if (result == ATA_OK) {
        return (int)nblocks;
    }

    klog_error("block: ATA write failed on device %d: %s\n",
               ata_dev->ata_device, ata_error_string(result));
    return BLOCK_ERR_IO;
}

/**
 * @brief Flush ATA device cache
 *
 * ATA doesn't have an explicit flush command in PIO mode,
 * so this is a no-op for now.
 */
static int ata_block_flush(block_device_t* dev) {
    (void)dev;
    /* ATA flush not implemented in PIO mode */
    return BLOCK_OK;
}

/**
 * @brief Get ATA device geometry
 */
static int ata_block_get_geometry(block_device_t* dev, block_geometry_t* geo) {
    if (!dev || !geo) {
        return BLOCK_ERR_INVAL;
    }

    ata_block_dev_t* ata_dev = (ata_block_dev_t*)dev->private_data;

    ata_device_info_t info;
    ata_result_t result = ata_get_info(ata_dev->ata_device, &info);

    if (result != ATA_OK) {
        return BLOCK_ERR_IO;
    }

    geo->nsectors = info.lba_sectors;
    geo->sector_size = 512;
    geo->heads = info.heads;
    geo->cylinders = info.cylinders;
    geo->sectors_per_track = info.sectors_per_track;

    return BLOCK_OK;
}

/* ATA block operations table */
static const block_operations_t ata_block_ops = {
    .read_blocks = ata_block_read,
    .write_blocks = ata_block_write,
    .flush = ata_block_flush,
    .get_geometry = ata_block_get_geometry,
};

/* ============================================================================
 * Block Device Registry
 * ============================================================================ */

/**
 * @brief Register a block device
 */
int block_device_register(block_device_t* dev) {
    if (!dev) {
        return BLOCK_ERR_INVAL;
    }

    if (dev->dev_id < 0 || dev->dev_id >= 4) {
        klog_error("block: Invalid device ID %d\n", dev->dev_id);
        return BLOCK_ERR_INVAL;
    }

    if (block_registry.devices[dev->dev_id] != NULL) {
        klog_warn("block: Device %d already registered\n", dev->dev_id);
        return BLOCK_ERR_BUSY;
    }

    /* Initialize request queue */
    INIT_LIST_HEAD(&dev->request_queue);
    dev->processing = false;
    dev->ref_count = 1;

    /* Register device */
    block_registry.devices[dev->dev_id] = dev;
    block_registry.count++;

    klog_info("block: Registered device %d as %s (size=%lu MB)\n",
              dev->dev_id, dev->name, dev->size / (1024 * 1024));

    return BLOCK_OK;
}

/**
 * @brief Unregister a block device
 */
void block_device_unregister(block_device_t* dev) {
    if (!dev) {
        return;
    }

    if (dev->dev_id >= 0 && dev->dev_id < 4) {
        block_registry.devices[dev->dev_id] = NULL;
        block_registry.count--;
    }

    /* Free private data if it exists */
    if (dev->private_data) {
        kfree(dev->private_data);
        dev->private_data = NULL;
    }
}

/**
 * @brief Get block device by ID
 */
block_device_t* block_device_get(int dev_id) {
    if (dev_id < 0 || dev_id >= 4) {
        return NULL;
    }

    block_device_t* dev = block_registry.devices[dev_id];
    if (dev) {
        dev->ref_count++;
    }

    return dev;
}

/**
 * @brief Put block device reference
 */
void block_device_put(block_device_t* dev) {
    if (dev && dev->ref_count > 0) {
        dev->ref_count--;
    }
}

/**
 * @brief Check if device exists
 */
bool block_device_exists(int dev_id) {
    if (dev_id < 0 || dev_id >= 4) {
        return false;
    }
    return block_registry.devices[dev_id] != NULL;
}

/* ============================================================================
 * Block Device I/O
 * ============================================================================ */

/**
 * @brief Synchronous read from block device
 */
int block_read_sync(block_device_t* dev, uint64_t block,
                    void* buffer, uint32_t nblocks) {
    if (!dev || !buffer) {
        return BLOCK_ERR_INVAL;
    }

    if (!dev->ops || !dev->ops->read_blocks) {
        return BLOCK_ERR_NODEV;
    }

    return dev->ops->read_blocks(dev, block, buffer, nblocks);
}

/**
 * @brief Synchronous write to block device
 */
int block_write_sync(block_device_t* dev, uint64_t block,
                     const void* buffer, uint32_t nblocks) {
    if (!dev || !buffer) {
        return BLOCK_ERR_INVAL;
    }

    if (!dev->ops || !dev->ops->write_blocks) {
        return BLOCK_ERR_NODEV;
    }

    return dev->ops->write_blocks(dev, block, buffer, nblocks);
}

/* ============================================================================
 * ATA Block Device Creation
 * ============================================================================ */

/**
 * @brief Create block device from ATA device
 */
block_device_t* ata_create_block_device(int ata_device) {
    if (ata_device < 0 || ata_device >= 4) {
        klog_error("block: Invalid ATA device %d\n", ata_device);
        return NULL;
    }

    if (!ata_device_exists(ata_device)) {
        klog_warn("block: ATA device %d does not exist\n", ata_device);
        return NULL;
    }

    /* Allocate block device structure */
    block_device_t* dev = (block_device_t*)kmalloc(sizeof(block_device_t));
    if (!dev) {
        klog_error("block: Failed to allocate block device\n");
        return NULL;
    }

    /* Allocate private data */
    ata_block_dev_t* ata_dev = (ata_block_dev_t*)kmalloc(sizeof(ata_block_dev_t));
    if (!ata_dev) {
        kfree(dev);
        klog_error("block: Failed to allocate ATA private data\n");
        return NULL;
    }

    /* Initialize private data */
    ata_dev->ata_device = ata_device;
    ata_dev->initialized = true;

    /* Get device info */
    ata_device_info_t info;
    ata_result_t result = ata_get_info(ata_device, &info);
    if (result != ATA_OK) {
        kfree(ata_dev);
        kfree(dev);
        klog_error("block: Failed to get ATA device info\n");
        return NULL;
    }

    /* Initialize block device */
    dev->dev_id = ata_device;
    dev->block_size = 512;
    dev->nblocks = info.lba_sectors;
    dev->size = info.lba_sectors * 512;
    dev->processing = false;
    dev->ref_count = 0;
    dev->ops = &ata_block_ops;
    dev->private_data = ata_dev;

    /* Set device name */
    switch (ata_device) {
        case ATA_PRIMARY_MASTER:
            strcpy(dev->name, "hda");
            break;
        case ATA_PRIMARY_SLAVE:
            strcpy(dev->name, "hdb");
            break;
        case ATA_SECONDARY_MASTER:
            strcpy(dev->name, "hdc");
            break;
        case ATA_SECONDARY_SLAVE:
            strcpy(dev->name, "hdd");
            break;
        default:
            strcpy(dev->name, "hd?");
            break;
    }

    INIT_LIST_HEAD(&dev->request_queue);

    return dev;
}

/**
 * @brief Initialize ATA block devices
 */
int ata_block_init(void) {
    int count = 0;

    /* Initialize ATA driver first */
    int result = ata_init();
    if (result != 0) {
        klog_error("block: Failed to initialize ATA driver\n");
        return 0;
    }

    /* Probe for ATA devices and create block devices */
    for (int i = 0; i < 4; i++) {
        if (ata_device_exists(i)) {
            block_device_t* dev = ata_create_block_device(i);
            if (dev) {
                if (block_device_register(dev) == BLOCK_OK) {
                    count++;
                } else {
                    kfree(dev->private_data);
                    kfree(dev);
                }
            }
        }
    }

    klog_info("block: Initialized %d ATA block device(s)\n", count);

    return count;
}

/* ============================================================================
 * Block Subsystem Initialization
 * ============================================================================ */

/**
 * @brief Initialize block device subsystem
 */
int block_init(void) {
    /* Initialize registry */
    for (int i = 0; i < 4; i++) {
        block_registry.devices[i] = NULL;
    }
    block_registry.count = 0;

    /* Initialize ATA block devices */
    int count = ata_block_init();

    if (count == 0) {
        klog_warn("block: No block devices found\n");
    }

    return count > 0 ? BLOCK_OK : BLOCK_ERR_NODEV;
}
