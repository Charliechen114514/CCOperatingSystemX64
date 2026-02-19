/* ==============================================================================
 * CCOS - Memory State Helper Functions
 * ==============================================================================
 * This module provides reusable helper functions for memory management modules.
 * Functions include size formatting, unit conversion, and memory statistics.
 * ==============================================================================
 */
#pragma once
#include "defines/types.h"

/* ==============================================================================
 * Size Unit Enum
 * ============================================================================== */
typedef enum {
    SIZE_UNIT_BYTES,
    SIZE_UNIT_KB,
    SIZE_UNIT_MB,
    SIZE_UNIT_GB,
    SIZE_UNIT_TB,
} size_unit_t;

/* ==============================================================================
 * Formatted Size Structure
 * ============================================================================== */
typedef struct {
    uint64_t value;
    size_unit_t unit;
    char string[32];
} formatted_size_t;

/* ==============================================================================
 * Memory Region Summary Structure
 * ============================================================================== */
typedef struct {
    uint64_t total_bytes;
    uint64_t usable_bytes;
    uint64_t reserved_bytes;
    uint64_t total_pages;
    uint64_t usable_pages;
    uint32_t total_mb;
    uint32_t usable_mb;
} memory_summary_t;

/* ==============================================================================
 * Size Formatting Functions
 * ============================================================================== */

/**
 * bytes_to_mb - Convert bytes to megabytes
 * @param bytes: Size in bytes
 * @return Size in megabytes
 */
uint32_t bytes_to_mb(uint64_t bytes);

/**
 * bytes_to_kb - Convert bytes to kilobytes
 * @param bytes: Size in bytes
 * @return Size in kilobytes
 */
uint64_t bytes_to_kb(uint64_t bytes);

/**
 * mb_to_bytes - Convert megabytes to bytes
 * @param mb: Size in megabytes
 * @return Size in bytes
 */
uint64_t mb_to_bytes(uint32_t mb);

/**
 * kb_to_bytes - Convert kilobytes to bytes
 * @param kb: Size in kilobytes
 * @return Size in bytes
 */
uint64_t kb_to_bytes(uint64_t kb);

/**
 * pages_to_bytes - Convert page count to bytes
 * @param pages: Number of pages
 * @return Size in bytes (assuming 4KB pages)
 */
uint64_t pages_to_bytes(uint64_t pages);

/**
 * bytes_to_pages - Convert bytes to page count
 * @param bytes: Size in bytes
 * @return Number of pages (rounded up)
 */
uint64_t bytes_to_pages(uint64_t bytes);

/**
 * align_up - Align a value up to the next multiple
 * @param value: Value to align
 * @param alignment: Alignment power (e.g., 12 for 4096)
 * @return Aligned value
 */
uint64_t align_up(uint64_t value, uint32_t alignment);

/**
 * align_down - Align a value down to the previous multiple
 * @param value: Value to align
 * @param alignment: Alignment power (e.g., 12 for 4096)
 * @return Aligned value
 */
uint64_t align_down(uint64_t value, uint32_t alignment);

/**
 * format_size - Format a size into human-readable string
 * @param size: Size in bytes
 * @param buf: Buffer to store formatted string
 * @param buf_len: Buffer length
 * @return Pointer to formatted string
 */
const char* format_size(uint64_t size, char* buf, size_t buf_len);

/**
 * format_size_ex - Extended size formatting with unit info
 * @param size: Size in bytes
 * @param fs: Pointer to formatted_size_t structure to fill
 * @return Pointer to formatted string
 */
const char* format_size_ex(uint64_t size, formatted_size_t* fs);

/**
 * format_size_unit - Format size with specific unit
 * @param size: Size in bytes
 * @param unit: Desired unit
 * @param buf: Buffer to store formatted string
 * @param buf_len: Buffer length
 * @return Pointer to formatted string
 */
const char* format_size_unit(uint64_t size, size_unit_t unit, char* buf, size_t buf_len);

/* ==============================================================================
 * Memory Summary Functions
 * ============================================================================== */

/**
 * mem_summary_init - Initialize memory summary structure
 * @param summary: Pointer to memory_summary_t to initialize
 */
void mem_summary_init(memory_summary_t* summary);

/**
 * mem_summary_add_region - Add a memory region to summary
 * @param summary: Pointer to memory_summary_t
 * @param base: Base address of region
 * @param length: Length of region in bytes
 * @param is_usable: Whether the region is usable
 */
void mem_summary_add_region(memory_summary_t* summary, uint64_t base, uint64_t length,
                            bool is_usable);

/**
 * mem_summary_calculate_mb - Calculate MB values from bytes
 * @param summary: Pointer to memory_summary_t
 */
void mem_summary_calculate_mb(memory_summary_t* summary);

/**
 * mem_summary_calculate_pages - Calculate page counts from bytes
 * @param summary: Pointer to memory_summary_t
 */
void mem_summary_calculate_pages(memory_summary_t* summary);

/**
 * mem_summary_dump - Print memory summary
 * @param summary: Pointer to memory_summary_t
 */
void mem_summary_dump(const memory_summary_t* summary);

/* ==============================================================================
 * Address Range Functions
 * ============================================================================== */

/**
 * ranges_overlap - Check if two ranges overlap
 * @param start1: Start of first range
 * @param end1: End of first range (exclusive)
 * @param start2: Start of second range
 * @param end2: End of second range (exclusive)
 * @return true if ranges overlap
 */
bool ranges_overlap(uint64_t start1, uint64_t end1, uint64_t start2, uint64_t end2);

/**
 * range_contains - Check if a range contains another range
 * @param outer_start: Start of outer range
 * @param outer_end: End of outer range (exclusive)
 * @param inner_start: Start of inner range
 * @param inner_end: End of inner range (exclusive)
 * @return true if outer range fully contains inner range
 */
bool range_contains(uint64_t outer_start, uint64_t outer_end, uint64_t inner_start,
                    uint64_t inner_end);

/**
 * is_aligned - Check if a value is aligned
 * @param value: Value to check
 * @param alignment: Alignment power (e.g., 12 for 4096)
 * @return true if aligned
 */
bool is_aligned(uint64_t value, uint32_t alignment);
