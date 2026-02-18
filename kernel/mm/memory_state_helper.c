/* ==============================================================================
 * CCOS - Memory State Helper Functions Implementation
 * ==============================================================================
 */

#include "memory_state_helper.h"
#include "base/memory.h"
#include "base/string.h"
#include "klogs/kprintf.h"
#include "klogs/ksnprintf.h"

/* ==============================================================================
 * Constants
 * ============================================================================== */
#define KB_SHIFT 10
#define MB_SHIFT 20
#define GB_SHIFT 30
#define TB_SHIFT 40
#define PAGE_SHIFT 12
#define PAGE_SIZE (1ULL << PAGE_SHIFT) /* 4096 bytes */

/* ==============================================================================
 * Size Conversion Functions
 * ============================================================================== */

uint32_t bytes_to_mb(uint64_t bytes) {
    return (uint32_t)(bytes >> MB_SHIFT);
}

uint64_t bytes_to_kb(uint64_t bytes) {
    return bytes >> KB_SHIFT;
}

uint64_t mb_to_bytes(uint32_t mb) {
    return ((uint64_t)mb) << MB_SHIFT;
}

uint64_t kb_to_bytes(uint64_t kb) {
    return kb << KB_SHIFT;
}

uint64_t pages_to_bytes(uint64_t pages) {
    return pages << PAGE_SHIFT;
}

uint64_t bytes_to_pages(uint64_t bytes) {
    /* Round up to next page boundary */
    return (bytes + PAGE_SIZE - 1) >> PAGE_SHIFT;
}

uint64_t align_up(uint64_t value, uint32_t alignment) {
    uint64_t mask = (1ULL << alignment) - 1;
    return (value + mask) & ~mask;
}

uint64_t align_down(uint64_t value, uint32_t alignment) {
    uint64_t mask = (1ULL << alignment) - 1;
    return value & ~mask;
}

bool is_aligned(uint64_t value, uint32_t alignment) {
    uint64_t mask = (1ULL << alignment) - 1;
    return (value & mask) == 0;
}

/* ==============================================================================
 * Size Formatting Functions
 * ============================================================================== */

const char* format_size(uint64_t size, char* buf, size_t buf_len) {
    if (buf == NULL || buf_len == 0) {
        return "";
    }

    if (size >= (1ULL << TB_SHIFT)) {
        ksnprintf(buf, buf_len, "%llu TB", size >> TB_SHIFT);
    } else if (size >= (1ULL << GB_SHIFT)) {
        ksnprintf(buf, buf_len, "%llu GB", size >> GB_SHIFT);
    } else if (size >= (1ULL << MB_SHIFT)) {
        ksnprintf(buf, buf_len, "%llu MB", size >> MB_SHIFT);
    } else if (size >= (1ULL << KB_SHIFT)) {
        ksnprintf(buf, buf_len, "%llu KB", size >> KB_SHIFT);
    } else {
        ksnprintf(buf, buf_len, "%llu B", size);
    }

    return buf;
}

const char* format_size_ex(uint64_t size, formatted_size_t* fs) {
    if (fs == NULL) {
        return "";
    }

    fs->value = size;

    if (size >= (1ULL << TB_SHIFT)) {
        fs->unit = SIZE_UNIT_TB;
        fs->value = size >> TB_SHIFT;
    } else if (size >= (1ULL << GB_SHIFT)) {
        fs->unit = SIZE_UNIT_GB;
        fs->value = size >> GB_SHIFT;
    } else if (size >= (1ULL << MB_SHIFT)) {
        fs->unit = SIZE_UNIT_MB;
        fs->value = size >> MB_SHIFT;
    } else if (size >= (1ULL << KB_SHIFT)) {
        fs->unit = SIZE_UNIT_KB;
        fs->value = size >> KB_SHIFT;
    } else {
        fs->unit = SIZE_UNIT_BYTES;
        fs->value = size;
    }

    /* Generate string */
    switch (fs->unit) {
        case SIZE_UNIT_TB:
            ksnprintf(fs->string, sizeof(fs->string), "%llu TB", fs->value);
            break;
        case SIZE_UNIT_GB:
            ksnprintf(fs->string, sizeof(fs->string), "%llu GB", fs->value);
            break;
        case SIZE_UNIT_MB:
            ksnprintf(fs->string, sizeof(fs->string), "%llu MB", fs->value);
            break;
        case SIZE_UNIT_KB:
            ksnprintf(fs->string, sizeof(fs->string), "%llu KB", fs->value);
            break;
        case SIZE_UNIT_BYTES:
            ksnprintf(fs->string, sizeof(fs->string), "%llu B", fs->value);
            break;
    }

    return fs->string;
}

