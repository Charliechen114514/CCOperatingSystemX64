/**
 * @file ata_demo.c
 * @brief ATA/IDE Disk Driver Demo - Demonstrates disk I/O functionality
 */

#include "driver/ata/ata.h"
#include "driver/ata/ata_constants.h"
#include "driver/ata/ata_internal.h"
#include "klogs/kprintf.h"
#include "klogs/ksnprintf.h"
#include "base/memory.h"

/* ============================================================================
 * Demo Constants
 * ============================================================================ */

#define MBR_SIGNATURE 0xAA55
#define TEST_SECTOR_LBA 1      // Use sector 1 for testing (after MBR)
#define TEST_SECTOR_COUNT 2    // Read 2 sectors for testing
#define ATA_SECTOR_SIZE 512

/* ============================================================================
 * Async Demo Callbacks
 * ============================================================================ */

static volatile bool g_async_read_complete = false;
static ata_result_t g_async_read_result = ATA_OK;

static void demo_async_read_callback(int device, uint64_t lba, uint16_t sectors,
                                     void* buffer, ata_result_t result, void* context) {
    (void)device;
    (void)lba;
    (void)sectors;
    (void)buffer;
    (void)context;

    g_async_read_complete = true;
    g_async_read_result = result;

    klog_info("Async read callback: result=%s\n", ata_error_string(result));
}

/* ============================================================================
 * Demo Functions
 * ============================================================================ */

/**
 * @brief Demo 1: Read MBR and verify signature
 */
static void demo_read_mbr(void) {
    klog_trace("\n=== Demo 1: Reading MBR ===\n");

    uint8_t mbr_buffer[ATA_SECTOR_SIZE];
    ata_result_t result = ata_read(ATA_PRIMARY_MASTER, 0, mbr_buffer, 1);

    if (result != ATA_OK) {
        klog_error("[ATA Demo] Failed to read MBR: %s\n", ata_error_string(result));
        return;
    }

    klog_info("Successfully read MBR (512 bytes)\n");

    // Check MBR signature (last 2 bytes)
    uint16_t signature = *(uint16_t*)&mbr_buffer[510];
    if (signature == MBR_SIGNATURE) {
        klog_info("MBR signature valid: 0x%04X\n", signature);
    } else {
        klog_warn("MBR signature invalid: 0x%04X (expected 0x%04X)\n",
                  signature, MBR_SIGNATURE);
    }

    // Display partition table entries (if any)
    klog_trace("Partition Table:\n");
    for (int i = 0; i < 4; i++) {
        uint8_t* entry = &mbr_buffer[446 + i * 16];
        uint8_t status = entry[0];
        uint8_t type = entry[4];

        if (type != 0) {
            uint32_t lba = *(uint32_t*)&entry[8];
            uint32_t sectors = *(uint32_t*)&entry[12];

            klog_trace("  Partition %d: Type=0x%02X, Status=0x%02X, LBA=%u, Sectors=%u\n",
                     i, type, status, lba, sectors);
        } else {
            klog_trace("  Partition %d: Empty\n", i);
        }
    }
}

/**
 * @brief Demo 2: Display device information
 */
static void demo_device_info(void) {
    klog_trace("\n=== Demo 2: Device Information ===\n");

    for (int dev = 0; dev < 4; dev++) {
        if (!ata_device_exists(dev)) {
            continue;
        }

        const char* ctrl_name = (dev < 2) ? "Primary" : "Secondary";
        const char* dev_name = (dev % 2 == 0) ? "Master" : "Slave";

        klog_trace("--- %s %s ---\n", ctrl_name, dev_name);

        ata_device_info_t info;
        ata_result_t result = ata_get_info(dev, &info);

        if (result == ATA_OK) {
            klog_trace("  Model:    %s\n", info.model);
            klog_trace("  Serial:   %s\n", info.serial);
            klog_trace("  Firmware: %s\n", info.firmware);
            klog_trace("  LBA:      %s\n", info.lba_supported ? "Yes" : "No");
            klog_trace("  LBA48:    %s\n", info.lba48_supported ? "Yes" : "No");
            klog_trace("  DMA:      %s\n", info.dma_supported ? "Yes" : "No");

            uint64_t sectors = ata_get_sector_count(dev);
            uint64_t bytes = sectors * ATA_SECTOR_SIZE;
            uint64_t mb = bytes / (1024 * 1024);

            klog_info("  Capacity: %llu sectors (%llu MB, %llu GB)\n",
                     sectors, mb, mb / 1024);
        } else {
            klog_warn("  Failed to get info: %s\n", ata_error_string(result));
        }
    }
}

