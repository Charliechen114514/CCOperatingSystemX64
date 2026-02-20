/* ==============================================================================
 * CCOS - Kernel Heap Allocator Implementation
 * ==============================================================================
 */

#include "mm/heap/heap.h"
#include "assert/assert.h"
#include "base/memory.h"
#include "heap_config.h"
#include "klogs/kprintf.h"
#include "math/bits.h"
#include "mm/pframe/pframe.h"
#include "mm/vmm/vmm.h"
#include "sync/spinlock.h"

/* ==============================================================================
 * Internal Constants
 * ============================================================================== */

#define HEAP_BLOCK_MAGIC 0x00114514 /* "HEAP" magic number for debugging */

/* ==============================================================================
 * Internal Data Structures
 * ============================================================================== */

/**
 * @brief Heap block header
 *
 * Each block (free or allocated) has this header.
 * The heap is a contiguous sequence of blocks.
 * Aligned to 16 bytes to ensure user pointers are also aligned.
 */
typedef struct heap_block {
    uint64_t size;           /* Total block size including header */
    bool used;               /* true = allocated, false = free */
    struct heap_block* prev; /* Previous block in physical order */
    struct heap_block* next; /* Next block in free list (only valid if free) */
    uint32_t magic;          /* Magic number for corruption detection */
    uint32_t _padding;       /* Explicit padding to make size 16-byte aligned */
} __attribute__((aligned(16))) heap_block_t;

/**
 * @brief Heap state
 */
typedef struct {
    virtual_addr_t heap_start; /* Start of heap region */
    virtual_addr_t heap_end;   /* End of heap region */
    virtual_addr_t heap_brk;   /* Current heap break (top of allocated space) */
    heap_block_t* free_list;   /* Head of free list (best-fit search) */
    heap_stats_t stats;        /* Statistics */
    bool initialized;
} heap_state_t;

/* ==============================================================================
 * Internal State
 * ============================================================================== */

static heap_state_t s_heap = {0};

/* Spinlock protecting the heap allocator state */
static spinlock_t s_heap_lock = SPIN_LOCK_INIT;

/* ==============================================================================
 * Internal Helper Functions
 * ============================================================================== */

/**
 * align_size - Align size to HEAP_ALIGN boundary
 */
static inline size_t align_size(size_t size) {
    return (size + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);
}

/**
 * total_block_size - Calculate total block size including header
 */
static inline size_t total_block_size(size_t user_size) {
    return align_size(user_size) + sizeof(heap_block_t);
}

/**
 * block_to_ptr - Convert block pointer to user pointer
 */
static inline void* block_to_ptr(heap_block_t* block) {
    return (void*)((virtual_addr_t)block + sizeof(heap_block_t));
}

/**
 * ptr_to_block - Convert user pointer to block pointer
 *
 * This function also handles aligned allocations by checking if there's
 * a stored original pointer before the user pointer.
 */
static inline heap_block_t* ptr_to_block(void* ptr) {
    virtual_addr_t addr = (virtual_addr_t)ptr;

    /* Check if there's an original pointer stored before this address */
    /* This happens with kmalloc_aligned allocations */
    virtual_addr_t* orig_ptr_loc = (virtual_addr_t*)(addr - sizeof(virtual_addr_t*));

    /* If the value at orig_ptr_loc points to a valid heap region,
     * and is reasonably aligned, assume this is an aligned allocation */
    if (*orig_ptr_loc >= s_heap.heap_start && *orig_ptr_loc < s_heap.heap_brk &&
        (*orig_ptr_loc & 0xF) == 0) { /* 16-byte aligned hint */
        /* orig_ptr_loc stores the user pointer of the raw allocation,
         * so we need to go back further to get the block header */
        virtual_addr_t raw_user_ptr = *orig_ptr_loc;
        heap_block_t* potential_block = (heap_block_t*)(raw_user_ptr - sizeof(heap_block_t));

        /* Now check if this is a valid block with correct magic */
        if (potential_block->magic == HEAP_BLOCK_MAGIC) {
            /* Verify the aligned address is actually within this block */
            virtual_addr_t block_start = raw_user_ptr;
            virtual_addr_t block_end = raw_user_ptr + potential_block->size - sizeof(heap_block_t);
            if (addr >= block_start && addr < block_end) {
                return potential_block;
            }
        }
    }

    /* Normal allocation: block header is immediately before user pointer */
    return (heap_block_t*)(addr - sizeof(heap_block_t));
}

