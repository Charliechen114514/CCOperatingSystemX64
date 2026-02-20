/* ==============================================================================
 * CCOS - Physical Frame Allocator Implementation
 * ==============================================================================
 */
#include "pframe.h"
#include "assert/assert.h"
#include "base/memory.h"
#include "bitmap/bitmap.h"
#include "klogs/kprintf.h"
#include "klogs/ksnprintf.h"
#include "memory_detect/e820.h"
#include "memory_detect/memory_state_helper.h"
#include "sync/spinlock.h"

/* ==============================================================================
 * Constants
 * ============================================================================== */

/* Maximum bitmap size for 64GB RAM = 2MB */
#define PFRAME_MAX_BITMAP_SIZE (2 * 1024 * 1024)

/* Reserved memory regions */
#define PFRAME_BIOS_END 0x100000                 /* 1MB - end of BIOS/reserved area */
#define PFRAME_E820_STORAGE_ADDR 0x6000          /* E820 storage */
#define PFRAME_PAGE_TABLE_START 0x9000           /* Page tables start */
#define PFRAME_PAGE_TABLE_END 0xBFFF             /* Page tables end */
#define PFRAME_KERNEL_LOAD_ADDR 0x10000          /* Kernel load address */
#define PFRAME_KERNEL_MAX_SIZE (1 * 1024 * 1024) /* 1MB max kernel size (conservative) */

/* ==============================================================================
 * Internal State
 * ============================================================================== */

static byte_t s_frame_bitmap_storage[PFRAME_MAX_BITMAP_SIZE] __attribute__((section(".lbss")));

/* Frame allocator state */
typedef struct {
    bitmap frame_map;         /* Bitmap tracking frame allocation */
    uint64_t total_frames;    /* Total number of frames managed */
    uint64_t managed_base;    /* Base address of managed region */
    uint64_t managed_end;     /* End address of managed region */
    uint64_t last_alloc_hint; /* Hint for allocation locality */
    bool initialized;
} pframe_state_t;

static pframe_state_t s_pframe_state = {0};

/* Spinlock protecting the frame allocator state */
static spinlock_t s_pframe_lock = SPIN_LOCK_INIT;

/* ==============================================================================
 * Internal Helper Functions
 * ============================================================================== */

/**
 * addr_to_frame - Convert physical address to frame index
 */
static inline uint64_t addr_to_frame(physical_addr_t addr) {
    return addr >> PAGE_SHIFT;
}

/**
 * frame_to_addr - Convert frame index to physical address
 */
static inline physical_addr_t frame_to_addr(uint64_t frame) {
    return frame << PAGE_SHIFT;
}

/**
 * is_page_aligned - Check if address is page-aligned
 */
static inline bool is_page_aligned(physical_addr_t addr) {
    return (addr & (PAGE_SIZE - 1)) == 0;
}

/**
 * mark_region_reserved - Mark a memory region as reserved (allocated)
 */
static void mark_region_reserved(uint64_t base, uint64_t length) {
    if (length == 0) {
        return;
    }

    /* Align to page boundaries */
    uint64_t start_frame = addr_to_frame((physical_addr_t)align_up(base, PAGE_SHIFT));
    uint64_t end_frame = addr_to_frame((physical_addr_t)align_down(base + length, PAGE_SHIFT));

    /* Clamp to managed range */
    if (start_frame < s_pframe_state.total_frames) {
        if (end_frame > s_pframe_state.total_frames) {
            end_frame = s_pframe_state.total_frames;
        }

        uint64_t frame_count = end_frame - start_frame;
        bitmap_set_range(&s_pframe_state.frame_map, start_frame, frame_count);

        klog_trace("[PFRAME] Reserved region: 0x%llX - 0x%llX (%llu frames)\n", base, base + length,
                   (unsigned long long)frame_count);
    }
}

/**
 * mark_region_free - Mark a memory region as free
 */