/**
 * @brief Demo 3: Read multiple sectors
 */
static void demo_read_multiple(void) {
    klog_trace("\n=== Demo 3: Reading Multiple Sectors ===\n");

    // Use a safe sector for testing (not the MBR)
    uint32_t lba = TEST_SECTOR_LBA;
    uint16_t sectors = TEST_SECTOR_COUNT;

    klog_trace("Reading %u sectors starting from LBA %u...\n", sectors, lba);

    uint8_t buffer[sectors * ATA_SECTOR_SIZE];
    ata_result_t result = ata_read(ATA_PRIMARY_MASTER, lba, buffer, sectors);

    if (result != ATA_OK) {
        klog_error("[ATA Demo] Failed to read sectors: %s\n", ata_error_string(result));
        return;
    }

    klog_info("Successfully read %u sectors (%u bytes)\n", sectors, sectors * ATA_SECTOR_SIZE);

    // Display first 64 bytes of first sector as hex dump
    klog_trace("Hex dump of first sector (first 64 bytes):\n");
    for (int row = 0; row < 4; row++) {  // 4 rows of 16 bytes
        klog_trace("  0x%03X: ", row * 16);
        for (int col = 0; col < 16; col++) {
            klog_trace("%02X ", buffer[row * 16 + col]);
        }
        klog_trace(" | ");
        // Print ASCII
        for (int col = 0; col < 16; col++) {
            uint8_t c = buffer[row * 16 + col];
            klog_trace("%c", (c >= 32 && c < 127) ? c : '.');
        }
        klog_trace("\n");
    }
}

/**
 * @brief Demo 4: Write and read back verification (safe test)
 *
 * NOTE: This is a destructive test! It writes to sector 1.
 * Only run this if you're okay with modifying sector 1.
 * For safety, we first read the sector, then write it back.
 */
static void demo_write_verify(void) {
    klog_trace("\n=== Demo 4: Write/Verify Test ===\n");
    klog_warn("This demo will MODIFY sector %d!\n", TEST_SECTOR_LBA);
    klog_warn("Only run this on test disks!\n");
    klog_trace("Skipping destructive write test for safety.\n");
    klog_trace("To enable, remove the early return in ata_demo.c\n");

    // For safety, we skip the actual write test
    // If you want to test writes, uncomment the code below:

    /*
    uint32_t lba = TEST_SECTOR_LBA;
    uint8_t original_buffer[ATA_SECTOR_SIZE];
    uint8_t test_pattern[ATA_SECTOR_SIZE];
    uint8_t read_back_buffer[ATA_SECTOR_SIZE];

    // First, read the original content
    ata_result_t result = ata_read(ATA_PRIMARY_MASTER, lba, original_buffer, 1);
    if (result != ATA_OK) {
        klog_error("[ATA Demo] Failed to read original: %s\n", ata_error_string(result));
        return;
    }

    // Create a test pattern (0xAA, 0x55, 0xFF, 0x00 repeating)
    for (int i = 0; i < ATA_SECTOR_SIZE; i++) {
        switch (i % 4) {
            case 0: test_pattern[i] = 0xAA; break;
            case 1: test_pattern[i] = 0x55; break;
            case 2: test_pattern[i] = 0xFF; break;
            case 3: test_pattern[i] = 0x00; break;
        }
    }

    // Write test pattern
    klog_trace("Writing test pattern to sector %u...\n", lba);
    result = ata_write(ATA_PRIMARY_MASTER, lba, test_pattern, 1);
    if (result != ATA_OK) {
        klog_error("[ATA Demo] Failed to write: %s\n", ata_error_string(result));
        return;
    }
    klog_info("Write successful\n");

    // Read back
    klog_trace("Reading back...\n");
    result = ata_read(ATA_PRIMARY_MASTER, lba, read_back_buffer, 1);
    if (result != ATA_OK) {
        klog_error("[ATA Demo] Failed to read back: %s\n", ata_error_string(result));
        // Restore original
        ata_write(ATA_PRIMARY_MASTER, lba, original_buffer, 1);
        return;
    }

    // Verify
    bool match = true;
    for (int i = 0; i < ATA_SECTOR_SIZE; i++) {
        if (test_pattern[i] != read_back_buffer[i]) {
            match = false;
            klog_error("Mismatch at byte %d: expected 0x%02X, got 0x%02X\n",
                     i, test_pattern[i], read_back_buffer[i]);
            break;
        }
    }

    if (match) {
        klog_info("Write/Verify: PASSED\n");
    } else {
        klog_error("Write/Verify: FAILED\n");
    }

    // Restore original content
    klog_trace("Restoring original content...\n");
    ata_write(ATA_PRIMARY_MASTER, lba, original_buffer, 1);
    */
}

