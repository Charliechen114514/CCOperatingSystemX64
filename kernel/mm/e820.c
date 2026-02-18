/* ==============================================================================
 * CCOS - E820 Memory Map Parser Implementation
 * ==============================================================================
 */

#include "e820.h"
#include "base/memory.h"
#include "klogs/kprintf.h"
#include "klogs/ksnprintf.h"
#include "memory_state_helper.h"

/* ==============================================================================
 * Internal State
 * ============================================================================== */

/* Memory map storage - cached from bootloader */
static e820_entry_t s_e820_entries[E820_MAX_ENTRIES];
static uint32_t s_e820_entry_count = 0;
static mem_detect_method_t s_detect_method = MEM_DETECT_UNKNOWN;
static bool s_initialized = false;

/* ==============================================================================
 * API Implementation
 * ============================================================================== */

void e820_init(void) {
    if (s_initialized) {
        klog_warn("[E820] Already initialized\n");
        return;
    }

    /* Pointer to bootloader's memory map storage */
    volatile uint8_t* mem_map_ptr = (volatile uint8_t*)E820_STORAGE_ADDR;
    /* Read detection method (first byte) */
    uint8_t method_byte = mem_map_ptr[0];
    s_detect_method = (mem_detect_method_t)method_byte;

    /* Read entry count (bytes 1-2, little endian 16-bit) */
    uint8_t byte1 = mem_map_ptr[1];
    uint8_t byte2 = mem_map_ptr[2];
    s_e820_entry_count = ((uint32_t)byte2 << 8) | (uint32_t)byte1;

    /* Validate entry count */
    if (s_e820_entry_count > E820_MAX_ENTRIES) {
        klog_error("[E820] Invalid entry count: %u (max: %u)\n", s_e820_entry_count,
                   E820_MAX_ENTRIES);
        s_e820_entry_count = 0;
        s_detect_method = MEM_DETECT_UNKNOWN;
        return;
    }

    /* Copy entries from bootloader storage */
    /* Layout: [method (1 byte)][entry_count (2 bytes)][entries (24 bytes each)] */
    /* Entries start at offset 3 */
    volatile e820_entry_t* src_entries = (volatile e820_entry_t*)(mem_map_ptr + 3);

    /* Debug: Print last few entries' raw bytes */
    if (s_e820_entry_count > 0) {
        volatile uint8_t* raw_bytes = (volatile uint8_t*)src_entries;
        uint32_t last_idx = s_e820_entry_count - 1;

        klog_trace("[E820] Entry %u raw bytes (first 24):\n", (unsigned int)last_idx);
        volatile uint8_t* entry_bytes = raw_bytes + (last_idx * 24);
        for (int b = 0; b < 24; b++) {
            klog_trace("%02X ", entry_bytes[b]);
            if ((b & 7) == 7)
                klog_trace("\n");
        }
        klog_trace("\n");
    }

    for (uint32_t i = 0; i < s_e820_entry_count; i++) {
        /* Copy entry data */
        s_e820_entries[i].base = src_entries[i].base;
        s_e820_entries[i].length = src_entries[i].length;
        s_e820_entries[i].type = src_entries[i].type;
        s_e820_entries[i].acpi_attrs = src_entries[i].acpi_attrs;

        if (s_e820_entries[i].length == 0) {
            klog_warn("[E820] Entry %u: length is 0, skipping\n", (unsigned int)i);
            s_e820_entries[i].type = 0;
            continue;
        }
    }

    s_initialized = true;

    klog_trace("[E820] Initialized: method=%s, entries=%u\n",
               e820_get_detect_method_name(s_detect_method), s_e820_entry_count);
}

mem_detect_method_t e820_get_detect_method(void) {
    return s_detect_method;
}

const char* e820_get_detect_method_name(mem_detect_method_t method) {
    switch (method) {
        case MEM_DETECT_E820:
            return "E820";
        case MEM_DETECT_E801:
            return "E801";
        case MEM_DETECT_88H:
            return "INT 15h/88h";
        case MEM_DETECT_UNKNOWN:
        default:
            return "Unknown";
    }
}

uint32_t e820_get_entry_count(void) {
    return s_e820_entry_count;
}

bool e820_get_entry(uint32_t index, e820_entry_t* entry) {
    if (!s_initialized) {
        klog_error("[E820] Not initialized");
        return false;
    }

    if (entry == NULL) {
        return false;
    }

    if (index >= s_e820_entry_count) {
        return false;
    }

    entry->base = s_e820_entries[index].base;
    entry->length = s_e820_entries[index].length;
    entry->type = s_e820_entries[index].type;
    entry->acpi_attrs = s_e820_entries[index].acpi_attrs;

    return true;
}

void e820_get_stats(mem_stats_t* stats) {
    if (!s_initialized || stats == NULL) {
        return;
    }

    /* Initialize stats */
    memset(stats, 0, sizeof(mem_stats_t));
    stats->entry_count = s_e820_entry_count;

    /* Calculate statistics */
    for (uint32_t i = 0; i < s_e820_entry_count; i++) {
        const e820_entry_t* entry = &s_e820_entries[i];

        stats->total_bytes += entry->length;

        switch (entry->type) {
            case E820_TYPE_USABLE:
                stats->usable_bytes += entry->length;
                break;
            case E820_TYPE_RESERVED:
            case E820_TYPE_UNUSABLE:
                stats->reserved_bytes += entry->length;
                break;
            /* ACPI reclaimable and NVS counted as reserved for safety */
            case E820_TYPE_ACPI_RECLAIM:
            case E820_TYPE_NVS:
                stats->reserved_bytes += entry->length;
                break;
            default:
                break;
        }
    }

    /* Convert to MB using helper */
    stats->total_mb = bytes_to_mb(stats->total_bytes);
    stats->usable_mb = bytes_to_mb(stats->usable_bytes);
}

