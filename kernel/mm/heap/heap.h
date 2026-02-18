/* ==============================================================================
 * CCOS - Kernel Heap Allocator
 * ==============================================================================
 * This module provides dynamic memory allocation (kmalloc/kfree) for the
 * kernel. It implements a best-fit allocator with free block coalescing.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"

/* ==============================================================================
 * Result Codes
 * ============================================================================== */

typedef enum {
    HEAP_OK = 0,
    HEAP_ERR_NOT_INIT = -1,
    HEAP_ERR_OOM = -2,
    HEAP_ERR_INVALID = -3,
    HEAP_ERR_DOUBLE_FREE = -4,
    HEAP_ERR_CORRUPTED = -5,
} heap_result_t;

/* ==============================================================================
 * Statistics
 * ============================================================================== */

typedef struct {
    uint64_t total_bytes;  /* Total heap bytes managed */
    uint64_t used_bytes;   /* Bytes currently allocated */
    uint64_t free_bytes;   /* Bytes available */
    uint64_t total_blocks; /* Total blocks in heap */
    uint64_t used_blocks;  /* Allocated blocks */
    uint64_t free_blocks;  /* Free blocks */
    uint64_t alloc_count;  /* Total allocations */
    uint64_t free_count;   /* Total frees */
    uint64_t expand_count; /* Times heap expanded */
} heap_stats_t;

/* ==============================================================================
 * API
 * ============================================================================== */

/**
 * heap_init - Initialize the kernel heap allocator
 *
 * Sets up the heap region and initial free block.
 * Must be called after vmm_init().
 *
 * @return HEAP_OK on success, error code otherwise
 */
heap_result_t heap_init(void);

/**
 * kmalloc - Allocate memory from kernel heap
 *
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void* kmalloc(size_t size);

/**
 * kfree - Free memory allocated from kernel heap
 *
 * @param ptr Pointer to memory to free (NULL is safe)
 */
void kfree(void* ptr);

/**
 * krealloc - Reallocate memory with new size
 *
 * If ptr is NULL, equivalent to kmalloc(new_size).
 * If new_size is 0, equivalent to kfree(ptr).
 *
 * @param ptr Original pointer (may be NULL)
 * @param new_size New size in bytes
 * @return Pointer to reallocated memory, or NULL if failed
 */
void* krealloc(void* ptr, size_t new_size);

/**
 * kmalloc_aligned - Allocate aligned memory
 *
 * @param size Number of bytes to allocate
 * @param alignment Alignment in bytes (must be power of 2)
 * @return Pointer to aligned memory, or NULL if failed
 */
void* kmalloc_aligned(size_t size, size_t alignment);

/**
 * heap_get_stats - Get heap statistics
 *
 * @param stats Pointer to stats structure to fill
 * @return HEAP_OK on success
 */
heap_result_t heap_get_stats(heap_stats_t* stats);

/**
 * heap_dump - Dump heap state for debugging
 *
 * Prints current heap status and block information.
 */
void heap_dump(void);