static void mark_region_free(uint64_t base, uint64_t length) {
    if (length == 0) {
        return;
    }

    /* Align to page boundaries */
    uint64_t start_frame = addr_to_frame((physical_addr_t)align_up(base, PAGE_SHIFT));
    uint64_t end_frame = addr_to_frame((physical_addr_t)align_down(base + length, PAGE_SHIFT));

    /* Clamp to managed range */
    if (start_frame < s_pframe_state.total_frames) {
        if (end_frame > s_pframe_state.total_frames) {
            end_frame = s_pframe_state.total_frames;
        }

        uint64_t frame_count = end_frame - start_frame;
        bitmap_clear_range(&s_pframe_state.frame_map, start_frame, frame_count);
    }
}

/* ==============================================================================
 * API Implementation
 * ============================================================================== */

/**
 * pframe_init - Initialize the physical frame allocator
 */
pframe_result_t pframe_init(void) {
    if (s_pframe_state.initialized) {
        klog_warn("[PFRAME] Already initialized\n");
        return PFRAME_OK;
    }

    /* Get E820 statistics to determine memory size */
    mem_stats_t e820_stats;
    e820_get_stats(&e820_stats);

    if (e820_stats.usable_bytes == 0) {
        klog_error("[PFRAME] No usable memory found\n");
        return PFRAME_ERR_INVALID;
    }

    /* Calculate total frames based on highest USABLE memory address */
    /* We only manage usable memory, not reserved regions */
    uint64_t max_addr = 0;
    uint32_t entry_count = e820_get_entry_count();

    /* Find the highest USABLE memory address */
    for (uint32_t i = 0; i < entry_count; i++) {
        e820_entry_t entry;
        if (e820_get_entry(i, &entry)) {
            uint64_t entry_end = entry.base + entry.length;
            /* Only consider USABLE memory for determining max managed address */
            if (entry.type == E820_TYPE_USABLE && entry_end > max_addr) {
                max_addr = entry_end;
            }
        }
    }

    /* Align max address down to page boundary */
    max_addr = align_down(max_addr, PAGE_SHIFT);

    klog_trace("[PFRAME] Highest usable address: 0x%llX\n", (unsigned long long)max_addr);

    /* Calculate total frames - this represents the entire addressable space */
    s_pframe_state.total_frames = bytes_to_pages(max_addr);
    s_pframe_state.managed_base = PFRAME_MIN_ALLOC_BASE;
    s_pframe_state.managed_end = max_addr;
    s_pframe_state.last_alloc_hint = addr_to_frame(PFRAME_MIN_ALLOC_BASE);
    s_pframe_state.initialized = false; /* Will set to true at end */

    /* Calculate bitmap size needed */
    size_t bitmap_bytes = (s_pframe_state.total_frames + 7) / 8;

    if (bitmap_bytes > PFRAME_MAX_BITMAP_SIZE) {
        klog_error("[PFRAME] Bitmap size %lu exceeds maximum %u\n", (unsigned long)bitmap_bytes,
                   PFRAME_MAX_BITMAP_SIZE);
        return PFRAME_ERR_INVALID;
    }

    /* Initialize bitmap with static storage */
    bitmap_init(&s_pframe_state.frame_map, s_frame_bitmap_storage, s_pframe_state.total_frames);

    /* Initially mark ALL frames as allocated (will free usable regions) */
    bitmap_set_range(&s_pframe_state.frame_map, 0, s_pframe_state.total_frames);

    /* Mark all E820 usable regions as free */
    for (uint32_t i = 0; i < entry_count; i++) {
        e820_entry_t entry;
        if (e820_get_entry(i, &entry)) {
            if (entry.type == E820_TYPE_USABLE) {
                mark_region_free(entry.base, entry.length);
            }
        }
    }

    /* Mark reserved regions as allocated */
    /* 1. BIOS area below 1MB */
    mark_region_reserved(0, PFRAME_BIOS_END);

    /* 2. E820 storage */
    mark_region_reserved(PFRAME_E820_STORAGE_ADDR, PAGE_SIZE);

    /* 3. Page tables */
    mark_region_reserved(PFRAME_PAGE_TABLE_START,
                         PFRAME_PAGE_TABLE_END - PFRAME_PAGE_TABLE_START + 1);

    /* 4. Kernel code/data */
    mark_region_reserved(PFRAME_KERNEL_LOAD_ADDR, PFRAME_KERNEL_MAX_SIZE);

    /* 5. Bitmap storage itself */
    mark_region_reserved((uint64_t)s_frame_bitmap_storage, PFRAME_MAX_BITMAP_SIZE);

    s_pframe_state.initialized = true;

    /* Print initialization summary */
    pframe_stats_t stats = {0};
    pframe_get_stats(&stats);

    klog_info(
        "[PFRAME] Initialized: %llu frames (%llu MB managed)\n",
        (unsigned long long)stats.total_frames,
        (unsigned long long)((stats.managed_end - stats.managed_start) / (1024ULL * 1024ULL)));
    klog_info("[PFRAME] Free: %llu, Reserved: %llu, Allocated: %llu\n",
              (unsigned long long)stats.free_frames, (unsigned long long)stats.reserved_frames,
              (unsigned long long)stats.allocated_frames);

    return PFRAME_OK;
}