void e820_dump_map(void) {
    if (!s_initialized) {
        klog_error("[E820] Not initialized");
        return;
    }

    klog_info("[E820] Memory Map (method: %s, entries: %u)\n",
              e820_get_detect_method_name(s_detect_method), s_e820_entry_count);
    klog_info("[E820] %-18s %-18s %-12s %s\n", "Base", "Length", "Type", "Description");

    for (uint32_t i = 0; i < s_e820_entry_count; i++) {
        const e820_entry_t* entry = &s_e820_entries[i];
        char size_buf[32];
        char base_buf[32];

        format_size(entry->length, size_buf, sizeof(size_buf));
        ksnprintf(base_buf, sizeof(base_buf), "0x%016llX", entry->base);

        klog_info("[E820] %s  %s  %-12s  %s\n", base_buf, size_buf, e820_get_type_name(entry->type),
                  e820_get_type_name(entry->type));
    }

    /* Print summary */
    mem_stats_t stats;
    e820_get_stats(&stats);
    klog_info("[E820] Summary: Total=%u MB, Usable=%u MB, Reserved=%u MB\n", stats.total_mb,
              stats.usable_mb, bytes_to_mb(stats.reserved_bytes));
}

bool e820_is_range_usable(uint64_t base, uint64_t length) {
    if (!s_initialized) {
        return false;
    }

    if (length == 0) {
        return true; /* Zero-length range is trivially usable */
    }

    /* Check each entry */
    for (uint32_t i = 0; i < s_e820_entry_count; i++) {
        const e820_entry_t* entry = &s_e820_entries[i];
        uint64_t entry_start = entry->base;
        uint64_t entry_end = entry->base + entry->length;

        /* Check for overlap */
        if (!ranges_overlap(base, base + length, entry_start, entry_end)) {
            continue; /* No overlap */
        }

        /* If this entry overlaps and is NOT usable, range is not fully usable */
        if (entry->type != E820_TYPE_USABLE) {
            return false;
        }
    }

    return true;
}

bool e820_find_usable_range(uint64_t min_base, uint64_t min_length, uint64_t* out_base,
                            uint64_t* out_length) {
    if (!s_initialized || out_base == NULL || out_length == NULL) {
        return false;
    }

    /* Find the largest usable region at or above min_base */
    uint64_t best_base = 0;
    uint64_t best_length = 0;

    for (uint32_t i = 0; i < s_e820_entry_count; i++) {
        const e820_entry_t* entry = &s_e820_entries[i];

        if (entry->type != E820_TYPE_USABLE) {
            continue;
        }

        uint64_t entry_end = entry->base + entry->length;

        /* Check if entry is at or above min_base */
        if (entry_end <= min_base) {
            continue;
        }

        /* Calculate effective base (max of entry base and min_base) */
        uint64_t effective_base = (entry->base > min_base) ? entry->base : min_base;

        /* Check if we're past the entry */
        if (effective_base >= entry_end) {
            continue;
        }

        /* Calculate effective length */
        uint64_t effective_length = entry_end - effective_base;

        /* Check if this meets minimum length requirement */
        if (effective_length >= min_length && effective_length > best_length) {
            best_base = effective_base;
            best_length = effective_length;
        }
    }

    if (best_length > 0) {
        *out_base = best_base;
        *out_length = best_length;
        return true;
    }

    return false;
}

const char* e820_get_type_name(uint32_t type) {
    switch (type) {
        case E820_TYPE_USABLE:
            return "Usable";
        case E820_TYPE_RESERVED:
            return "Reserved";
        case E820_TYPE_ACPI_RECLAIM:
            return "ACPI Reclaim";
        case E820_TYPE_NVS:
            return "ACPI NVS";
        case E820_TYPE_UNUSABLE:
            return "Unusable";
        default:
            return "Unknown";
    }
}

uint64_t e820_get_usable_memory_above(uint64_t min_base) {
    if (!s_initialized) {
        return 0;
    }

    uint64_t max_usable = 0;

    for (uint32_t i = 0; i < s_e820_entry_count; i++) {
        const e820_entry_t* entry = &s_e820_entries[i];

        if (entry->type != E820_TYPE_USABLE) {
            continue;
        }

        /* Check if entry is at or above min_base */
        if ((entry->base + entry->length) > min_base) {
            if (entry->length > max_usable) {
                max_usable = entry->length;
            }
        }
    }

    return max_usable;
}

uint32_t e820_count_usable_regions(void) {
    if (!s_initialized) {
        return 0;
    }

    uint32_t count = 0;

    for (uint32_t i = 0; i < s_e820_entry_count; i++) {
        if (s_e820_entries[i].type == E820_TYPE_USABLE) {
            count++;
        }
    }

    return count;
}