const char* format_size_unit(uint64_t size, size_unit_t unit, char* buf, size_t buf_len) {
    if (buf == NULL || buf_len == 0) {
        return "";
    }

    const char* unit_str;
    uint64_t value;

    switch (unit) {
        case SIZE_UNIT_TB:
            unit_str = "TB";
            value = size >> TB_SHIFT;
            break;
        case SIZE_UNIT_GB:
            unit_str = "GB";
            value = size >> GB_SHIFT;
            break;
        case SIZE_UNIT_MB:
            unit_str = "MB";
            value = size >> MB_SHIFT;
            break;
        case SIZE_UNIT_KB:
            unit_str = "KB";
            value = size >> KB_SHIFT;
            break;
        case SIZE_UNIT_BYTES:
        default:
            unit_str = "B";
            value = size;
            break;
    }

    ksnprintf(buf, buf_len, "%llu %s", value, unit_str);
    return buf;
}

/* ==============================================================================
 * Memory Summary Functions
 * ============================================================================== */

void mem_summary_init(memory_summary_t* summary) {
    if (summary == NULL) {
        return;
    }

    memset(summary, 0, sizeof(memory_summary_t));
}

void mem_summary_add_region(memory_summary_t* summary, uint64_t base, uint64_t length,
                            bool is_usable) {
    (void)base;  /* Reserved for future use */
    if (summary == NULL) {
        return;
    }

    summary->total_bytes += length;

    if (is_usable) {
        summary->usable_bytes += length;
    } else {
        summary->reserved_bytes += length;
    }
}

void mem_summary_calculate_mb(memory_summary_t* summary) {
    if (summary == NULL) {
        return;
    }

    summary->total_mb = bytes_to_mb(summary->total_bytes);
    summary->usable_mb = bytes_to_mb(summary->usable_bytes);
}

void mem_summary_calculate_pages(memory_summary_t* summary) {
    if (summary == NULL) {
        return;
    }

    summary->total_pages = bytes_to_pages(summary->total_bytes);
    summary->usable_pages = bytes_to_pages(summary->usable_bytes);
}

void mem_summary_dump(const memory_summary_t* summary) {
    if (summary == NULL) {
        klog_error("[MEM] Invalid summary pointer");
        return;
    }

    char total_buf[32], usable_buf[32], reserved_buf[32];

    format_size(summary->total_bytes, total_buf, sizeof(total_buf));
    format_size(summary->usable_bytes, usable_buf, sizeof(usable_buf));
    format_size(summary->reserved_bytes, reserved_buf, sizeof(reserved_buf));

    klog_info("[MEM] Summary: Total=%s (%u MB), Usable=%s (%u MB), Reserved=%s", total_buf,
              summary->total_mb, usable_buf, summary->usable_mb, reserved_buf);

    if (summary->total_pages > 0) {
        klog_info("[MEM] Pages: Total=%llu, Usable=%llu", summary->total_pages,
                  summary->usable_pages);
    }
}

/* ==============================================================================
 * Address Range Functions
 * ============================================================================== */

bool ranges_overlap(uint64_t start1, uint64_t end1, uint64_t start2, uint64_t end2) {
    /* No overlap if one range ends before the other starts */
    if (end1 <= start2 || end2 <= start1) {
        return false;
    }
    return true;
}

bool range_contains(uint64_t outer_start, uint64_t outer_end, uint64_t inner_start,
                    uint64_t inner_end) {
    /* Outer contains inner if outer starts at or before inner,
     * and outer ends at or after inner */
    return (outer_start <= inner_start) && (outer_end >= inner_end);
}