/**
 * next_block - Get the next block in physical memory order
 */
static inline heap_block_t* next_block(heap_block_t* block) {
    return (heap_block_t*)((virtual_addr_t)block + block->size);
}

/**
 * is_last_block - Check if this is the last block
 */
static inline bool is_last_block(heap_block_t* block) {
    virtual_addr_t next_addr = (virtual_addr_t)next_block(block);
    /* Check if next block is beyond the current heap break OR beyond heap region */
    return (next_addr >= s_heap.heap_brk) || (next_addr >= s_heap.heap_end);
}

/**
 * validate_block - Validate block integrity
 */
static bool validate_block(heap_block_t* block) {
    if (block == NULL) {
        return false;
    }

    /* Check magic */
    if (block->magic != HEAP_BLOCK_MAGIC) {
        klog_error("[HEAP] Invalid magic at 0x%llX: 0x%X\n", (virtual_addr_t)block, block->magic);
        return false;
    }

    /* Check address range */
    if ((virtual_addr_t)block < s_heap.heap_start || (virtual_addr_t)block >= s_heap.heap_end) {
        klog_error("[HEAP] Block 0x%llX out of heap range\n", (virtual_addr_t)block);
        return false;
    }

    /* Check alignment */
    if (((virtual_addr_t)block & (HEAP_ALIGN - 1)) != 0) {
        klog_error("[HEAP] Block 0x%llX not aligned\n", (virtual_addr_t)block);
        return false;
    }

    /* Check size */
    if (block->size < sizeof(heap_block_t) ||
        (virtual_addr_t)block + block->size > s_heap.heap_end) {
        klog_error("[HEAP] Invalid block size: %llu\n", block->size);
        return false;
    }

    return true;
}

/**
 * remove_from_free_list - Remove a block from free list
 */
static void remove_from_free_list(heap_block_t* block) {
    CCOS_ASSERT(!block->used);

    heap_block_t* prev = block->prev;
    heap_block_t* next = block->next;

    if (prev) {
        prev->next = next;
    } else {
        /* This was the head */
        s_heap.free_list = next;
    }

    if (next) {
        next->prev = prev;
    }

    block->prev = NULL;
    block->next = NULL;
}

/**
 * insert_to_free_list - Insert a block to free list (sorted by address)
 */
static void insert_to_free_list(heap_block_t* block) {
    CCOS_ASSERT(!block->used);
    CCOS_ASSERT(block->prev == NULL);
    CCOS_ASSERT(block->next == NULL);

    /* Insert sorted by address for better coalescing */
    heap_block_t* curr = s_heap.free_list;
    heap_block_t* prev = NULL;

    while (curr && curr < block) {
        prev = curr;
        curr = curr->next;
    }

    block->next = curr;
    if (curr) {
        curr->prev = block;
    }
    block->prev = prev;
    if (prev) {
        prev->next = block;
    } else {
        s_heap.free_list = block;
    }
}

/**
 * find_best_fit - Find the best fitting free block (best-fit algorithm)
 */
static heap_block_t* find_best_fit(size_t size) {
    heap_block_t* best = NULL;
    heap_block_t* curr = s_heap.free_list;

    while (curr) {
        if (curr->size >= size) {
            if (best == NULL || curr->size < best->size) {
                best = curr;
                /* Exact match is optimal */
                if (curr->size == size) {
                    break;
                }
            }
        }
        curr = curr->next;
    }

    return best;
}

/**
 * split_block - Split a block if it's large enough
 */
static void split_block(heap_block_t* block, size_t needed_size) {
    size_t remaining = block->size - needed_size;

    /* Only split if remaining is large enough for a new block */
    if (remaining >= HEAP_MIN_BLOCK) {
        heap_block_t* new_block = (heap_block_t*)((virtual_addr_t)block + needed_size);

        new_block->size = remaining;
        new_block->used = false;
        new_block->magic = HEAP_BLOCK_MAGIC;
        new_block->prev = NULL;
        new_block->next = NULL;

        /* Update original block */
        block->size = needed_size;

        /* Insert new block to free list */
        insert_to_free_list(new_block);

        /* Update stats */
        s_heap.stats.total_blocks++;
        s_heap.stats.free_blocks++;
        s_heap.stats.free_bytes += remaining;

        klog_trace("[HEAP] Split block: 0x%llX -> %llu + %llu\n", (virtual_addr_t)block,
                   needed_size, remaining);
    }
}