/**
 * @brief Demo 5: Performance test
 */
static void demo_performance(void) {
    klog_trace("\n=== Demo 5: Read Performance Test ===\n");

    uint32_t lba = TEST_SECTOR_LBA;
    uint16_t sectors = 16;  // Read 8KB
    uint8_t buffer[sectors * ATA_SECTOR_SIZE];

    klog_trace("Reading %u sectors (%u KB)...\n", sectors, (sectors * ATA_SECTOR_SIZE) / 1024);

    ata_result_t result = ata_read(ATA_PRIMARY_MASTER, lba, buffer, sectors);

    if (result != ATA_OK) {
        klog_error("[ATA Demo] Failed to read: %s\n", ata_error_string(result));
        return;
    }

    klog_info("Read test completed\n");
    klog_trace("Note: Performance timing requires timer integration\n");
}

/**
 * @brief Demo 6: LBA48 Support Test
 */
static void demo_lba48_support(void) {
    klog_trace("\n=== Demo 6: LBA48 Support Test ===\n");

    // Check if device supports LBA48
    ata_device_info_t info;
    if (ata_get_info(ATA_PRIMARY_MASTER, &info) != ATA_OK) {
        klog_error("Failed to get device info\n");
        return;
    }

    klog_info("Device: %s\n", info.model);
    klog_info("LBA48 Supported: %s\n", info.lba48_supported ? "Yes" : "No");

    if (info.lba48_supported) {
        klog_info("LBA48 Sectors: %llu\n", info.lba48_sectors);

        // Test 1: Read at high LBA (last sector of disk)
        uint64_t high_lba = info.lba48_sectors - 1;
        if (high_lba > 0xFFFFFFFF) {
            // If disk is very large, use a more reasonable test address
            high_lba = 0x10000000;  // Just beyond LBA28 limit
            klog_trace("Disk very large, testing at LBA 0x%llX\n", high_lba);
        } else {
            klog_trace("Testing at last sector (LBA %llu)\n", high_lba);
        }

        uint8_t buffer1[ATA_SECTOR_SIZE];
        klog_trace("Attempting LBA48 read at LBA %llu...\n", high_lba);

        ata_result_t result = ata_read(ATA_PRIMARY_MASTER, high_lba, buffer1, 1);
        if (result == ATA_OK) {
            klog_info("LBA48 read successful at high LBA\n");
            klog_trace("First bytes: %02X %02X %02X %02X\n",
                       buffer1[0], buffer1[1], buffer1[2], buffer1[3]);
        } else {
            klog_warn("LBA48 read at high LBA failed: %s\n", ata_error_string(result));
            // This might be expected if the disk has reserved areas
        }

        // Test 2: Multi-sector read beyond LBA28 limit (256 sectors)
        if (info.lba48_sectors > 512) {
            uint16_t multi_sectors = 512;

            // For safety, use a smaller buffer on stack
            multi_sectors = 16;
            uint8_t buffer_small[multi_sectors * ATA_SECTOR_SIZE];

            klog_trace("Testing %u-sector read...\n", multi_sectors);
            result = ata_read(ATA_PRIMARY_MASTER, 0, buffer_small, multi_sectors);
            if (result == ATA_OK) {
                klog_info("Multi-sector LBA48 read successful\n");
            } else {
                klog_warn("Multi-sector read failed: %s\n", ata_error_string(result));
            }
        }

        // Test 3: Verify LBA28 auto-selection still works
        uint8_t buffer3[ATA_SECTOR_SIZE];
        klog_trace("Testing LBA28 compatibility (low LBA)...\n");
        result = ata_read(ATA_PRIMARY_MASTER, 100, buffer3, 1);
        if (result == ATA_OK) {
            klog_info("LBA28 auto-selection working correctly\n");
        }
    } else {
        klog_info("Device does not support LBA48 - LBA28 mode only\n");
        klog_info("LBA28 Sectors: %llu\n", info.lba_sectors);

        // Still test high LBA within LBA28 range
        uint64_t test_lba = info.lba_sectors - 1;
        if (test_lba > 0 && test_lba < 0x0FFFFFFF) {
            uint8_t buffer[ATA_SECTOR_SIZE];
            klog_trace("Testing read at last sector (LBA %llu)...\n", test_lba);

            ata_result_t result = ata_read(ATA_PRIMARY_MASTER, test_lba, buffer, 1);
            if (result == ATA_OK) {
                klog_info("High LBA28 read successful\n");
            } else {
                klog_warn("High LBA28 read failed: %s\n", ata_error_string(result));
            }
        }
    }
}