/**
 * pframe_alloc - Allocate a single physical frame
 */
pframe_result_t pframe_alloc(physical_addr_t* out_addr) {
    CCOS_ASSERT(out_addr != NULL);

    spinlock_flags_t flags;
    spin_lock_irqsave(&s_pframe_lock, &flags);

    if (!s_pframe_state.initialized) {
        klog_error("[PFRAME] Allocator not initialized\n");
        spin_unlock_irqrestore(&s_pframe_lock, flags);
        return PFRAME_ERR_NOT_INIT;
    }

    /* Search for first free frame starting from hint */
    ssize_t frame_idx =
        bitmap_find_next_zero(&s_pframe_state.frame_map, s_pframe_state.last_alloc_hint);

    /* If not found from hint, try from beginning */
    if (frame_idx < 0) {
        frame_idx = bitmap_find_first_zero(&s_pframe_state.frame_map);
    }

    if (frame_idx < 0) {
        klog_warn("[PFRAME] Out of memory: no free frames\n");
        spin_unlock_irqrestore(&s_pframe_lock, flags);
        return PFRAME_ERR_OOM;
    }

    /* Mark frame as allocated */
    bitmap_set(&s_pframe_state.frame_map, frame_idx);

    /* Update hint for next allocation (locality) */
    s_pframe_state.last_alloc_hint = frame_idx + 1;
    if (s_pframe_state.last_alloc_hint >= s_pframe_state.total_frames) {
        s_pframe_state.last_alloc_hint = addr_to_frame(PFRAME_MIN_ALLOC_BASE);
    }

    *out_addr = frame_to_addr(frame_idx);

    spin_unlock_irqrestore(&s_pframe_lock, flags);

    return PFRAME_OK;
}

/**
 * pframe_alloc_n - Allocate multiple contiguous physical frames
 */
