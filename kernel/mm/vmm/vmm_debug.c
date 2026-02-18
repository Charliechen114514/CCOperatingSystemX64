/* ==============================================================================
 * CCOS - Virtual Memory Manager Debug Functions
 * ==============================================================================
 */

#include "mm/vmm/vmm_debug_config.h"
#include "mm/vmm/vmm.h"
#include "klogs/kprintf.h"
#include "klogs/ksnprintf.h"
#include "mm/vmm/page.h"

/* External reference to kernel PML4 from vmm.c */
extern physical_addr_t vmm_get_kernel_pml4(void);

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/**
 * Structure to hold collected mapping information
 */
typedef struct {
    virtual_addr_t vaddr;
    physical_addr_t paddr;
    uint64_t flags;
    bool is_huge;
    uint8_t level; /* 0=PML4, 1=PDPT, 2=PD, 3=PT */
    char description[64];
} mapping_entry_t;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * str_append - Helper to append string to buffer
 */
static void str_append(char* buf, const char* str) {
    char* p = buf;
    while (*p)
        p++;
    while (*str)
        *p++ = *str++;
    *p = '\0';
}

/**
 * build_flags_str - Helper to build flags string
 */
static void build_flags_str(uint64_t flags, char* buf) {
    buf[0] = '\0';
    if (flags & PAGE_PRESENT)
        str_append(buf, "P");
    if (flags & PAGE_WRITE)
        str_append(buf, "W");
    if (flags & PAGE_USER)
        str_append(buf, "U");
    if (flags & PAGE_NO_EXEC)
        str_append(buf, "NX");
    if (flags & PAGE_GLOBAL)
        str_append(buf, "G");
    if (flags & PAGE_ACCESSED)
        str_append(buf, "A");
    if (flags & PAGE_DIRTY)
        str_append(buf, "D");
}

/**
 * collect_mappings - Collect memory mapping entries from page tables
 */
static int collect_mappings(physical_addr_t pml4_phys, mapping_entry_t* mappings, int max_count) {
    int count = 0;
    physical_addr_t kernel_pml4 = vmm_get_kernel_pml4();

    /* Use kernel PML4 if 0 is specified */
    if (pml4_phys == 0) {
        pml4_phys = kernel_pml4;
    }

    pml4_t* pml4 = (pml4_t*)phys_to_virt_offset(pml4_phys);

    /* Scan PML4 entries - only scan entries that are actually used */
    /* We'll scan indices that are likely to have mappings:
     * - 0: low memory (identity mapped by bootloader)
     * - 256: direct map region (kernel)
     * - 511: kernel high half (bootloader)
     */
    int indices_to_scan[] = {0, 256, 511, -1};

    for (int idx = 0; indices_to_scan[idx] >= 0; idx++) {
        int i = indices_to_scan[idx];

        page_table_entry_t* pml4e = &pml4->entries[i];
        if (!pml4e->bits.present)
            continue;

        physical_addr_t pdpt_phys = pml4e->bits.frame << PAGE_SHIFT;
        pdpt_t* pdpt = (pdpt_t*)phys_to_virt_offset(pdpt_phys);

        /* Scan PDPT entries */
        for (int j = 0; j < VMM_DEBUG_MAX_PDPT_ENTRIES && count < max_count; j++) {
            page_table_entry_t* pdpte = &pdpt->entries[j];
            if (!pdpte->bits.present)
                continue;

            physical_addr_t pd_phys = pdpte->bits.frame << PAGE_SHIFT;
            pd_t* pd = (pd_t*)phys_to_virt_offset(pd_phys);

            /* Check for 1GB huge page */
            if (pdpte->bits.pat) {
                mappings[count].vaddr = ((uint64_t)i << 39) | ((uint64_t)j << 30);
                mappings[count].paddr = pd_phys;
                mappings[count].flags = pdpte->value & 0xFFF;
                mappings[count].is_huge = true;
                mappings[count].level = 1;
                ksnprintf(mappings[count].description, 64, "1GB [%d:%d]", i, j);
                count++;
                continue;
            }

            /* Scan PD entries */
            for (int k = 0; k < VMM_DEBUG_MAX_PD_ENTRIES && count < max_count; k++) {
                page_table_entry_t* pde = &pd->entries[k];
                if (!pde->bits.present)
                    continue;

                physical_addr_t pt_phys = pde->bits.frame << PAGE_SHIFT;

                /* Check for 2MB huge page */
                if (pde->bits.pat) {
                    mappings[count].vaddr =
                        ((uint64_t)i << 39) | ((uint64_t)j << 30) | ((uint64_t)k << 21);
                    mappings[count].paddr = pt_phys;
                    mappings[count].flags = pde->value & 0xFFF;
                    mappings[count].is_huge = true;
                    mappings[count].level = 2;
                    ksnprintf(mappings[count].description, 64, "2MB [%d:%d:%d]", i, j, k);
                    count++;
                    continue;
                }

                /* Scan PT entries */
                pt_t* pt = (pt_t*)phys_to_virt_offset(pt_phys);
                for (int l = 0; l < VMM_DEBUG_MAX_PT_ENTRIES && count < max_count; l++) {
                    page_table_entry_t* pte = &pt->entries[l];
                    if (!pte->bits.present)
                        continue;

                    mappings[count].vaddr = ((uint64_t)i << 39) | ((uint64_t)j << 30) |
                                            ((uint64_t)k << 21) | ((uint64_t)l << 12);
                    mappings[count].paddr = pte->bits.frame << PAGE_SHIFT;
                    mappings[count].flags = pte->value & 0xFFF;
                    mappings[count].is_huge = false;
                    mappings[count].level = 3;
                    ksnprintf(mappings[count].description, 64, "4KB [%d:%d:%d:%d]", i, j, k, l);
                    count++;
                }
            }
        }
    }

    return count;
}