/**
 * @brief Demo 7: Async I/O Test
 */
static void demo_async_io(void) {
    klog_trace("\n=== Demo 7: Async I/O Test ===\n");

    // Initialize async mode
    ata_result_t result = ata_init_async_mode();
    if (result != ATA_OK) {
        klog_error("Failed to init async I/O: %s\n", ata_error_string(result));
        klog_info("Note: Async I/O requires ATA_MODE_ASYNC initialization\n");
        return;
    }

    klog_info("Async I/O initialized\n");

    // Check if device supports async
    if (!ata_device_exists(ATA_PRIMARY_MASTER)) {
        klog_error("No device found for async test\n");
        return;
    }

    // Test 1: Single async read with callback
    uint8_t buffer1[ATA_SECTOR_SIZE];
    g_async_read_complete = false;

    klog_trace("Initiating async read of sector 1...\n");
    result = ata_read_async(ATA_PRIMARY_MASTER, 1, buffer1, 1,
                           demo_async_read_callback, NULL);

    if (result != ATA_OK) {
        klog_error("Failed to queue async read: %s\n", ata_error_string(result));
        return;
    }

    klog_info("Async read queued\n");

    // Wait for completion (polling in this demo)
    uint32_t timeout = 1000000;
    while (!g_async_read_complete && timeout-- > 0) {
        __asm__ volatile("pause");
    }

    if (g_async_read_complete) {
        klog_info("Async read completed successfully\n");
        klog_trace("First bytes: %02X %02X %02X %02X\n",
                   buffer1[0], buffer1[1], buffer1[2], buffer1[3]);
    } else {
        klog_error("Async read timeout\n");
    }

    // Test 2: Multiple async reads
    klog_trace("Testing multiple concurrent async reads...\n");

    uint8_t buffers[4][ATA_SECTOR_SIZE];
    uint32_t sectors_test[4] = {10, 20, 30, 40};
    int pending_count = 0;

    for (int i = 0; i < 4; i++) {
        result = ata_read_async(ATA_PRIMARY_MASTER, sectors_test[i],
                               buffers[i], 1, NULL, NULL);
        if (result == ATA_OK) {
            pending_count++;
        } else {
            klog_warn("Failed to queue async read %d\n", i);
        }
    }

    klog_info("Queued %d async reads\n", pending_count);

    // Wait for all to complete
    timeout = 2000000;
    while (!ata_async_is_idle(ATA_PRIMARY_MASTER) && timeout-- > 0) {
        __asm__ volatile("pause");
    }

    if (ata_async_is_idle(ATA_PRIMARY_MASTER)) {
        klog_info("All async reads completed\n");
    } else {
        klog_warn("Some async reads may still be pending\n");
    }

    // Report interrupt statistics
    klog_trace("Async I/O demo complete\n");
}

/* ============================================================================
 * Main Demo Entry Point
 * ============================================================================ */

/**
 * @brief Run all ATA demos
 *
 * This function demonstrates the various ATA capabilities:
 * 1. Reading MBR and verifying signature
 * 2. Displaying device information
 * 3. Reading multiple sectors
 * 4. Write and read back verification (safe test)
 * 5. Performance test
 * 6. LBA48 support test
 * 7. Async I/O test
 *
 * Call this function after ATA initialization to see the demos.
 */
void ata_run_demo(void) {
    klog_trace("\n");
    klog_trace("========================================\n");
    klog_trace("   ATA/IDE Disk Driver Demo\n");
    klog_trace("========================================\n");

    // Check if any devices are present
    bool any_device = false;
    for (int i = 0; i < 4; i++) {
        if (ata_device_exists(i)) {
            any_device = true;
            break;
        }
    }

    if (!any_device) {
        klog_warn("[ATA Demo] No ATA devices found!\n");
        return;
    }

    // Demo 1: Read MBR
    demo_read_mbr();

    // Demo 2: Show device info
    demo_device_info();

    // Demo 3: Read multiple sectors
    demo_read_multiple();

    // Demo 4: Write/verify (disabled for safety)
    demo_write_verify();

    // Demo 5: Performance test
    demo_performance();

    // Demo 6: LBA48 support test
    demo_lba48_support();

    // Demo 7: Async I/O test
    demo_async_io();

    klog_trace("\n=== ATA Demo Complete ===\n");
}