/**
 * coalesce_block - Merge a free block with adjacent free blocks
 */
static void coalesce_block(heap_block_t* block) {
    CCOS_ASSERT(!block->used);

    /* Try to merge with next block */
    if (!is_last_block(block)) {
        heap_block_t* next = next_block(block);
        if (!next->used && validate_block(next)) {
            klog_trace("[HEAP] Coalescing with next block: 0x%llX + 0x%llX\n",
                       (virtual_addr_t)block, (virtual_addr_t)next);

            /* Remove next from free list */
            remove_from_free_list(next);

            /* Merge sizes */
            block->size += next->size;

            /* Update stats */
            s_heap.stats.total_blocks--;
            s_heap.stats.free_blocks--;
        }
    }

    /* Try to merge with previous block */
    /* Need to find previous block by scanning from start or using prev pointer */
    /* For simplicity, we'll use the free list which is sorted by address */
    heap_block_t* prev_in_free = NULL;
    heap_block_t* curr = s_heap.free_list;
    while (curr && curr < block) {
        prev_in_free = curr;
        curr = curr->next;
    }

    if (prev_in_free) {
        /* Check if prev_in_free is immediately before block */
        if ((virtual_addr_t)prev_in_free + prev_in_free->size == (virtual_addr_t)block) {
            klog_trace("[HEAP] Coalescing with prev block: 0x%llX + 0x%llX\n",
                       (virtual_addr_t)prev_in_free, (virtual_addr_t)block);

            /* Merge sizes into prev */
            prev_in_free->size += block->size;

            /* Remove current block from free list (it's being merged) */
            remove_from_free_list(block);

            /* Update stats */
            s_heap.stats.total_blocks--;
            s_heap.stats.free_blocks--;
        }
    }
}

/**
 * expand_heap_locked - Expand heap by allocating more pages (caller must hold lock)
 *
 * IMPORTANT: Caller must hold s_heap_lock when calling this function!
 * This function assumes the lock is already held to prevent race conditions.
 */