pframe_result_t pframe_alloc_n(physical_addr_t* out_addr, uint64_t frame_count) {
    CCOS_ASSERT(out_addr != NULL);

    spinlock_flags_t flags;
    spin_lock_irqsave(&s_pframe_lock, &flags);

    if (!s_pframe_state.initialized) {
        klog_error("[PFRAME] Allocator not initialized\n");
        spin_unlock_irqrestore(&s_pframe_lock, flags);
        return PFRAME_ERR_NOT_INIT;
    }

    if (frame_count == 0) {
        spin_unlock_irqrestore(&s_pframe_lock, flags);
        return PFRAME_ERR_INVALID;
    }

    if (frame_count > s_pframe_state.total_frames) {
        spin_unlock_irqrestore(&s_pframe_lock, flags);
        return PFRAME_ERR_OOM;
    }

    /* Search for consecutive free frames */
    uint64_t start_frame = s_pframe_state.last_alloc_hint;
    bool found = false;
    uint64_t run_start = 0;
    uint64_t run_length = 0;

    /* Search from hint to end */
    for (uint64_t i = start_frame; i < s_pframe_state.total_frames;) {
        if (!bitmap_test(&s_pframe_state.frame_map, i)) {
            /* Found a free frame */
            if (run_length == 0) {
                run_start = i;
            }
            run_length++;

            if (run_length >= frame_count) {
                found = true;
                break;
            }

            i++;
        } else {
            /* Frame is allocated, reset run */
            run_length = 0;
            i++;
        }
    }

    /* If not found, search from beginning to hint */
    if (!found) {
        run_length = 0;
        for (uint64_t i = addr_to_frame(PFRAME_MIN_ALLOC_BASE);
             i < start_frame && i < s_pframe_state.total_frames;) {

            if (!bitmap_test(&s_pframe_state.frame_map, i)) {
                if (run_length == 0) {
                    run_start = i;
                }
                run_length++;

                if (run_length >= frame_count) {
                    found = true;
                    break;
                }

                i++;
            } else {
                run_length = 0;
                i++;
            }
        }
    }

    if (!found) {
        klog_warn("[PFRAME] Out of memory: cannot allocate %llu contiguous frames\n",
                  (unsigned long long)frame_count);
        spin_unlock_irqrestore(&s_pframe_lock, flags);
        return PFRAME_ERR_OOM;
    }

    /* Mark frames as allocated */
    bitmap_set_range(&s_pframe_state.frame_map, run_start, frame_count);

    /* Update hint */
    s_pframe_state.last_alloc_hint = run_start + frame_count;
    if (s_pframe_state.last_alloc_hint >= s_pframe_state.total_frames) {
        s_pframe_state.last_alloc_hint = addr_to_frame(PFRAME_MIN_ALLOC_BASE);
    }

    *out_addr = frame_to_addr(run_start);

    spin_unlock_irqrestore(&s_pframe_lock, flags);

    return PFRAME_OK;
}

/**
 * pframe_free - Free a single physical frame
 */
pframe_result_t pframe_free(physical_addr_t addr) {
    spin_lock(&s_pframe_lock);

    if (!s_pframe_state.initialized) {
        klog_error("[PFRAME] Allocator not initialized\n");
        spin_unlock(&s_pframe_lock);
        return PFRAME_ERR_NOT_INIT;
    }

    if (!is_page_aligned(addr)) {
        klog_error("[PFRAME] Invalid address to free: not page aligned (0x%X)\n", addr);
        spin_unlock(&s_pframe_lock);
        return PFRAME_ERR_INVALID;
    }

    uint64_t frame = addr_to_frame(addr);

    if (frame >= s_pframe_state.total_frames) {
        klog_error("[PFRAME] Invalid address to free: out of range (0x%X)\n", addr);
        spin_unlock(&s_pframe_lock);
        return PFRAME_ERR_INVALID;
    }

    /* Check for double-free */
    if (!bitmap_test(&s_pframe_state.frame_map, frame)) {
        klog_warn("[PFRAME] Double-free detected: 0x%X\n", addr);
        spin_unlock(&s_pframe_lock);
        return PFRAME_ERR_INVALID;
    }

    bitmap_clear(&s_pframe_state.frame_map, frame);

    spin_unlock(&s_pframe_lock);

    return PFRAME_OK;
}

/**
 * pframe_free_n - Free multiple contiguous physical frames
 */
pframe_result_t pframe_free_n(physical_addr_t addr, uint64_t frame_count) {
    spin_lock(&s_pframe_lock);

    if (!s_pframe_state.initialized) {
        klog_error("[PFRAME] Allocator not initialized\n");
        spin_unlock(&s_pframe_lock);
        return PFRAME_ERR_NOT_INIT;
    }

    if (frame_count == 0) {
        spin_unlock(&s_pframe_lock);
        return PFRAME_ERR_INVALID;
    }

    if (!is_page_aligned(addr)) {
        klog_error("[PFRAME] Invalid address to free: not page aligned (0x%X)\n", addr);
        spin_unlock(&s_pframe_lock);
        return PFRAME_ERR_INVALID;
    }

    uint64_t start_frame = addr_to_frame(addr);

    if (start_frame >= s_pframe_state.total_frames) {
        klog_error("[PFRAME] Invalid address to free: out of range (0x%X)\n", addr);
        spin_unlock(&s_pframe_lock);
        return PFRAME_ERR_INVALID;
    }

    if (start_frame + frame_count > s_pframe_state.total_frames) {
        klog_error("[PFRAME] Free range exceeds managed memory\n");
        spin_unlock(&s_pframe_lock);
        return PFRAME_ERR_INVALID;
    }

    /* Clear the range */
    bitmap_clear_range(&s_pframe_state.frame_map, start_frame, frame_count);

    spin_unlock(&s_pframe_lock);

    return PFRAME_OK;
}

