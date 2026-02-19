/**
 * @file ata.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief ATA/IDE Disk Driver Implementation
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "ata.h"
#include "ata_constants.h"
#include "ata_internal.h"
#include "base/memory.h"
#include "base/string.h"
#include "driver/io/io.h"
#include "driver/pic/pic.h"
#include "interrupt/idt.h"
#include "klogs/kprintf.h"
#include "mm/heap/heap.h"

/* ============================================================================
 * Global State
 * ============================================================================ */

static ata_controller_t g_controllers[2] = {0};
static ata_device_info_t g_devices[4] = {0};
static bool g_driver_initialized = false;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get error string for result code
 */
const char* ata_error_string(ata_result_t result) {
    switch (result) {
        case ATA_OK:
            return "Success";
        case ATA_ERR_NOT_INIT:
            return "Not initialized";
        case ATA_ERR_TIMEOUT:
            return "Timeout";
        case ATA_ERR_NO_DEVICE:
            return "No device";
        case ATA_ERR_IO_ERROR:
            return "I/O error";
        case ATA_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case ATA_ERR_UNSUPPORTED:
            return "Unsupported";
        default:
            return "Unknown error";
    }
}

/**
 * @brief Convert big-endian words to C string
 *
 * IDENTIFY data stores strings in big-endian word format.
 * Each word contains 2 bytes in reverse order.
 */
void ata_identify_string_to_c(const uint16_t* src, char* dest, int words) {
    int i;
    for (i = 0; i < words; i++) {
        uint16_t word = src[i];
        dest[i * 2] = (char)(word >> 8);
        dest[i * 2 + 1] = (char)(word & 0xFF);
    }
    // Null-terminate
    dest[words * 2] = '\0';

    // Trim trailing spaces
    int len = words * 2;
    while (len > 0 && dest[len - 1] == ' ') {
        dest[--len] = '\0';
    }
}

/**
 * @brief Delay for a short period (400ns minimum for ATA)
 */
static inline void ata_delay(void) {
    // ATA spec requires 400ns delay - we do a few port reads
    for (int i = 0; i < 4; i++) {
        inb(0x80); // Read from unused port (delay)
    }
}

/* ============================================================================
 * Low-Level I/O Functions
 * ============================================================================ */

/**
 * @brief Select device on controller
 */
ata_result_t ata_select_device(ata_controller_t* ctrl, ata_device_t device) {
    if (ctrl == NULL) {
        return ATA_ERR_INVALID_PARAM;
    }

    uint8_t dev_byte = ATA_DEV_LBA; // Use LBA addressing
    if (device == ATA_DEVICE_MASTER) {
        dev_byte |= ATA_DEV_MASTER;
    } else {
        dev_byte |= ATA_DEV_SLAVE;
    }

    outb(ctrl->io_base + ATA_REG_DEVICE, dev_byte);
    ata_delay(); // Wait for device selection

    return ATA_OK;
}

/**
 * @brief Wait for BSY bit to clear
 */
ata_result_t ata_wait_bsy(uint16_t io_base) {
    // Simple timeout using counter
    // TODO: Use timer when available for accurate timeout
    uint32_t timeout = 100000; // Approx timeout

    while (timeout-- > 0) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if (!(status & ATA_STATUS_BSY)) {
            return ATA_OK;
        }
        __asm__ volatile("pause");
    }

    return ATA_ERR_TIMEOUT;
}

/**
 * @brief Wait for DRQ bit to set (and BSY to clear)
 */
ata_result_t ata_wait_drq(uint16_t io_base) {
    uint32_t timeout = 100000;

    while (timeout-- > 0) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if ((status & ATA_STATUS_DRQ) && !(status & ATA_STATUS_BSY)) {
            return ATA_OK;
        }
        if (status & ATA_STATUS_ERR) {
            return ATA_ERR_IO_ERROR;
        }
        __asm__ volatile("pause");
    }

    return ATA_ERR_TIMEOUT;
}

/**
 * @brief Software reset the controller
 */
void ata_soft_reset(ata_controller_t* ctrl) {
    if (ctrl == NULL)
        return;

    // Set SRST bit
    outb(ctrl->ctrl_base + ATA_CTRL_DEVICE_CTL, ATA_CTL_SRST);
    ata_delay();

    // Clear SRST bit
    outb(ctrl->ctrl_base + ATA_CTRL_DEVICE_CTL, 0);
    ata_delay();

    // Wait for BSY to clear
    ata_wait_bsy(ctrl->io_base);
}

/* ============================================================================
 * Device Detection and IDENTIFY
 * ============================================================================ */

/**
 * @brief Check if device is present
 */