/* ============================================================================
 * Public Debug API
 * ============================================================================ */

void vmm_dump_memory_map(physical_addr_t pml4_phys, uint32_t max_entries) {
    physical_addr_t kernel_pml4 = vmm_get_kernel_pml4();

    if (pml4_phys == 0) {
        pml4_phys = kernel_pml4;
    }

    if (max_entries == 0 || max_entries > VMM_DEBUG_MAX_MAPPINGS) {
        max_entries = VMM_DEBUG_DEFAULT_DISPLAY;
    }

    klog_trace("\n");
    klog_trace("========================================\n");
    klog_trace("    Virtual to Physical Memory Map\n");
    klog_trace("========================================\n");
    klog_trace("PML4: 0x%X\n", pml4_phys);

    mapping_entry_t mappings[VMM_DEBUG_MAX_MAPPINGS];
    int count = collect_mappings(pml4_phys, mappings, max_entries);

    if (count == 0) {
        klog_trace("No mappings found.\n");
        klog_trace("========================================\n");
        return;
    }

    klog_trace("Found %d mapping entries:\n\n", count);

    /* Print header */
    klog_trace("Virtual Address              | Physical    | Type   | Flags\n");
    klog_trace("----------------------------|-------------|--------|-------\n");

    /* Print each mapping */
    for (int i = 0; i < count && i < (int)max_entries; i++) {
        mapping_entry_t* m = &mappings[i];

        /* Type string */
        const char* type_str;
        if (m->is_huge) {
            if (m->level == 1)
                type_str = "1GB";
            else if (m->level == 2)
                type_str = "2MB";
            else
                type_str = "???";
        } else {
            type_str = "4KB";
        }

        /* Build flags string */
        char flags_str[16];
        build_flags_str(m->flags, flags_str);

        klog_trace("0x%016llX | 0x%09X | %-6s | %s\n", m->vaddr, m->paddr, type_str, flags_str);
    }

    klog_trace("========================================\n");
}