static heap_result_t expand_heap_locked(size_t min_needed) {
    /* Calculate pages needed */
    size_t page_count = (min_needed + PAGE_SIZE - 1) / PAGE_SIZE;
    if (page_count < 4) {
        page_count = 4; /* Allocate at least 4 pages (16KB) */
    }

    klog_info("[HEAP] Expanding heap by %lu pages (%lu KB)\n", page_count,
              (page_count * PAGE_SIZE) / 1024);

    /* Allocate pages at the current heap break to ensure continuity
     * If heap_brk is 0 (first allocation), use KERNEL_HEAP_BASE */
    virtual_addr_t target_vaddr = s_heap.heap_brk;
    if (target_vaddr == 0) {
        target_vaddr = KERNEL_HEAP_BASE;
    }

    /* Align target_vaddr to page boundary */
    virtual_addr_t orig_vaddr = target_vaddr;
    target_vaddr = (target_vaddr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (orig_vaddr != target_vaddr) {
        klog_trace("[HEAP] Aligning heap_brk: 0x%llX -> 0x%llX\n", orig_vaddr, target_vaddr);
    }

    klog_trace("[HEAP] Attempting to allocate %lu pages at 0x%llX\n", page_count, target_vaddr);

    /*
     * NOTE: vmm_alloc_pages_at now acquires its own lock internally.
     * The heap lock remains held, which prevents concurrent heap operations
     * while we're expanding the heap.
     */

    vmm_result_t vmm_result = vmm_alloc_pages_at(target_vaddr, page_count, VMAP_FLAG_WRITE);
    if (vmm_result != VMM_OK) {
        klog_error("[HEAP] Failed to allocate pages at 0x%llX for expansion\n", target_vaddr);
        return HEAP_ERR_OOM;
    }

    /* Initialize as a new free block */
    heap_block_t* new_block = (heap_block_t*)target_vaddr;
    new_block->size = page_count * PAGE_SIZE;
    new_block->used = false;
    new_block->magic = HEAP_BLOCK_MAGIC;
    new_block->prev = NULL;
    new_block->next = NULL;

    /* Clear the memory */
    memset((void*)(target_vaddr + sizeof(heap_block_t)), 0,
           new_block->size - sizeof(heap_block_t));

    /* Insert to free list (heap lock is still held) */
    insert_to_free_list(new_block);

    /* Update heap break (should always be page-aligned after this) */
    s_heap.heap_brk = target_vaddr + new_block->size;

    /* Update stats */
    s_heap.stats.total_bytes += new_block->size;
    s_heap.stats.free_bytes += new_block->size;
    s_heap.stats.total_blocks++;
    s_heap.stats.free_blocks++;
    s_heap.stats.expand_count++;

    /* Try to coalesce with the last block if adjacent */
    coalesce_block(new_block);

    return HEAP_OK;
}

/* ==============================================================================
 * Public API Implementation
 * ============================================================================== */

/**
 * heap_init - Initialize the kernel heap allocator
 */
heap_result_t heap_init(void) {
    if (s_heap.initialized) {
        klog_warn("[HEAP] Already initialized\n");
        return HEAP_OK;
    }

    klog_info("[HEAP] Initializing Kernel Heap Allocator...\n");

    /* Clear state */
    memset(&s_heap, 0, sizeof(s_heap));

    /* Set up heap region boundaries (max possible range) */
    s_heap.heap_start = KERNEL_HEAP_BASE;
    s_heap.heap_end = KERNEL_HEAP_MAX;
    s_heap.heap_brk = 0; /* Will be set on first allocation */
    s_heap.free_list = NULL;

    /* Allocate initial pages - this will set heap_brk.
     * Note: No need to acquire lock here since we're in single-threaded init. */
    heap_result_t result = expand_heap_locked(HEAP_INIT_PAGES * PAGE_SIZE);
    if (result != HEAP_OK) {
        klog_error("[HEAP] Initial heap expansion failed\n");
        return result;
    }

    /* Update heap_start to match actual allocated address */
    s_heap.heap_start = (virtual_addr_t)s_heap.free_list;

    s_heap.initialized = true;

    klog_info("[HEAP] Heap initialized:\n");
    klog_info("[HEAP]   Region:  0x%llX - 0x%llX\n", s_heap.heap_start, s_heap.heap_end);
    klog_info("[HEAP]   Break:   0x%llX\n", s_heap.heap_brk);
    klog_info("[HEAP]   Size:   %lu KB\n", (s_heap.heap_brk - s_heap.heap_start) / 1024);

    return HEAP_OK;
}

/**
 * kmalloc - Allocate memory from kernel heap
 */
void* kmalloc(size_t size) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&s_heap_lock, &flags);

    if (!s_heap.initialized) {
        klog_error("[HEAP] Not initialized\n");
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return NULL;
    }

    if (size == 0) {
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return NULL;
    }

    /* Calculate total size needed */
    size_t total_size = total_block_size(size);

    /* Find best fit block */
    heap_block_t* block = find_best_fit(total_size);

    if (block == NULL) {
        /* Need to expand heap - heap lock is already held */
        klog_trace("[HEAP] No free block found, expanding heap\n");

        /*
         * IMPORTANT: expand_heap_locked expects the heap lock to be held.
         * We keep the lock held during expansion to prevent race conditions.
         * The VMM lock is acquired internally by vmm_alloc_pages_at.
         */
        heap_result_t result = expand_heap_locked(total_size);
        if (result != HEAP_OK) {
            klog_error("[HEAP] Out of memory\n");
            spin_unlock_irqrestore(&s_heap_lock, flags);
            return NULL;
        }

        /* Try again after expansion (still holding lock) */
        block = find_best_fit(total_size);
        if (block == NULL) {
            klog_error("[HEAP] Still no free block after expansion\n");
            spin_unlock_irqrestore(&s_heap_lock, flags);
            return NULL;
        }
    }

    /* Remove from free list */
    remove_from_free_list(block);

    /* Split if necessary */
    split_block(block, total_size);

    /* Mark as used */
    block->used = true;

    /* Update stats */
    s_heap.stats.used_bytes += block->size;
    s_heap.stats.free_bytes -= block->size;
    s_heap.stats.used_blocks++;
    s_heap.stats.free_blocks--;
    s_heap.stats.alloc_count++;

    void* result = block_to_ptr(block);

    spin_unlock_irqrestore(&s_heap_lock, flags);

    klog_trace("[HEAP] Allocated %lu bytes at 0x%llX (block size: %lu)\n", size,
               (virtual_addr_t)result, block->size);

    return result;
}