bool ata_device_present(ata_controller_t* ctrl, ata_device_t device) {
    if (ctrl == NULL)
        return false;

    // Select device
    if (ata_select_device(ctrl, device) != ATA_OK) {
        return false;
    }

    // Send IDENTIFY command
    outb(ctrl->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    // Check status immediately
    uint8_t status = inb(ctrl->io_base + ATA_REG_STATUS);

    // If status = 0, no device present
    if (status == 0) {
        return false;
    }

    // If error bit set, might be ATAPI (not supported)
    if (status & ATA_STATUS_ERR) {
        return false;
    }

    // Wait for BSY to clear
    if (ata_wait_bsy(ctrl->io_base) != ATA_OK) {
        return false;
    }

    // Check for DRQ
    status = inb(ctrl->io_base + ATA_REG_STATUS);
    if (!(status & ATA_STATUS_DRQ)) {
        return false;
    }

    return true;
}

/**
 * @brief Send IDENTIFY command and read response
 */
ata_result_t ata_identify_device(ata_controller_t* ctrl, ata_device_t device,
                                 ata_device_info_t* info) {
    if (ctrl == NULL || info == NULL) {
        return ATA_ERR_INVALID_PARAM;
    }

    // Clear info structure
    memset(info, 0, sizeof(ata_device_info_t));

    // Select device
    if (ata_select_device(ctrl, device) != ATA_OK) {
        return ATA_ERR_IO_ERROR;
    }

    // Send IDENTIFY command
    outb(ctrl->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    // Wait for BSY to clear
    ata_result_t result = ata_wait_bsy(ctrl->io_base);
    if (result != ATA_OK) {
        return result;
    }

    // Check for errors
    uint8_t status = inb(ctrl->io_base + ATA_REG_STATUS);
    if (status & ATA_STATUS_ERR) {
        return ATA_ERR_NO_DEVICE;
    }

    // Wait for DRQ
    result = ata_wait_drq(ctrl->io_base);
    if (result != ATA_OK) {
        return result;
    }

    // Read 256 words of IDENTIFY data
    uint16_t identify_data[ATA_IDENTIFY_WORDS];
    for (int i = 0; i < ATA_IDENTIFY_WORDS; i++) {
        identify_data[i] = inw(ctrl->io_base + ATA_REG_DATA);
    }

    // Parse IDENTIFY data
    info->exists = true;

    // Check capabilities
    uint16_t caps = identify_data[ATA_IDENT_CAPABILITIES];
    info->lba_supported = (caps & ATA_CAP_LBA) != 0;
    info->dma_supported = (caps & ATA_CAP_DMA) != 0;
    info->lba48_supported = (caps & ATA_CAP_LBA48) != 0;

    // Extract strings
    ata_identify_string_to_c(&identify_data[ATA_IDENT_MODEL], info->model, 20);
    ata_identify_string_to_c(&identify_data[ATA_IDENT_SERIAL], info->serial, 10);
    ata_identify_string_to_c(&identify_data[ATA_IDENT_FIRMWARE], info->firmware, 4);

    // Extract LBA sector count (words 60-61)
    if (info->lba_supported) {
        info->lba_sectors = ((uint64_t)identify_data[61] << 16) | identify_data[60];
    } else {
        // CHS mode - calculate from geometry
        info->lba_sectors = (uint64_t)identify_data[ATA_IDENT_CYLS] *
                            identify_data[ATA_IDENT_HEADS] * identify_data[ATA_IDENT_SECS_PER_TRK];
    }

    // Extract LBA48 sector count (words 100-103)
    if (info->lba48_supported) {
        info->lba48_sectors = ((uint64_t)identify_data[103] << 48) |
                              ((uint64_t)identify_data[102] << 32) |
                              ((uint64_t)identify_data[101] << 16) | identify_data[100];
    }

    // Extract CHS geometry
    info->cylinders = identify_data[ATA_IDENT_CYLS];
    info->heads = identify_data[ATA_IDENT_HEADS];
    info->sectors_per_track = identify_data[ATA_IDENT_SECS_PER_TRK];

    return ATA_OK;
}

/* ============================================================================
 * PIO Read/Write Functions
 * ============================================================================ */

/**
 * @brief Read sectors using PIO mode
 */
ata_result_t ata_read_pio(ata_controller_t* ctrl, ata_device_t device, uint32_t lba, void* buffer,
                          uint16_t sectors) {
    if (ctrl == NULL || buffer == NULL) {
        return ATA_ERR_INVALID_PARAM;
    }

    if (sectors == 0 || sectors > 256) {
        return ATA_ERR_INVALID_PARAM;
    }

    // 256 sectors is encoded as 0
    uint8_t sec_count = (sectors == 256) ? 0 : sectors;

    // Select device
    if (ata_select_device(ctrl, device) != ATA_OK) {
        return ATA_ERR_IO_ERROR;
    }

    // Wait for device to be ready
    ata_result_t result = ata_wait_bsy(ctrl->io_base);
    if (result != ATA_OK) {
        return result;
    }

    // Setup LBA address
    uint8_t lba_lo = lba & 0xFF;
    uint8_t lba_mid = (lba >> 8) & 0xFF;
    uint8_t lba_hi = (lba >> 16) & 0xFF;
    uint8_t lba_highest = (lba >> 24) & 0x0F; // Only 4 bits in device register

    // Set device/head register with LBA bits
    uint8_t dev_byte = ATA_DEV_LBA;
    if (device == ATA_DEVICE_SLAVE) {
        dev_byte |= ATA_DEV_SLAVE;
    }
    dev_byte |= lba_highest;
    outb(ctrl->io_base + ATA_REG_DEVICE, dev_byte);

    // Set sector count and LBA
    outb(ctrl->io_base + ATA_REG_SECCOUNT, sec_count);
    outb(ctrl->io_base + ATA_REG_LBA_LO, lba_lo);
    outb(ctrl->io_base + ATA_REG_LBA_MID, lba_mid);
    outb(ctrl->io_base + ATA_REG_LBA_HI, lba_hi);

    // Send READ command
    outb(ctrl->io_base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    // Read data for each sector
    uint16_t* buf16 = (uint16_t*)buffer;

    for (uint16_t sec = 0; sec < sectors; sec++) {
        // Wait for BSY to clear and DRQ to set
        result = ata_wait_drq(ctrl->io_base);
        if (result != ATA_OK) {
            ctrl->error_count++;
            return result;
        }

        // Check for errors
        uint8_t status = inb(ctrl->io_base + ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) {
            uint8_t error = inb(ctrl->io_base + ATA_REG_ERROR);
            klog_error("ATA read error: status=0x%02X, error=0x%02X\n", status, error);
            ctrl->error_count++;
            return ATA_ERR_IO_ERROR;
        }

        // Read 256 words (512 bytes)
        for (int i = 0; i < 256; i++) {
            buf16[sec * 256 + i] = inw(ctrl->io_base + ATA_REG_DATA);
        }
    }

    ctrl->read_count++;
    return ATA_OK;
}

/**
 * @brief Write sectors using PIO mode
 */
ata_result_t ata_write_pio(ata_controller_t* ctrl, ata_device_t device, uint32_t lba,
                           const void* buffer, uint16_t sectors) {
    if (ctrl == NULL || buffer == NULL) {
        return ATA_ERR_INVALID_PARAM;
    }

    if (sectors == 0 || sectors > 256) {
        return ATA_ERR_INVALID_PARAM;
    }

    // 256 sectors is encoded as 0
    uint8_t sec_count = (sectors == 256) ? 0 : sectors;

    // Select device
    if (ata_select_device(ctrl, device) != ATA_OK) {
        return ATA_ERR_IO_ERROR;
    }

    // Wait for device to be ready
    ata_result_t result = ata_wait_bsy(ctrl->io_base);
    if (result != ATA_OK) {
        return result;
    }

    // Setup LBA address
    uint8_t lba_lo = lba & 0xFF;
    uint8_t lba_mid = (lba >> 8) & 0xFF;
    uint8_t lba_hi = (lba >> 16) & 0xFF;
    uint8_t lba_highest = (lba >> 24) & 0x0F;

    // Set device/head register with LBA bits
    uint8_t dev_byte = ATA_DEV_LBA;
    if (device == ATA_DEVICE_SLAVE) {
        dev_byte |= ATA_DEV_SLAVE;
    }
    dev_byte |= lba_highest;
    outb(ctrl->io_base + ATA_REG_DEVICE, dev_byte);

    // Set sector count and LBA
    outb(ctrl->io_base + ATA_REG_SECCOUNT, sec_count);
    outb(ctrl->io_base + ATA_REG_LBA_LO, lba_lo);
    outb(ctrl->io_base + ATA_REG_LBA_MID, lba_mid);
    outb(ctrl->io_base + ATA_REG_LBA_HI, lba_hi);

    // Write data for each sector
    const uint16_t* buf16 = (const uint16_t*)buffer;

    for (uint16_t sec = 0; sec < sectors; sec++) {
        // Send WRITE command for this sector
        outb(ctrl->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

        // Wait for DRQ
        result = ata_wait_drq(ctrl->io_base);
        if (result != ATA_OK) {
            ctrl->error_count++;
            return result;
        }

        // Check for errors
        uint8_t status = inb(ctrl->io_base + ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) {
            uint8_t error = inb(ctrl->io_base + ATA_REG_ERROR);
            klog_error("ATA write error: status=0x%02X, error=0x%02X\n", status, error);
            ctrl->error_count++;
            return ATA_ERR_IO_ERROR;
        }

        // Write 256 words (512 bytes)
        for (int i = 0; i < 256; i++) {
            outw(ctrl->io_base + ATA_REG_DATA, buf16[sec * 256 + i]);
        }

        // Wait for BSY to clear (write complete)
        ata_wait_bsy(ctrl->io_base);

        // Flush cache after last sector
        if (sec == sectors - 1) {
            outb(ctrl->io_base + ATA_REG_COMMAND, ATA_CMD_FLUSH_CACHE);
            ata_wait_bsy(ctrl->io_base);
        }
    }

    ctrl->write_count++;
    return ATA_OK;
}

/* ============================================================================
 * LBA48 PIO Read/Write Functions
 * ============================================================================ */

/**
 * @brief Flush device cache (LBA48 version)
 */
ata_result_t ata_flush_cache_ext(ata_controller_t* ctrl) {
    if (ctrl == NULL) {
        return ATA_ERR_INVALID_PARAM;
    }

    // Send FLUSH CACHE EXT command
    outb(ctrl->io_base + ATA_REG_COMMAND, ATA_CMD_FLUSH_CACHE_EXT);

    // Wait for BSY to clear
    ata_result_t result = ata_wait_bsy(ctrl->io_base);
    if (result != ATA_OK) {
        return result;
    }

    // Check for errors
    uint8_t status = inb(ctrl->io_base + ATA_REG_STATUS);
    if (status & ATA_STATUS_ERR) {
        return ATA_ERR_IO_ERROR;
    }

    return ATA_OK;
}

/**
 * @brief Read sectors using PIO mode with LBA48 addressing
 */
ata_result_t ata_read_pio_lba48(ata_controller_t* ctrl, ata_device_t device, uint64_t lba,
                                void* buffer, uint16_t sectors) {
    if (ctrl == NULL || buffer == NULL) {
        return ATA_ERR_INVALID_PARAM;
    }

    if (sectors == 0) {
        return ATA_ERR_INVALID_PARAM;
    }
    // Note: uint16_t max is 65535, so sectors is naturally <= 65535 < 65536

    // Select device
    if (ata_select_device(ctrl, device) != ATA_OK) {
        return ATA_ERR_IO_ERROR;
    }

    // Wait for device to be ready
    ata_result_t result = ata_wait_bsy(ctrl->io_base);
    if (result != ATA_OK) {
        return result;
    }

    // Setup LBA48 address - extract all 48 bits
    uint8_t lba0 = (lba >> 0) & 0xFF;
    uint8_t lba1 = (lba >> 8) & 0xFF;
    uint8_t lba2 = (lba >> 16) & 0xFF;
    uint8_t lba3 = (lba >> 24) & 0xFF;
    uint8_t lba4 = (lba >> 32) & 0xFF;
    uint8_t lba5 = (lba >> 40) & 0xFF;

    uint8_t sec_count_lo = sectors & 0xFF;
    uint8_t sec_count_hi = (sectors >> 8) & 0xFF;

    // Set device/head register (LBA mode, no LBA bits here for LBA48)
    uint8_t dev_byte = ATA_DEV_LBA;
    if (device == ATA_DEVICE_SLAVE) {
        dev_byte |= ATA_DEV_SLAVE;
    }
    outb(ctrl->io_base + ATA_REG_DEVICE, dev_byte);

    // Write LO bytes for sector count and LBA
    outb(ctrl->io_base + ATA_REG_SECCOUNT, sec_count_lo);
    outb(ctrl->io_base + ATA_REG_LBA_LO, lba0);
    outb(ctrl->io_base + ATA_REG_LBA_MID, lba1);
    outb(ctrl->io_base + ATA_REG_LBA_HI, lba2);

    // Write HI bytes (HOB - High Order Byte)
    outb(ctrl->io_base + ATA_REG_SECCOUNT, sec_count_hi);
    outb(ctrl->io_base + ATA_REG_LBA_LO, lba3);
    outb(ctrl->io_base + ATA_REG_LBA_MID, lba4);
    outb(ctrl->io_base + ATA_REG_LBA_HI, lba5);

    // Send READ SECTORS EXT command
    outb(ctrl->io_base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS_EXT);

    // Read data for each sector
    uint16_t* buf16 = (uint16_t*)buffer;

    for (uint16_t sec = 0; sec < sectors; sec++) {
        // Wait for BSY to clear and DRQ to set
        result = ata_wait_drq(ctrl->io_base);
        if (result != ATA_OK) {
            ctrl->error_count++;
            return result;
        }

        // Check for errors
        uint8_t status = inb(ctrl->io_base + ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) {
            uint8_t error = inb(ctrl->io_base + ATA_REG_ERROR);
            klog_error("ATA LBA48 read error: status=0x%02X, error=0x%02X\n", status, error);
            ctrl->error_count++;
            return ATA_ERR_IO_ERROR;
        }

        // Read 256 words (512 bytes)
        for (int i = 0; i < 256; i++) {
            buf16[sec * 256 + i] = inw(ctrl->io_base + ATA_REG_DATA);
        }
    }

    ctrl->read_count++;
    return ATA_OK;
}

/**
 * @brief Write sectors using PIO mode with LBA48 addressing
 */
ata_result_t ata_write_pio_lba48(ata_controller_t* ctrl, ata_device_t device, uint64_t lba,
                                 const void* buffer, uint16_t sectors) {
    if (ctrl == NULL || buffer == NULL) {
        return ATA_ERR_INVALID_PARAM;
    }

    if (sectors == 0) {
        return ATA_ERR_INVALID_PARAM;
    }
    // Note: uint16_t max is 65535, so sectors is naturally <= 65535 < 65536

    // Select device
    if (ata_select_device(ctrl, device) != ATA_OK) {
        return ATA_ERR_IO_ERROR;
    }

    // Wait for device to be ready
    ata_result_t result = ata_wait_bsy(ctrl->io_base);
    if (result != ATA_OK) {
        return result;
    }

    // Setup LBA48 address - extract all 48 bits
    uint8_t lba0 = (lba >> 0) & 0xFF;
    uint8_t lba1 = (lba >> 8) & 0xFF;
    uint8_t lba2 = (lba >> 16) & 0xFF;
    uint8_t lba3 = (lba >> 24) & 0xFF;
    uint8_t lba4 = (lba >> 32) & 0xFF;
    uint8_t lba5 = (lba >> 40) & 0xFF;

    uint8_t sec_count_lo = sectors & 0xFF;
    uint8_t sec_count_hi = (sectors >> 8) & 0xFF;

    // Set device/head register (LBA mode)
    uint8_t dev_byte = ATA_DEV_LBA;
    if (device == ATA_DEVICE_SLAVE) {
        dev_byte |= ATA_DEV_SLAVE;
    }
    outb(ctrl->io_base + ATA_REG_DEVICE, dev_byte);

    // Write LO bytes for sector count and LBA
    outb(ctrl->io_base + ATA_REG_SECCOUNT, sec_count_lo);
    outb(ctrl->io_base + ATA_REG_LBA_LO, lba0);
    outb(ctrl->io_base + ATA_REG_LBA_MID, lba1);
    outb(ctrl->io_base + ATA_REG_LBA_HI, lba2);

    // Write HI bytes (HOB)
    outb(ctrl->io_base + ATA_REG_SECCOUNT, sec_count_hi);
    outb(ctrl->io_base + ATA_REG_LBA_LO, lba3);
    outb(ctrl->io_base + ATA_REG_LBA_MID, lba4);
    outb(ctrl->io_base + ATA_REG_LBA_HI, lba5);

    // Write data for each sector
    const uint16_t* buf16 = (const uint16_t*)buffer;

    for (uint16_t sec = 0; sec < sectors; sec++) {
        // Send WRITE SECTORS EXT command
        outb(ctrl->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS_EXT);

        // Wait for DRQ
        result = ata_wait_drq(ctrl->io_base);
        if (result != ATA_OK) {
            ctrl->error_count++;
            return result;
        }

        // Check for errors
        uint8_t status = inb(ctrl->io_base + ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) {
            uint8_t error = inb(ctrl->io_base + ATA_REG_ERROR);
            klog_error("ATA LBA48 write error: status=0x%02X, error=0x%02X\n", status, error);
            ctrl->error_count++;
            return ATA_ERR_IO_ERROR;
        }

        // Write 256 words (512 bytes)
        for (int i = 0; i < 256; i++) {
            outw(ctrl->io_base + ATA_REG_DATA, buf16[sec * 256 + i]);
        }

        // Wait for BSY to clear (write complete)
        ata_wait_bsy(ctrl->io_base);

        // Flush cache after last sector
        if (sec == sectors - 1) {
            result = ata_flush_cache_ext(ctrl);
            if (result != ATA_OK) {
                klog_warn("ATA LBA48 flush cache failed: %s\n", ata_error_string(result));
            }
        }
    }

    ctrl->write_count++;
    return ATA_OK;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * @brief Initialize the ATA driver with mode selection
 */
int ata_init_ex(ata_init_mode_t mode) {
    if (g_driver_initialized) {
        klog_warn("ATA driver already initialized\n");
        return 0;
    }

    klog_info("Initializing ATA/IDE disk driver...\n");

    // Initialize Primary Controller
    g_controllers[0].io_base = ATA_PRIMARY_IO;
    g_controllers[0].ctrl_base = ATA_PRIMARY_CTRL;
    g_controllers[0].irq = 14;
    g_controllers[0].initialized = false;
    g_controllers[0].interrupts_enabled = false;
    g_controllers[0].use_async_mode = false;
    g_controllers[0].queue = NULL;

    // Initialize Secondary Controller
    g_controllers[1].io_base = ATA_SECONDARY_IO;
    g_controllers[1].ctrl_base = ATA_SECONDARY_CTRL;
    g_controllers[1].irq = 15;
    g_controllers[1].initialized = false;
    g_controllers[1].interrupts_enabled = false;
    g_controllers[1].use_async_mode = false;
    g_controllers[1].queue = NULL;

    // Detect devices on each controller
    for (int ctrl_idx = 0; ctrl_idx < 2; ctrl_idx++) {
        ata_controller_t* ctrl = &g_controllers[ctrl_idx];

        klog_info("Probing %s ATA channel...\n", ctrl_idx == 0 ? "Primary" : "Secondary");

        // Software reset
        ata_soft_reset(ctrl);

        // Check for master device
        if (ata_device_present(ctrl, ATA_DEVICE_MASTER)) {
            ctrl->master_present = true;
            klog_info("  Master drive detected\n");

            // Get device info
            ata_device_info_t* info = &g_devices[ctrl_idx * 2];
            ata_result_t result = ata_identify_device(ctrl, ATA_DEVICE_MASTER, info);
            if (result == ATA_OK) {
                klog_info("    Model: %s\n", info->model);
                klog_info("    Serial: %s\n", info->serial);
                klog_info("    LBA sectors: %llu (%,llu MB)\n", info->lba_sectors,
                          (info->lba_sectors * 512) / (1024 * 1024));
                klog_info("    LBA48: %s\n", info->lba48_supported ? "Yes" : "No");
                klog_info("    DMA: %s\n", info->dma_supported ? "Yes" : "No");
            }
        } else {
            klog_info("  Master drive not present\n");
        }

        // Check for slave device
        if (ata_device_present(ctrl, ATA_DEVICE_SLAVE)) {
            ctrl->slave_present = true;
            klog_info("  Slave drive detected\n");

            // Get device info
            ata_device_info_t* info = &g_devices[ctrl_idx * 2 + 1];
            ata_result_t result = ata_identify_device(ctrl, ATA_DEVICE_SLAVE, info);
            if (result == ATA_OK) {
                klog_info("    Model: %s\n", info->model);
                klog_info("    Serial: %s\n", info->serial);
                klog_info("    LBA sectors: %llu (%,llu MB)\n", info->lba_sectors,
                          (info->lba_sectors * 512) / (1024 * 1024));
                klog_info("    LBA48: %s\n", info->lba48_supported ? "Yes" : "No");
                klog_info("    DMA: %s\n", info->dma_supported ? "Yes" : "No");
            }
        } else {
            klog_info("  Slave drive not present\n");
        }

        ctrl->initialized = true;
    }

    g_driver_initialized = true;
    klog_info("ATA driver initialization complete\n");

    // Initialize async I/O if requested
    if (mode == ATA_MODE_ASYNC) {
        klog_info("Initializing async I/O mode...\n");
        for (int i = 0; i < 2; i++) {
            if (g_controllers[i].initialized) {
                ata_init_async_io(&g_controllers[i]);
            }
        }
    }

    return 0;
}

/**
 * @brief Initialize the ATA driver (default sync mode)
 */
int ata_init(void) {
    return ata_init_ex(ATA_MODE_ASYNC);
}

/**
 * @brief Read sectors from an ATA device
 */
ata_result_t ata_read(int device, uint64_t lba, void* buffer, uint16_t sectors) {
    if (!g_driver_initialized) {
        return ATA_ERR_NOT_INIT;
    }

    if (device < 0 || device > 3) {
        return ATA_ERR_INVALID_PARAM;
    }

    int ctrl_idx = ATA_DEV_TO_CONTROLLER(device);
    ata_device_t drive = ATA_DEV_TO_DRIVE(device);

    ata_controller_t* ctrl = &g_controllers[ctrl_idx];

    // Check if device exists
    if (drive == ATA_DEVICE_MASTER && !ctrl->master_present) {
        return ATA_ERR_NO_DEVICE;
    }
    if (drive == ATA_DEVICE_SLAVE && !ctrl->slave_present) {
        return ATA_ERR_NO_DEVICE;
    }

    // Check if LBA48 is required and supported
    ata_device_info_t* info = &g_devices[device];
    bool use_lba48 = (lba > 0x0FFFFFFF) && info->lba48_supported;

    if (lba > 0x0FFFFFFF && !info->lba48_supported) {
        return ATA_ERR_UNSUPPORTED; // Address too high for LBA28
    }

    if (use_lba48) {
        return ata_read_pio_lba48(ctrl, drive, lba, buffer, sectors);
    } else {
        return ata_read_pio(ctrl, drive, (uint32_t)lba, buffer, sectors);
    }
}

/**
 * @brief Write sectors to an ATA device
 */
ata_result_t ata_write(int device, uint64_t lba, const void* buffer, uint16_t sectors) {
    if (!g_driver_initialized) {
        return ATA_ERR_NOT_INIT;
    }

    if (device < 0 || device > 3) {
        return ATA_ERR_INVALID_PARAM;
    }

    int ctrl_idx = ATA_DEV_TO_CONTROLLER(device);
    ata_device_t drive = ATA_DEV_TO_DRIVE(device);

    ata_controller_t* ctrl = &g_controllers[ctrl_idx];

    // Check if device exists
    if (drive == ATA_DEVICE_MASTER && !ctrl->master_present) {
        return ATA_ERR_NO_DEVICE;
    }
    if (drive == ATA_DEVICE_SLAVE && !ctrl->slave_present) {
        return ATA_ERR_NO_DEVICE;
    }

    // Check if LBA48 is required and supported
    ata_device_info_t* info = &g_devices[device];
    bool use_lba48 = (lba > 0x0FFFFFFF) && info->lba48_supported;

    if (lba > 0x0FFFFFFF && !info->lba48_supported) {
        return ATA_ERR_UNSUPPORTED; // Address too high for LBA28
    }

    if (use_lba48) {
        return ata_write_pio_lba48(ctrl, drive, lba, buffer, sectors);
    } else {
        return ata_write_pio(ctrl, drive, (uint32_t)lba, buffer, sectors);
    }
}

/**
 * @brief Get information about an ATA device
 */
ata_result_t ata_get_info(int device, ata_device_info_t* info) {
    if (!g_driver_initialized) {
        return ATA_ERR_NOT_INIT;
    }

    if (device < 0 || device > 3 || info == NULL) {
        return ATA_ERR_INVALID_PARAM;
    }

    // Copy cached info
    *info = g_devices[device];

    if (!info->exists) {
        return ATA_ERR_NO_DEVICE;
    }

    return ATA_OK;
}

/**
 * @brief Check if a device is present
 */
bool ata_device_exists(int device) {
    if (!g_driver_initialized || device < 0 || device > 3) {
        return false;
    }

    return g_devices[device].exists;
}

/**
 * @brief Get the number of sectors for a device
 */
uint64_t ata_get_sector_count(int device) {
    if (!g_driver_initialized || device < 0 || device > 3) {
        return 0;
    }

    ata_device_info_t* info = &g_devices[device];
    if (!info->exists) {
        return 0;
    }

    // Use LBA48 count if available and larger
    if (info->lba48_supported && info->lba48_sectors > 0) {
        return info->lba48_sectors;
    }

    return info->lba_sectors;
}

/* ============================================================================
 * Async I/O Implementation
 * ============================================================================ */

/**
 * @brief Enable interrupts for a controller
 */
void ata_enable_interrupts(ata_controller_t* ctrl) {
    if (ctrl == NULL)
        return;

    // Clear nIEN bit to enable interrupts
    outb(ctrl->ctrl_base + ATA_CTRL_DEVICE_CTL, 0);
    ctrl->interrupts_enabled = true;
}

/**
 * @brief Disable interrupts for a controller
 */
void ata_disable_interrupts(ata_controller_t* ctrl) {
    if (ctrl == NULL)
        return;

    // Set nIEN bit to disable interrupts
    outb(ctrl->ctrl_base + ATA_CTRL_DEVICE_CTL, ATA_CTL_nIEN);
    ctrl->interrupts_enabled = false;
}

/**
 * @brief Complete an async operation
 */
void ata_complete_async_op(ata_async_operation_t* op, ata_result_t result) {
    if (op == NULL)
        return;

    op->status = (result == ATA_OK) ? ATA_ASYNC_COMPLETED : ATA_ASYNC_ERROR;

    // Call completion callback
    if (op->callback != NULL) {
        op->callback(op->device, op->lba, op->sectors, op->buffer, result, op->context);
    }

    // Free the operation
    kfree(op);
}

/**
 * @brief Start next async operation from queue
 */
void ata_start_next_async_op(ata_controller_t* ctrl) {
    if (ctrl == NULL || ctrl->queue == NULL)
        return;

    // Check if there's already an active operation
    if (ctrl->queue->active != NULL) {
        return;
    }

    // Get next operation from queue
    ata_async_operation_t* op = ctrl->queue->head;
    if (op == NULL) {
        return; // Queue empty
    }

    // Remove from queue
    ctrl->queue->head = op->next;
    if (ctrl->queue->head == NULL) {
        ctrl->queue->tail = NULL;
    }

    // Mark as active
    ctrl->queue->active = op;
    op->status = ATA_ASYNC_IN_PROGRESS;

    // Start the operation
    uint16_t io_base = ctrl->io_base;

    // Select device
    ata_select_device(ctrl, op->drive);
    ata_wait_bsy(io_base);

    if (op->type == ATA_ASYNC_OP_READ) {
        if (op->use_lba48) {
            // Setup LBA48 read
            uint64_t lba = op->lba;
            uint8_t lba0 = (lba >> 0) & 0xFF;
            uint8_t lba1 = (lba >> 8) & 0xFF;
            uint8_t lba2 = (lba >> 16) & 0xFF;
            uint8_t lba3 = (lba >> 24) & 0xFF;
            uint8_t lba4 = (lba >> 32) & 0xFF;
            uint8_t lba5 = (lba >> 40) & 0xFF;
            uint8_t sc_lo = op->sectors & 0xFF;
            uint8_t sc_hi = (op->sectors >> 8) & 0xFF;

            uint8_t dev_byte = ATA_DEV_LBA;
            if (op->drive == ATA_DEVICE_SLAVE)
                dev_byte |= ATA_DEV_SLAVE;
            outb(io_base + ATA_REG_DEVICE, dev_byte);

            outb(io_base + ATA_REG_SECCOUNT, sc_lo);
            outb(io_base + ATA_REG_LBA_LO, lba0);
            outb(io_base + ATA_REG_LBA_MID, lba1);
            outb(io_base + ATA_REG_LBA_HI, lba2);

            outb(io_base + ATA_REG_SECCOUNT, sc_hi);
            outb(io_base + ATA_REG_LBA_LO, lba3);
            outb(io_base + ATA_REG_LBA_MID, lba4);
            outb(io_base + ATA_REG_LBA_HI, lba5);

            outb(io_base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS_EXT);
        } else {
            // Setup LBA28 read
            uint32_t lba = (uint32_t)op->lba;
            uint8_t lba_lo = lba & 0xFF;
            uint8_t lba_mid = (lba >> 8) & 0xFF;
            uint8_t lba_hi = (lba >> 16) & 0xFF;
            uint8_t lba_highest = (lba >> 24) & 0x0F;

            uint8_t dev_byte = ATA_DEV_LBA;
            if (op->drive == ATA_DEVICE_SLAVE)
                dev_byte |= ATA_DEV_SLAVE;
            dev_byte |= lba_highest;
            outb(io_base + ATA_REG_DEVICE, dev_byte);

            uint8_t sec_count = (op->sectors == 256) ? 0 : op->sectors;
            outb(io_base + ATA_REG_SECCOUNT, sec_count);
            outb(io_base + ATA_REG_LBA_LO, lba_lo);
            outb(io_base + ATA_REG_LBA_MID, lba_mid);
            outb(io_base + ATA_REG_LBA_HI, lba_hi);

            outb(io_base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);
        }
        // Interrupt will arrive when data is ready
    } else if (op->type == ATA_ASYNC_OP_WRITE) {
        // For writes, we send command first, then data in interrupt handler
        if (op->use_lba48) {
            uint64_t lba = op->lba;
            uint8_t lba0 = (lba >> 0) & 0xFF;
            uint8_t lba1 = (lba >> 8) & 0xFF;
            uint8_t lba2 = (lba >> 16) & 0xFF;
            uint8_t lba3 = (lba >> 24) & 0xFF;
            uint8_t lba4 = (lba >> 32) & 0xFF;
            uint8_t lba5 = (lba >> 40) & 0xFF;
            uint8_t sc_lo = op->sectors & 0xFF;
            uint8_t sc_hi = (op->sectors >> 8) & 0xFF;

            uint8_t dev_byte = ATA_DEV_LBA;
            if (op->drive == ATA_DEVICE_SLAVE)
                dev_byte |= ATA_DEV_SLAVE;
            outb(io_base + ATA_REG_DEVICE, dev_byte);

            outb(io_base + ATA_REG_SECCOUNT, sc_lo);
            outb(io_base + ATA_REG_LBA_LO, lba0);
            outb(io_base + ATA_REG_LBA_MID, lba1);
            outb(io_base + ATA_REG_LBA_HI, lba2);

            outb(io_base + ATA_REG_SECCOUNT, sc_hi);
            outb(io_base + ATA_REG_LBA_LO, lba3);
            outb(io_base + ATA_REG_LBA_MID, lba4);
            outb(io_base + ATA_REG_LBA_HI, lba5);

            outb(io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS_EXT);
        } else {
            uint32_t lba = (uint32_t)op->lba;
            uint8_t lba_lo = lba & 0xFF;
            uint8_t lba_mid = (lba >> 8) & 0xFF;
            uint8_t lba_hi = (lba >> 16) & 0xFF;
            uint8_t lba_highest = (lba >> 24) & 0x0F;

            uint8_t dev_byte = ATA_DEV_LBA;
            if (op->drive == ATA_DEVICE_SLAVE)
                dev_byte |= ATA_DEV_SLAVE;
            dev_byte |= lba_highest;
            outb(io_base + ATA_REG_DEVICE, dev_byte);

            uint8_t sec_count = (op->sectors == 256) ? 0 : op->sectors;
            outb(io_base + ATA_REG_SECCOUNT, sec_count);
            outb(io_base + ATA_REG_LBA_LO, lba_lo);
            outb(io_base + ATA_REG_LBA_MID, lba_mid);
            outb(io_base + ATA_REG_LBA_HI, lba_hi);

            outb(io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);
        }
        // Wait for DRQ to send first sector data
        ata_wait_drq(io_base);

        // Write first sector
        uint16_t* buf16 = (uint16_t*)op->buffer;
        for (int i = 0; i < 256; i++) {
            outw(io_base + ATA_REG_DATA, buf16[i]);
        }
    }
}

/**
 * @brief IRQ handler for ATA controller
 */
void ata_irq_handler(interrupt_frame_t* frame, void* context) {
    (void)frame;

    ata_controller_t* ctrl = (ata_controller_t*)context;
    if (ctrl == NULL || ctrl->queue == NULL) {
        // Spurious interrupt, still send EOI
        pic_send_eoi(ctrl->irq);
        return;
    }

    uint16_t io_base = ctrl->io_base;
    uint8_t status = inb(io_base + ATA_REG_STATUS);

    // Check if this is a data transfer interrupt
    if (!(status & (ATA_STATUS_DRQ | ATA_STATUS_ERR))) {
        // Spurious interrupt
        ctrl->irq_desc.invocation_count++;
        pic_send_eoi(ctrl->irq);
        return;
    }

    // Check for errors
    if (status & ATA_STATUS_ERR) {
        uint8_t error = inb(io_base + ATA_REG_ERROR);
        klog_error("ATA IRQ error: status=0x%02X, error=0x%02X\n", status, error);

        // Fail active operation
        if (ctrl->queue->active != NULL) {
            ata_complete_async_op(ctrl->queue->active, ATA_ERR_IO_ERROR);
            ctrl->queue->active = NULL;
        }

        ctrl->irq_desc.invocation_count++;
        pic_send_eoi(ctrl->irq);

        // Try next operation
        ata_start_next_async_op(ctrl);
        return;
    }

    // Process active operation
    ata_async_operation_t* op = ctrl->queue->active;
    if (op == NULL) {
        // No active operation, spurious interrupt
        ctrl->irq_desc.invocation_count++;
        pic_send_eoi(ctrl->irq);
        return;
    }

    // Handle read operation
    if (op->type == ATA_ASYNC_OP_READ) {
        uint16_t* buf16 = (uint16_t*)op->buffer;

        // Read sector data
        for (int i = 0; i < 256; i++) {
            buf16[op->sectors_done * 256 + i] = inw(io_base + ATA_REG_DATA);
        }

        op->sectors_done++;

        // Check if operation complete
        if (op->sectors_done >= op->sectors) {
            ata_complete_async_op(op, ATA_OK);
            ctrl->queue->active = NULL;
        }
        // Otherwise, wait for next interrupt for next sector
    }
    // Handle write operation
    else if (op->type == ATA_ASYNC_OP_WRITE) {
        op->sectors_done++;

        if (op->sectors_done >= op->sectors) {
            // All sectors written, flush cache
            uint8_t cmd = op->use_lba48 ? ATA_CMD_FLUSH_CACHE_EXT : ATA_CMD_FLUSH_CACHE;
            outb(io_base + ATA_REG_COMMAND, cmd);
            ata_wait_bsy(io_base);

            ata_complete_async_op(op, ATA_OK);
            ctrl->queue->active = NULL;
        } else {
            // Write next sector
            uint16_t* buf16 = (uint16_t*)op->buffer;
            for (int i = 0; i < 256; i++) {
                outw(io_base + ATA_REG_DATA, buf16[op->sectors_done * 256 + i]);
            }
        }
    }

    ctrl->irq_desc.invocation_count++;
    pic_send_eoi(ctrl->irq);

    // Start next operation if current one is complete
    if (ctrl->queue->active == NULL) {
        ata_start_next_async_op(ctrl);
    }
}

/**
 * @brief Initialize interrupt-driven async I/O for a controller
 */
ata_result_t ata_init_async_io(ata_controller_t* ctrl) {
    if (ctrl == NULL) {
        return ATA_ERR_INVALID_PARAM;
    }

    // Allocate queue structure
    ctrl->queue = (ata_async_queue_t*)kmalloc(sizeof(ata_async_queue_t));
    if (ctrl->queue == NULL) {
        return ATA_ERR_IO_ERROR;
    }

    // Initialize queue
    ctrl->queue->head = NULL;
    ctrl->queue->tail = NULL;
    ctrl->queue->active = NULL;
    ctrl->queue->irq_pending = false;

    // Setup IRQ descriptor
    ctrl->irq_desc.name = (ctrl->irq == 14) ? "ATA Primary" : "ATA Secondary";
    ctrl->irq_desc.handler = ata_irq_handler;
    ctrl->irq_desc.context = ctrl;
    ctrl->irq_desc.flags = IRQ_FLAG_NONE;
    ctrl->irq_desc.invocation_count = 0;

    // Register IRQ handler
    int result = irq_register_handler(ctrl->irq, &ctrl->irq_desc);
    if (result != 0) {
        klog_error("Failed to register ATA IRQ %d: %d\n", ctrl->irq, result);
        kfree(ctrl->queue);
        ctrl->queue = NULL;
        return ATA_ERR_IO_ERROR;
    }

    // Enable interrupts on controller
    ata_enable_interrupts(ctrl);

    // Enable IRQ in PIC
    pic_enable_irq(ctrl->irq);

    ctrl->use_async_mode = true;

    klog_info("ATA async I/O initialized for IRQ %d\n", ctrl->irq);

    return ATA_OK;
}

/* ============================================================================
 * Async I/O Public API Implementation
 * ============================================================================ */

/**
 * @brief Initialize async mode (can be called after ata_init)
 */
ata_result_t ata_init_async_mode(void) {
    if (!g_driver_initialized) {
        return ATA_ERR_NOT_INIT;
    }

    for (int i = 0; i < 2; i++) {
        ata_controller_t* ctrl = &g_controllers[i];
        if (ctrl->initialized && !ctrl->use_async_mode) {
            ata_result_t result = ata_init_async_io(ctrl);
            if (result != ATA_OK) {
                return result;
            }
        }
    }

    return ATA_OK;
}

/**
 * @brief Async read sectors
 */
ata_result_t ata_read_async(int device, uint64_t lba, void* buffer, uint16_t sectors,
                            ata_read_callback_fn callback, void* context) {
    if (!g_driver_initialized) {
        return ATA_ERR_NOT_INIT;
    }

    if (device < 0 || device > 3 || buffer == NULL || sectors == 0) {
        return ATA_ERR_INVALID_PARAM;
    }

    int ctrl_idx = ATA_DEV_TO_CONTROLLER(device);
    ata_controller_t* ctrl = &g_controllers[ctrl_idx];

    if (!ctrl->use_async_mode) {
        return ATA_ERR_UNSUPPORTED;
    }

    // Allocate operation
    ata_async_operation_t* op = (ata_async_operation_t*)kmalloc(sizeof(ata_async_operation_t));
    if (op == NULL) {
        return ATA_ERR_IO_ERROR;
    }

    // Setup operation
    op->type = ATA_ASYNC_OP_READ;
    op->status = ATA_ASYNC_PENDING;
    op->device = device;
    op->drive = ATA_DEV_TO_DRIVE(device);
    op->lba = lba;
    op->sectors = sectors;
    op->sectors_done = 0;
    op->buffer = buffer;
    op->callback = (ata_async_callback_fn)callback;
    op->context = context;
    op->next = NULL;

    // Check if LBA48 needed
    ata_device_info_t* info = &g_devices[device];
    op->use_lba48 = (lba > 0x0FFFFFFF) && info->lba48_supported;

    // Add to queue
    if (ctrl->queue->tail == NULL) {
        ctrl->queue->head = op;
        ctrl->queue->tail = op;
        // Try to start immediately
        if (ctrl->queue->active == NULL) {
            ata_start_next_async_op(ctrl);
        }
    } else {
        ctrl->queue->tail->next = op;
        ctrl->queue->tail = op;
    }

    return ATA_OK;
}

/**
 * @brief Async write sectors
 */
ata_result_t ata_write_async(int device, uint64_t lba, const void* buffer, uint16_t sectors,
                             ata_write_callback_fn callback, void* context) {
    if (!g_driver_initialized) {
        return ATA_ERR_NOT_INIT;
    }

    if (device < 0 || device > 3 || buffer == NULL || sectors == 0) {
        return ATA_ERR_INVALID_PARAM;
    }

    int ctrl_idx = ATA_DEV_TO_CONTROLLER(device);
    ata_controller_t* ctrl = &g_controllers[ctrl_idx];

    if (!ctrl->use_async_mode) {
        return ATA_ERR_UNSUPPORTED;
    }

    // Allocate operation
    ata_async_operation_t* op = (ata_async_operation_t*)kmalloc(sizeof(ata_async_operation_t));
    if (op == NULL) {
        return ATA_ERR_IO_ERROR;
    }

    // Setup operation
    op->type = ATA_ASYNC_OP_WRITE;
    op->status = ATA_ASYNC_PENDING;
    op->device = device;
    op->drive = ATA_DEV_TO_DRIVE(device);
    op->lba = lba;
    op->sectors = sectors;
    op->sectors_done = 0;
    op->buffer = (void*)buffer; // Cast away const for internal use
    op->callback = (ata_async_callback_fn)callback;
    op->context = context;
    op->next = NULL;

    // Check if LBA48 needed
    ata_device_info_t* info = &g_devices[device];
    op->use_lba48 = (lba > 0x0FFFFFFF) && info->lba48_supported;

    // Add to queue
    if (ctrl->queue->tail == NULL) {
        ctrl->queue->head = op;
        ctrl->queue->tail = op;
        // Try to start immediately
        if (ctrl->queue->active == NULL) {
            ata_start_next_async_op(ctrl);
        }
    } else {
        ctrl->queue->tail->next = op;
        ctrl->queue->tail = op;
    }

    return ATA_OK;
}

/**
 * @brief Check if async operations are idle for a device
 */
bool ata_async_is_idle(int device) {
    if (!g_driver_initialized || device < 0 || device > 3) {
        return true;
    }

    int ctrl_idx = ATA_DEV_TO_CONTROLLER(device);
    ata_controller_t* ctrl = &g_controllers[ctrl_idx];

    if (ctrl->queue == NULL) {
        return true;
    }

    return (ctrl->queue->active == NULL && ctrl->queue->head == NULL);
}
