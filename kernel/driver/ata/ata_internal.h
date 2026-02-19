/**
 * @file ata_internal.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief ATA Driver Internal Structures and Functions
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "defines/types.h"
#include "interrupt/idt.h"
#include "ata_constants.h"

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

// Forward declarations
typedef struct ata_async_queue ata_async_queue_t;

/**
 * @brief ATA Controller State
 *
 * Represents an ATA controller (Primary or Secondary) with its
 * I/O ports and device presence status.
 */
typedef struct ata_controller {
    uint16_t io_base;           // Base I/O port (0x1F0 or 0x170)
    uint16_t ctrl_base;         // Control port (0x3F6 or 0x376)
    uint8_t irq;                // IRQ number (14 or 15)
    bool initialized;           // Controller initialized flag

    // Device presence
    bool master_present;        // Master drive detected
    bool slave_present;         // Slave drive detected

    // Statistics
    uint64_t read_count;        // Number of read operations
    uint64_t write_count;       // Number of write operations
    uint64_t error_count;       // Number of errors

    // Async I/O support
    bool interrupts_enabled;    // Interrupts enabled flag
    bool use_async_mode;        // Async mode enabled
    ata_async_queue_t* queue;   // Pending operations queue (allocated when async init)
    irq_descriptor_t irq_desc;  // IRQ descriptor for this controller
} ata_controller_t;

/**
 * @brief ATA Device Information
 *
 * Stores information about an ATA device obtained from IDENTIFY command.
 */
typedef struct ata_device_info {
    bool exists;                // Device is present
    bool lba_supported;         // LBA addressing supported
    bool lba48_supported;       // LBA48 addressing supported
    bool dma_supported;         // DMA mode supported

    char model[41];             // Model number (null-terminated)
    char serial[21];            // Serial number (null-terminated)
    char firmware[9];           // Firmware revision (null-terminated)

    uint64_t lba_sectors;       // Number of sectors (LBA28)
    uint64_t lba48_sectors;     // Number of sectors (LBA48)
    uint32_t cylinders;         // Physical cylinders (CHS)
    uint32_t heads;             // Physical heads (CHS)
    uint32_t sectors_per_track; // Physical sectors per track (CHS)
} ata_device_info_t;

/**
 * @brief ATA Device Selection
 */
typedef enum {
    ATA_DEVICE_MASTER = 0,
    ATA_DEVICE_SLAVE  = 1
} ata_device_t;

/**
 * @brief ATA Result Codes
 */
typedef enum {
    ATA_OK               = 0,    // Operation successful
    ATA_ERR_NOT_INIT     = -1,   // Controller not initialized
    ATA_ERR_TIMEOUT      = -2,   // Operation timed out
    ATA_ERR_NO_DEVICE    = -3,   // Device not present
    ATA_ERR_IO_ERROR     = -4,   // I/O error occurred
    ATA_ERR_INVALID_PARAM = -5,  // Invalid parameter
    ATA_ERR_UNSUPPORTED  = -6,   // Operation not supported
} ata_result_t;

/* ============================================================================
 * Async I/O Data Structures
 * ============================================================================ */

/**
 * @brief Async operation types
 */
typedef enum {
    ATA_ASYNC_OP_READ,
    ATA_ASYNC_OP_WRITE,
    ATA_ASYNC_OP_FLUSH
} ata_async_op_type_t;

/**
 * @brief Async operation status
 */
typedef enum {
    ATA_ASYNC_PENDING,     // Operation queued, not started
    ATA_ASYNC_IN_PROGRESS, // Operation in progress, waiting for interrupt
    ATA_ASYNC_COMPLETED,   // Operation completed successfully
    ATA_ASYNC_ERROR        // Operation failed
} ata_async_status_t;

/**
 * @brief Completion callback type
 *
 * @param device Device number (0-3)
 * @param lba Starting LBA address
 * @param sectors Number of sectors
 * @param buffer Data buffer
 * @param result ATA_OK on success, error code otherwise
 * @param context User-provided context pointer
 */
typedef void (*ata_async_callback_fn)(int device, uint64_t lba, uint16_t sectors,
                                      void* buffer, ata_result_t result, void* context);

/**
 * @brief Async operation descriptor
 */
typedef struct ata_async_operation {
    ata_async_op_type_t type;        // Operation type
    ata_async_status_t status;       // Current status

    // Operation parameters
    int device;                      // Device number (0-3)
    ata_device_t drive;              // Master/Slave
    uint64_t lba;                    // Starting LBA
    uint16_t sectors;                // Sector count
    uint16_t sectors_done;           // Progress tracking
    void* buffer;                    // Data buffer

    // Completion notification
    ata_async_callback_fn callback;  // Completion callback
    void* context;                   // User context for callback

    // Internal state
    bool use_lba48;                  // Using LBA48 addressing

    // Queue linkage
    struct ata_async_operation* next;
} ata_async_operation_t;

/**
 * @brief Async queue per controller
 */
typedef struct ata_async_queue {
    struct ata_async_operation* head;     // Queue head
    struct ata_async_operation* tail;     // Queue tail
    struct ata_async_operation* active;   // Currently active operation
    volatile bool irq_pending;             // IRQ pending flag
} ata_async_queue_t;

/* ============================================================================
 * Internal Function Prototypes
 * ============================================================================ */