/**
 * kfree - Free memory allocated from kernel heap
 */
void kfree(void* ptr) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&s_heap_lock, &flags);

    if (!s_heap.initialized) {
        klog_error("[HEAP] Not initialized\n");
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return;
    }

    if (ptr == NULL) {
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return;
    }

    /* Convert to block pointer */
    heap_block_t* block = ptr_to_block(ptr);

    /* Validate block */
    if (!validate_block(block)) {
        klog_error("[HEAP] Invalid pointer or corrupted block: 0x%llX\n", (virtual_addr_t)ptr);
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return;
    }

    if (!block->used) {
        klog_warn("[HEAP] Double free detected at 0x%llX\n", (virtual_addr_t)ptr);
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return;
    }

    /* Mark as free */
    block->used = false;

    /* Update stats */
    s_heap.stats.used_bytes -= block->size;
    s_heap.stats.free_bytes += block->size;
    s_heap.stats.used_blocks--;
    s_heap.stats.free_blocks++;
    s_heap.stats.free_count++;

    spin_unlock_irqrestore(&s_heap_lock, flags);

    klog_trace("[HEAP] Freed %lu bytes at 0x%llX\n", block->size - sizeof(heap_block_t),
               (virtual_addr_t)ptr);

    /* Re-acquire lock for list operations */
    spin_lock_irqsave(&s_heap_lock, &flags);

    /* Insert to free list */
    insert_to_free_list(block);

    /* Coalesce with adjacent blocks */
    coalesce_block(block);

    spin_unlock_irqrestore(&s_heap_lock, flags);
}

/**
 * krealloc - Reallocate memory with new size
 */
void* krealloc(void* ptr, size_t new_size) {
    /* Handle NULL pointer - equivalent to kmalloc */
    if (ptr == NULL) {
        return kmalloc(new_size);
    }

    /* Handle zero size - equivalent to kfree */
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    spinlock_flags_t flags;
    spin_lock_irqsave(&s_heap_lock, &flags);

    if (!s_heap.initialized) {
        klog_error("[HEAP] Not initialized\n");
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return NULL;
    }

    /* Get current block */
    heap_block_t* old_block = ptr_to_block(ptr);

    if (!validate_block(old_block) || !old_block->used) {
        klog_error("[HEAP] Invalid pointer in krealloc\n");
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return NULL;
    }

    size_t old_user_size = old_block->size - sizeof(heap_block_t);

    /* If new size fits in old block, just return it */
    size_t new_total = total_block_size(new_size);
    if (new_total <= old_block->size) {
        /* Shrink or same size - could split here but skip for simplicity */
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return ptr;
    }

    spin_unlock_irqrestore(&s_heap_lock, flags);

    /* Need to allocate new block */
    void* new_ptr = kmalloc(new_size);
    if (new_ptr == NULL) {
        return NULL;
    }

    /* Copy data - only the smaller of old and new size */
    size_t copy_size = old_user_size < new_size ? old_user_size : new_size;
    memcpy(new_ptr, ptr, copy_size);

    /* Free old block */
    kfree(ptr);

    return new_ptr;
}

/**
 * kmalloc_aligned - Allocate aligned memory
 *
 * Note: Allocated memory must be freed with normal kfree, not a special function.
 * The original pointer is stored just before the returned pointer.
 */
void* kmalloc_aligned(size_t size, size_t alignment) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&s_heap_lock, &flags);

    if (!s_heap.initialized) {
        klog_error("[HEAP] Not initialized\n");
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return NULL;
    }

    if (size == 0) {
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return NULL;
    }

    /* Validate alignment */
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        klog_error("[HEAP] Invalid alignment: %lu\n", alignment);
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return NULL;
    }

    /* Minimum alignment is HEAP_ALIGN */
    if (alignment < HEAP_ALIGN) {
        alignment = HEAP_ALIGN;
    }

    spin_unlock_irqrestore(&s_heap_lock, flags);

    /* Allocate extra space: alignment + sizeof(virtual_addr_t*) for storing original pointer */
    size_t total_size = total_block_size(size) + alignment + sizeof(virtual_addr_t*);

    void* raw_ptr = kmalloc(total_size);
    if (raw_ptr == NULL) {
        return NULL;
    }

    /* Calculate aligned address - ensure we have space for the pointer before it */
    virtual_addr_t raw_start = (virtual_addr_t)raw_ptr;
    virtual_addr_t aligned_addr = align_up(raw_start + sizeof(virtual_addr_t*), alignment);

    /* Store original pointer just before aligned address */
    virtual_addr_t* orig_ptr_loc = (virtual_addr_t*)(aligned_addr - sizeof(virtual_addr_t*));
    *orig_ptr_loc = raw_start;

    klog_trace("[HEAP] Allocated %lu bytes at 0x%llX (block size: %lu)\n", size,
               (virtual_addr_t)raw_ptr, total_block_size(total_size));
    klog_trace("[HEAP] Aligned allocation: raw=0x%llX, aligned=0x%llX\n", raw_start, aligned_addr);

    return (void*)aligned_addr;
}

