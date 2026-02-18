/* ==============================================================================
 * CCOS - E820 Memory Map Parser
 * ==============================================================================
 * This module parses the E820 memory map collected by the bootloader and
 * stored at the address defined in mem_config.h. It provides functions to
 * query memory layout and statistics.
 * ==============================================================================
 */
#pragma once

#include "defines/types.h"
#include "mem_config.h"

/* ==============================================================================
 * E820 Memory Types
 * ============================================================================== */
typedef enum {
    E820_TYPE_USABLE = 1,       /* Available RAM */
    E820_TYPE_RESERVED = 2,     /* Reserved, not available */
    E820_TYPE_ACPI_RECLAIM = 3, /* ACPI Reclaimable */
    E820_TYPE_NVS = 4,          /* ACPI NVS Memory */
    E820_TYPE_UNUSABLE = 5,     /* Unusable memory */
} e820_mem_type_t;

/* ==============================================================================
 * Memory Detection Method
 * ============================================================================== */
typedef enum {
    MEM_DETECT_E820 = 0,    /* INT 15h/E820 - Detailed memory map */
    MEM_DETECT_E801 = 1,    /* INT 15h/E801 - Two memory regions */
    MEM_DETECT_88H = 2,     /* INT 15h/88h - Max 64MB memory */
    MEM_DETECT_UNKNOWN = 3, /* No detection method worked */
} mem_detect_method_t;

/* ==============================================================================
 * E820 Memory Entry Structure (24 bytes)
 * ==============================================================================
 * Layout matches the bootloader's storage format:
 *   Offset 0-7:   Base address (64-bit)
 *   Offset 8-15:  Length (64-bit)
 *   Offset 16-19: Type (32-bit)
 *   Offset 20-23: ACPI attributes (32-bit)
 * ============================================================================== */
typedef struct PACKED {
    uint64_t base;       /* Base address of the memory region */
    uint64_t length;     /* Length of the memory region in bytes */
    uint32_t type;       /* Memory type (see e820_mem_type_t) */
    uint32_t acpi_attrs; /* ACPI extended attributes (usually 0) */
} e820_entry_t;

/* ==============================================================================
 * Memory Statistics
 * ============================================================================== */
typedef struct {
    uint64_t total_bytes;    /* Total physical memory in bytes */
    uint64_t usable_bytes;   /* Usable (available) memory in bytes */
    uint64_t reserved_bytes; /* Reserved memory in bytes */
    uint32_t total_mb;       /* Total memory in MB */
    uint32_t usable_mb;      /* Usable memory in MB */
    uint32_t entry_count;    /* Number of E820 entries */
} mem_stats_t;

/* ==============================================================================
 * E820 API Functions
 * ============================================================================== */

/**
 * e820_init - Initialize the E820 memory map parser
 *
 * Reads the memory map from E820_STORAGE_ADDR and parses it.
 * Must be called before any other E820 functions.
 *
 * @return void
 */
void e820_init(void);

/**
 * e820_get_detect_method - Get the memory detection method used
 *
 * @return The detection method (mem_detect_method_t)
 */
mem_detect_method_t e820_get_detect_method(void);

/**
 * e820_get_detect_method_name - Get name of detection method
 *
 * @param method The detection method
 * @return String name of the method
 */
const char* e820_get_detect_method_name(mem_detect_method_t method);

/**
 * e820_get_entry_count - Get the number of E820 entries
 *
 * @return Number of entries in the memory map
 */
uint32_t e820_get_entry_count(void);

/**
 * e820_get_entry - Get a specific E820 entry
 *
 * @param index The entry index (0-based)
 * @param entry Pointer to store the entry data
 * @return true if entry exists, false otherwise
 */
bool e820_get_entry(uint32_t index, e820_entry_t* entry);

/**
 * e820_get_stats - Get memory statistics
 *
 * Calculates and returns statistics about the system memory.
 *
 * @param stats Pointer to store the statistics
 */
void e820_get_stats(mem_stats_t* stats);

/**
 * e820_dump_map - Print the memory map to serial/VGA
 *
 * Outputs a formatted table of all memory entries.
 */
void e820_dump_map(void);

/**
 * e820_is_range_usable - Check if a memory range is usable
 *
 * Checks if the entire range [base, base+length) is marked as usable.
 *
 * @param base Base address of the range
 * @param length Length of the range in bytes
 * @return true if range is fully usable, false otherwise
 */
bool e820_is_range_usable(uint64_t base, uint64_t length);

/**
 * e820_find_usable_range - Find the next usable memory range
 *
 * Finds the first usable memory range at or above the given address.
 *
 * @param min_base Minimum base address to search from
 * @param min_length Minimum length required
 * @param out_base Pointer to store the found base address
 * @param out_length Pointer to store the found length
 * @return true if a suitable range was found, false otherwise
 */
bool e820_find_usable_range(uint64_t min_base, uint64_t min_length, uint64_t* out_base,
                            uint64_t* out_length);

/**
 * e820_get_type_name - Get the name of an E820 memory type
 *
 * @param type The E820 type
 * @return String name of the type
 */
const char* e820_get_type_name(uint32_t type);

/**
 * e820_get_usable_memory_above - Find usable memory above a threshold
 *
 * Finds the largest contiguous usable memory region above the given address.
 * Useful for finding memory for kernel heap or other large allocations.
 *
 * @param min_base Minimum base address
 * @return Size of the largest usable region above min_base, or 0 if none
 */
uint64_t e820_get_usable_memory_above(uint64_t min_base);

/**
 * e820_count_usable_regions - Count number of usable memory regions
 *
 * @return Number of regions marked as usable
 */
uint32_t e820_count_usable_regions(void);