/**
 * pframe_get_stats - Get frame allocator statistics
 */
pframe_result_t pframe_get_stats(pframe_stats_t* stats) {
    spin_lock(&s_pframe_lock);

    if (!s_pframe_state.initialized) {
        spin_unlock(&s_pframe_lock);
        return PFRAME_ERR_NOT_INIT;
    }

    if (stats == NULL) {
        spin_unlock(&s_pframe_lock);
        return PFRAME_ERR_INVALID;
    }

    stats->total_frames = s_pframe_state.total_frames;
    stats->managed_start = s_pframe_state.managed_base;
    stats->managed_end = s_pframe_state.managed_end;
    stats->bitmap_size_bytes = (s_pframe_state.total_frames + 7) / 8;

    /* Count allocated and free frames using bitmap weight */
    size_t allocated = bitmap_weight(&s_pframe_state.frame_map);
    stats->allocated_frames = allocated;
    stats->free_frames = s_pframe_state.total_frames - allocated;

    /* Reserved frames are those below min alloc base (< 1MB) */
    uint64_t min_alloc_frame = addr_to_frame(PFRAME_MIN_ALLOC_BASE);
    stats->reserved_frames = min_alloc_frame;

    spin_unlock(&s_pframe_lock);

    return PFRAME_OK;
}

/**
 * pframe_dump - Dump frame allocator state for debugging
 */
void pframe_dump(void) {
    if (!s_pframe_state.initialized) {
        klog_error("[PFRAME] Allocator not initialized\n");
        return;
    }

    pframe_stats_t stats = {0};
    pframe_get_stats(&stats);

    klog_info("[PFRAME] Allocator State:\n");
    klog_info("[PFRAME]   Total frames:    %llu (%llu MB)\n",
              (unsigned long long)stats.total_frames,
              (unsigned long long)(pages_to_bytes(stats.total_frames) / (1024 * 1024)));
    klog_info("[PFRAME]   Free frames:     %llu\n", (unsigned long long)stats.free_frames);
    klog_info("[PFRAME]   Allocated:       %llu\n", (unsigned long long)stats.allocated_frames);
    klog_info("[PFRAME]   Reserved:        %llu\n", (unsigned long long)stats.reserved_frames);
    klog_info("[PFRAME]   Bitmap size:     %llu bytes\n",
              (unsigned long long)stats.bitmap_size_bytes);
    klog_info("[PFRAME]   Managed range:   0x%llX - 0x%llX\n",
              (unsigned long long)stats.managed_start, (unsigned long long)stats.managed_end);
}

/**
 * pframe_is_allocated - Check if a physical address is currently allocated
 */
bool pframe_is_allocated(physical_addr_t addr) {
    spin_lock(&s_pframe_lock);

    if (!s_pframe_state.initialized) {
        spin_unlock(&s_pframe_lock);
        return false;
    }

    uint64_t frame = addr_to_frame(addr);

    if (frame >= s_pframe_state.total_frames) {
        spin_unlock(&s_pframe_lock);
        return false;
    }

    bool result = bitmap_test(&s_pframe_state.frame_map, frame);

    spin_unlock(&s_pframe_lock);

    return result;
}

/**
 * pframe_get_total_frames - Get total number of frames managed
 */
uint64_t pframe_get_total_frames(void) {
    spin_lock(&s_pframe_lock);
    bool initialized = s_pframe_state.initialized;
    uint64_t total = s_pframe_state.total_frames;
    spin_unlock(&s_pframe_lock);

    if (!initialized) {
        return 0;
    }
    return total;
}

/**
 * pframe_get_free_frames - Get number of free frames
 */
uint64_t pframe_get_free_frames(void) {
    if (!s_pframe_state.initialized) {
        return 0;
    }

    pframe_stats_t stats = {0};
    pframe_get_stats(&stats);
    return stats.free_frames;
}