/**
 * heap_get_stats - Get heap statistics
 */
heap_result_t heap_get_stats(heap_stats_t* stats) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&s_heap_lock, &flags);

    if (!s_heap.initialized) {
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return HEAP_ERR_NOT_INIT;
    }

    if (stats == NULL) {
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return HEAP_ERR_INVALID;
    }

    /* Copy basic cached statistics */
    stats->alloc_count = s_heap.stats.alloc_count;
    stats->free_count = s_heap.stats.free_count;
    stats->expand_count = s_heap.stats.expand_count;

    /* Recalculate accurate counts by walking the heap */
    stats->total_blocks = 0;
    stats->used_blocks = 0;
    stats->free_blocks = 0;
    stats->used_bytes = 0;
    stats->free_bytes = 0;
    stats->total_bytes = 0;

    heap_block_t* block = (heap_block_t*)s_heap.heap_start;
    while ((virtual_addr_t)block < s_heap.heap_brk) {
        if (validate_block(block)) {
            size_t user_bytes = block->size - sizeof(heap_block_t);
            stats->total_blocks++;
            stats->total_bytes += user_bytes; /* Only count user-available bytes */

            if (block->used) {
                stats->used_blocks++;
                stats->used_bytes += user_bytes;
            } else {
                stats->free_blocks++;
                stats->free_bytes += user_bytes;
            }
        }
        block = (heap_block_t*)((virtual_addr_t)block + block->size);
    }

    spin_unlock_irqrestore(&s_heap_lock, flags);

    return HEAP_OK;
}

/**
 * heap_dump - Dump heap state for debugging
 */
void heap_dump(void) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&s_heap_lock, &flags);

    if (!s_heap.initialized) {
        spin_unlock_irqrestore(&s_heap_lock, flags);
        klog_error("[HEAP] Not initialized\n");
        return;
    }

    klog_info("[HEAP] Heap State:\n");
    klog_info("[HEAP]   Region:  0x%llX - 0x%llX\n", s_heap.heap_start, s_heap.heap_end);
    klog_info("[HEAP]   Break:   0x%llX\n", s_heap.heap_brk);
    klog_info("[HEAP]   Total:   %llu bytes (%llu MB)\n", s_heap.stats.total_bytes,
              s_heap.stats.total_bytes / (1024 * 1024));
    klog_info("[HEAP]   Used:    %llu bytes (%.1f%%)\n", s_heap.stats.used_bytes,
              s_heap.stats.total_bytes > 0
                  ? (100.0 * s_heap.stats.used_bytes / s_heap.stats.total_bytes)
                  : 0.0);
    klog_info("[HEAP]   Free:    %llu bytes\n", s_heap.stats.free_bytes);
    klog_info("[HEAP]   Blocks:  %u used, %u free\n", s_heap.stats.used_blocks,
              s_heap.stats.free_blocks);
    klog_info("[HEAP]   Ops:     %lu allocs, %lu frees, %lu expansions\n", s_heap.stats.alloc_count,
              s_heap.stats.free_count, s_heap.stats.expand_count);

    /* Dump free list */
    klog_info("[HEAP] Free List:\n");
    heap_block_t* curr = s_heap.free_list;
    int count = 0;
    while (curr && count < 16) { /* Limit to first 16 entries */
        klog_info("[HEAP]   [%d] 0x%llX: %llu bytes\n", count++, (virtual_addr_t)curr, curr->size);
        curr = curr->next;
    }
    if (curr) {
        klog_info("[HEAP]   ... and more\n");
    }

    spin_unlock_irqrestore(&s_heap_lock, flags);
}
