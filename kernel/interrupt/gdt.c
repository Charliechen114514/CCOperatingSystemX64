/* ==============================================================================
 * CCOS - Global Descriptor Table (GDT) Implementation
 * ==============================================================================
 */

#include "interrupt/gdt.h"
#include "base/memory.h"
#include "interrupt/tss.h"
#include "klogs/kprintf.h"
#include "serial/serial.h"

/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief Kernel GDT
 *
 * 7 entries:
 * 0: Null descriptor
 * 1: Kernel 64-bit code
 * 2: Kernel data
 * 3: User 64-bit code
 * 4: User data
 * 5: TSS (low 64 bits)
 * 6: TSS (high 64 bits, for x86_64)
 */
static struct {
    gdt_entry_t entries[5];    /* First 5 entries */
    gdt_tss_entry_t tss_entry; /* TSS entry (16 bytes) */
} __attribute__((aligned(16))) s_gdt = {0};

/**
 * @brief GDT pointer for lgdt instruction
 */
static gdt_ptr_t s_gdt_ptr = {0};

/* Flag to track initialization */
static bool s_initialized = false;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Set a GDT entry
 */
static void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access,
                          uint8_t granularity) {
    if (index < 0 || index >= 5) {
        klog_error("[GDT] Invalid index: %d\n", index);
        return;
    }

    s_gdt.entries[index].limit_low = limit & 0xFFFF;
    s_gdt.entries[index].base_low = base & 0xFFFF;
    s_gdt.entries[index].base_middle = (base >> 16) & 0xFF;
    s_gdt.entries[index].access = access;
    s_gdt.entries[index].granularity = (limit >> 16) & 0x0F;
    s_gdt.entries[index].granularity |= granularity & 0xF0;
    s_gdt.entries[index].base_high = (base >> 24) & 0xFF;
}

/**
 * @brief Set TSS entry in GDT
 */
static void gdt_set_tss(tss_t* tss) {
    uint64_t base = (uint64_t)tss;
    uint64_t limit = sizeof(tss_t) - 1;

    /* Set low 64 bits */
    s_gdt.tss_entry.limit_low = limit & 0xFFFF;
    s_gdt.tss_entry.base_low = base & 0xFFFF;
    s_gdt.tss_entry.base_middle = (base >> 16) & 0xFF;
    s_gdt.tss_entry.access = 0x89; /* Present, DPL0, Type=64-bit TSS */
    s_gdt.tss_entry.granularity = ((limit >> 16) & 0x0F) | 0x80; /* 4KB granularity */
    s_gdt.tss_entry.base_high = (base >> 24) & 0xFF;

    /* Set high 64 bits (x86_64 extension) */
    s_gdt.tss_entry.base_upper = (base >> 32) & 0xFFFFFFFF;
    s_gdt.tss_entry.reserved = 0;
}

/* ============================================================================
 * Assembly Functions
 * ============================================================================ */

extern void gdt_flush(uint64_t gdt_ptr);
extern void tss_load(void);

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void gdt_init(void) {
    if (s_initialized) {
        sync_serial_puts("[GDT] Already initialized\n");
        return;
    }

    sync_serial_puts("[GDT] Setting up kernel GDT...\n");

    /* Clear GDT */
    for (int i = 0; i < 5; i++) {
        for (size_t j = 0; j < sizeof(gdt_entry_t); j++) {
            ((uint8_t*)&s_gdt.entries[i])[j] = 0;
        }
    }

    /* Setup GDT pointer */
    s_gdt_ptr.limit = (sizeof(gdt_entry_t) * 5 + sizeof(gdt_tss_entry_t)) - 1;
    s_gdt_ptr.base = (uint64_t)&s_gdt;

    /* Entry 0: Null descriptor (already zeroed) */

    /* Entry 1: Kernel 64-bit code */
    gdt_set_entry(1, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_DPL0 | GDT_ACCESS_SYSTEM | GDT_ACCESS_TYPE_CODE,
                  GDT_GRANULARITY_4K | GDT_GRANULARITY_64BIT);

    /* Entry 2: Kernel data */
    gdt_set_entry(2, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_DPL0 | GDT_ACCESS_SYSTEM | GDT_ACCESS_TYPE_DATA,
                  GDT_GRANULARITY_4K | GDT_GRANULARITY_32BIT);

    /* Entry 3: User 64-bit code */
    gdt_set_entry(3, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_DPL3 | GDT_ACCESS_SYSTEM | GDT_ACCESS_TYPE_CODE,
                  GDT_GRANULARITY_4K | GDT_GRANULARITY_64BIT);

    /* Entry 4: User data */
    gdt_set_entry(4, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_DPL3 | GDT_ACCESS_SYSTEM | GDT_ACCESS_TYPE_DATA,
                  GDT_GRANULARITY_4K | GDT_GRANULARITY_32BIT);

    /* Entry 5: TSS */
    tss_t* tss = tss_get();
    if (tss == NULL) {
        sync_serial_puts("[GDT] TSS not initialized!\n");
        return;
    }
    gdt_set_tss(tss);

    /* Load GDT */
    gdt_flush((uint64_t)&s_gdt_ptr);

    /* Load TSS */
    tss_load();

    sync_serial_puts("[GDT] Kernel GDT loaded:\n");
    sync_serial_puts("[GDT]   NULL:        0x00\n");
    sync_serial_puts("[GDT]   Kernel Code: 0x08 (64-bit)\n");
    sync_serial_puts("[GDT]   Kernel Data: 0x10\n");
    sync_serial_puts("[GDT]   User Code:   0x18 (64-bit)\n");
    sync_serial_puts("[GDT]   User Data:   0x20\n");
    sync_serial_puts("[GDT]   TSS:         0x28\n");

    s_initialized = true;
}

const gdt_ptr_t* gdt_get_ptr(void) {
    return &s_gdt_ptr;
}

void gdt_dump(void) {
    if (!s_initialized) {
        klog_error("[GDT] Not initialized\n");
        return;
    }

    klog_info("[GDT] GDT Entries:\n");
    for (int i = 0; i < 5; i++) {
        const gdt_entry_t* e = &s_gdt.entries[i];
        uint32_t base = e->base_low | (e->base_middle << 16) | (e->base_high << 24);
        uint32_t limit = e->limit_low | ((e->granularity & 0x0F) << 16);
        klog_info("[GDT]   [%d] Base=0x%08X Limit=0x%08X Access=0x%02X Gran=0x%02X\n", i, base,
                  limit, e->access, e->granularity);
    }

    const gdt_tss_entry_t* tss = &s_gdt.tss_entry;
    uint64_t tss_base = tss->base_low | (tss->base_middle << 16) | (tss->base_high << 24) |
                        ((uint64_t)tss->base_upper << 32);
    uint32_t tss_limit = tss->limit_low | ((tss->granularity & 0x0F) << 16);
    klog_info("[GDT]   [5] TSS Base=0x%016llX Limit=0x%08X\n", tss_base, tss_limit);
}