/**
 * @brief Select a device on the controller
 *
 * @param ctrl Pointer to controller
 * @param device Device to select (master/slave)
 * @return ata_result_t ATA_OK or error code
 */
ata_result_t ata_select_device(ata_controller_t* ctrl, ata_device_t device);

/**
 * @brief Wait for BSY bit to clear
 *
 * @param io_base I/O base port
 * @return ata_result_t ATA_OK or ATA_ERR_TIMEOUT
 */
ata_result_t ata_wait_bsy(uint16_t io_base);

/**
 * @brief Wait for DRQ bit to set
 *
 * @param io_base I/O base port
 * @return ata_result_t ATA_OK or ATA_ERR_TIMEOUT
 */
ata_result_t ata_wait_drq(uint16_t io_base);

/**
 * @brief Check if device is present
 *
 * Sends IDENTIFY command and checks for response.
 *
 * @param ctrl Pointer to controller
 * @param device Device to check
 * @return true if device is present, false otherwise
 */
bool ata_device_present(ata_controller_t* ctrl, ata_device_t device);

/**
 * @brief Send IDENTIFY command and read response
 *
 * @param ctrl Pointer to controller
 * @param device Device to identify
 * @param info Pointer to store device info
 * @return ata_result_t ATA_OK or error code
 */
ata_result_t ata_identify_device(ata_controller_t* ctrl, ata_device_t device,
                                  ata_device_info_t* info);

/**
 * @brief Read sectors using PIO mode
 *
 * @param ctrl Pointer to controller
 * @param device Device to read from
 * @param lba Starting LBA address
 * @param buffer Buffer to store data (must be at least sectors * 512 bytes)
 * @param sectors Number of sectors to read (1-256)
 * @return ata_result_t ATA_OK or error code
 */
ata_result_t ata_read_pio(ata_controller_t* ctrl, ata_device_t device,
                           uint32_t lba, void* buffer, uint16_t sectors);

/**
 * @brief Write sectors using PIO mode
 *
 * @param ctrl Pointer to controller
 * @param device Device to write to
 * @param lba Starting LBA address
 * @param buffer Data to write (must be at least sectors * 512 bytes)
 * @param sectors Number of sectors to write (1-256)
 * @return ata_result_t ATA_OK or error code
 */
ata_result_t ata_write_pio(ata_controller_t* ctrl, ata_device_t device,
                            uint32_t lba, const void* buffer, uint16_t sectors);

/**
 * @brief Read sectors using PIO mode with LBA48 addressing
 *
 * @param ctrl Pointer to controller
 * @param device Device to read from
 * @param lba Starting LBA address (64-bit)
 * @param buffer Buffer to store data
 * @param sectors Number of sectors to read (1-65536)
 * @return ata_result_t ATA_OK or error code
 */
ata_result_t ata_read_pio_lba48(ata_controller_t* ctrl, ata_device_t device,
                                uint64_t lba, void* buffer, uint16_t sectors);

/**
 * @brief Write sectors using PIO mode with LBA48 addressing
 *
 * @param ctrl Pointer to controller
 * @param device Device to write to
 * @param lba Starting LBA address (64-bit)
 * @param buffer Data to write
 * @param sectors Number of sectors to write (1-65536)
 * @return ata_result_t ATA_OK or error code
 */
ata_result_t ata_write_pio_lba48(ata_controller_t* ctrl, ata_device_t device,
                                 uint64_t lba, const void* buffer, uint16_t sectors);

/**
 * @brief Flush device cache (LBA48 version)
 *
 * @param ctrl Pointer to controller
 * @return ata_result_t ATA_OK or error code
 */
ata_result_t ata_flush_cache_ext(ata_controller_t* ctrl);

/**
 * @brief Software reset the controller
 *
 * @param ctrl Pointer to controller
 */
void ata_soft_reset(ata_controller_t* ctrl);

/**
 * @brief Convert big-endian words to string
 *
 * IDENTIFY data stores strings in big-endian words.
 *
 * @param src Source buffer (words)
 * @param dest Destination buffer (bytes)
 * @param words Number of words to convert
 */
void ata_identify_string_to_c(const uint16_t* src, char* dest, int words);

/* ============================================================================
 * Async I/O Function Prototypes
 * ============================================================================ */

/**
 * @brief Initialize interrupt-driven async I/O for a controller
 *
 * @param ctrl Pointer to controller
 * @return ata_result_t ATA_OK or error code
 */
ata_result_t ata_init_async_io(ata_controller_t* ctrl);

/**
 * @brief Enable interrupts for a controller
 *
 * @param ctrl Pointer to controller
 */
void ata_enable_interrupts(ata_controller_t* ctrl);

/**
 * @brief Disable interrupts for a controller
 *
 * @param ctrl Pointer to controller
 */
void ata_disable_interrupts(ata_controller_t* ctrl);

/**
 * @brief Start next async operation from queue
 *
 * @param ctrl Controller
 */
void ata_start_next_async_op(ata_controller_t* ctrl);

/**
 * @brief IRQ handler for ATA controller
 *
 * @param frame Interrupt frame
 * @param context Controller pointer
 */
void ata_irq_handler(interrupt_frame_t* frame, void* context);

/**
 * @brief Complete an async operation
 *
 * @param op Operation to complete
 * @param result Result code
 */
void ata_complete_async_op(ata_async_operation_t* op, ata_result_t result);
