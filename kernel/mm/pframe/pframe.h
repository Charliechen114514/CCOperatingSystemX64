/* ==============================================================================
 * CCOS - Physical Frame Allocator
 * ==============================================================================
 * This module provides physical memory frame allocation using a bitmap-based
 * approach. It manages 4KB physical frames and integrates with the E820
 * memory map to track available and reserved memory regions.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"

/* ==============================================================================
 * Page Size Constants
 * ============================================================================== */
#define PAGE_SIZE       4096    /* 4KB page size for x86_64 */
#define PAGE_SHIFT      12      /* log2(PAGE_SIZE) */

/* ==============================================================================
 * Memory Constraints
 * ============================================================================== */
#define PFRAME_MIN_ALLOC_BASE   0x100000  /* 1MB - skip BIOS/reserved areas */

/* ==============================================================================
 * Allocation Result Codes
 * ============================================================================== */
typedef enum {
    PFRAME_OK = 0,              /* Success */
    PFRAME_ERR_NOT_INIT = -1,   /* Allocator not initialized */
    PFRAME_ERR_OOM = -2,        /* Out of memory */
    PFRAME_ERR_INVALID = -3,    /* Invalid parameter */
    PFRAME_ERR_ALIGN = -4,      /* Alignment not supported */
} pframe_result_t;

/* ==============================================================================
 * Frame Allocator Statistics
 * ============================================================================== */
typedef struct {
    uint64_t total_frames;      /* Total frames in managed memory */
    uint64_t free_frames;       /* Currently free frames */
    uint64_t allocated_frames;  /* Currently allocated frames */
    uint64_t reserved_frames;   /* Reserved frames (kernel/bios) */
    uint64_t bitmap_size_bytes; /* Size of bitmap in bytes */
    uint64_t managed_start;     /* Start of managed memory region */
    uint64_t managed_end;       /* End of managed memory region */
} pframe_stats_t;

/* ==============================================================================
 * Physical Frame Allocator API
 * ============================================================================== */

/**
 * pframe_init - Initialize the physical frame allocator
 *
 * Parses E820 map, sets up bitmap, marks reserved regions.
 * Must be called after e820_init().
 *
 * @return PFRAME_OK on success, error code otherwise
 */
pframe_result_t pframe_init(void);

/**
 * pframe_alloc - Allocate a single physical frame
 *
 * @param out_addr Pointer to store allocated physical address
 * @return PFRAME_OK on success, error code on failure
 */
pframe_result_t pframe_alloc(physical_addr_t* out_addr);

/**
 * pframe_alloc_n - Allocate multiple contiguous physical frames
 *
 * @param out_addr Pointer to store allocated base address
 * @param frame_count Number of frames to allocate
 * @return PFRAME_OK on success, error code on failure
 */
pframe_result_t pframe_alloc_n(physical_addr_t* out_addr, uint64_t frame_count);

/**
 * pframe_free - Free a single physical frame
 *
 * @param addr Physical address of frame to free
 * @return PFRAME_OK on success, error code on failure
 */
pframe_result_t pframe_free(physical_addr_t addr);

/**
 * pframe_free_n - Free multiple contiguous physical frames
 *
 * @param addr Base physical address of frames to free
 * @param frame_count Number of frames to free
 * @return PFRAME_OK on success, error code on failure
 */
pframe_result_t pframe_free_n(physical_addr_t addr, uint64_t frame_count);

/**
 * pframe_get_stats - Get frame allocator statistics
 *
 * @param stats Pointer to store statistics
 * @return PFRAME_OK on success
 */
pframe_result_t pframe_get_stats(pframe_stats_t* stats);

/**
 * pframe_dump - Dump frame allocator state for debugging
 *
 * Prints current allocation map and statistics.
 */
void pframe_dump(void);

/**
 * pframe_is_allocated - Check if a physical address is currently allocated
 *
 * @param addr Physical address to check
 * @return true if allocated, false if free or invalid
 */
bool pframe_is_allocated(physical_addr_t addr);

/**
 * pframe_get_total_frames - Get total number of frames managed
 *
 * @return Total frame count
 */
uint64_t pframe_get_total_frames(void);

/**
 * pframe_get_free_frames - Get number of free frames
 *
 * @return Free frame count
 */
uint64_t pframe_get_free_frames(void);
