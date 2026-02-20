/* ==============================================================================
 * CCOS - Page Table Management Implementation
 * ==============================================================================
 */

#include "mm/vmm/page.h"
#include "assert/assert.h"
#include "base/memory.h"
#include "driver/serial/serial.h"
#include "klogs/kprintf.h"
#include "mm/pframe/pframe.h"
#include "sync/spinlock.h"

/* ==============================================================================
 * Internal State
 * ============================================================================== */

static physical_addr_t s_kernel_pml4 = 0;
static bool s_initialized = false;
static bool s_direct_map_established = false;

/* Spinlock protecting page table operations
 * This lock must be held when modifying page tables to prevent
 * concurrent modifications from multiple CPUs or interrupt handlers.
 */
static spinlock_t s_page_lock = SPIN_LOCK_INIT;

/* Helper macros for locking page table operations */
#define PAGE_LOCK(lock_flags) spin_lock_irqsave(&s_page_lock, &(lock_flags))
#define PAGE_UNLOCK(lock_flags) spin_unlock_irqrestore(&s_page_lock, (lock_flags))

/* Direct physical map offset - must match PHYS_MAP_OFFSET from vmm.h */
#define DIRECT_MAP_BASE 0xFFFF800000000000ULL

/* Temporary identity mapping access - only use during early init when
 * direct mapping is not yet established. The bootloader maps low 2MB.
 */
#define TEMP_IDENTITY_VIRT(phys) ((void*)(uintptr_t)(phys))

/* ==============================================================================
 * Helper Functions
 * ============================================================================== */

/* Helper: Convert flags from VMAP format to PTE format */
static inline uint64_t vmap_flags_to_pte(uint64_t vmap_flags) {
    uint64_t pte_flags = PAGE_PRESENT;

    if (vmap_flags & VMAP_FLAG_WRITE) {
        pte_flags |= PAGE_WRITE;
    }
    if (vmap_flags & VMAP_FLAG_USER) {
        pte_flags |= PAGE_USER;
    }
    if (vmap_flags & VMAP_FLAG_NO_EXEC) {
        pte_flags |= PAGE_NO_EXEC;
    }
    if (vmap_flags & VMAP_FLAG_WRITE_THRU) {
        pte_flags |= PAGE_WRITE_THRU;
    }
    if (vmap_flags & VMAP_FLAG_NO_CACHE) {
        pte_flags |= PAGE_NO_CACHE;
    }

    return pte_flags;
}

/* Helper: Get virtual address for a physical address
 * Uses the direct physical mapping region to access physical memory.
 * This is required for x86_64 where low memory is not identity mapped.
 *
 * NOTE: This must only be called AFTER s_direct_map_established is true!
 */
static inline void* phys_to_virt(physical_addr_t phys) {
    /* Convert physical address to direct-mapped virtual address */
    return (void*)(uintptr_t)(DIRECT_MAP_BASE + (uint64_t)phys);
}

/* ============================================================================
 * Direct Physical Map Setup
 * ============================================================================ */

/**
 * establish_direct_map - Set up direct physical mapping
 *
 * The bootloader only maps low 2MB. We need to establish a proper
 * direct mapping at KERNEL_VIRT_BASE (0xFFFF800000000000) which
 * corresponds to PML4 entry 256.
 *
 * This function uses identity mapping (which the bootloader provided)
 * to access and modify page tables.
 */
static page_result_t establish_direct_map(void) {
    /* The bootloader's PML4 is at physical address 0x9000 */
    physical_addr_t pml4_phys = s_kernel_pml4; /* Should be 0x9000 */

    /* Use identity mapping to access PML4 (bootloader maps low 2MB) */
    volatile pml4_t* pml4 = (volatile pml4_t*)TEMP_IDENTITY_VIRT(pml4_phys);

    /* Check if PML4[256] is already mapped */
    if (pml4->entries[256].bits.present) {
        klog_info("[PAGE] Direct mapping already exists at PML4[256]\n");
        s_direct_map_established = true;
        return PAGE_OK;
    }

    /* Strategy: Create a proper direct map for more physical memory
     * The bootloader only maps low 2MB. We need to expand this.
     *
     * For a 512MB direct map using 2MB pages, we need:
     * - 1 PDPT (512 entries, but we only use first entry for now)
     * - 256 PDs (each has 512 2MB entries, but we only use first entry per PD)
     *
     * Actually, for 2MB huge pages:
     * - PML4[256] → PDPT
     * - PDPT[0] → PD (maps first 512GB using 2MB pages)
     * - Each PD entry maps 2MB
     *
     * But the bootloader's PDPT only has PDPT[0] set, and that PD only has PD[0] set!
     * We need to expand the PD to have 256 entries, not create new PDs.
     */

    /* Get the bootloader's PD (at 0xB000) via identity mapping */
    volatile pd_t* pd = (volatile pd_t*)TEMP_IDENTITY_VIRT(0xB000);

    /* The bootloader's PD has only PD[0] mapped (2MB at 0x0)
     * We need to add more 2MB page entries to map more physical memory.
     *
     * For a 512MB direct map, we need 256 2MB pages in the SAME PD.
     */
    for (int i = 0; i < 256; i++) {
        if (!pd->entries[i].bits.present) {
            /* Map a 2MB page at physical address i * 2MB */
            physical_addr_t paddr = i * 0x200000;
            pd->entries[i].value =
                (paddr & PTE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE_PD;
        }
    }

    /* Set PML4[256] to point to the bootloader's PDPT
     * This PDPT already has PDPT[0] → PD (0xB000) set by the bootloader
     */
    pml4->entries[256].value = (0xA000 & PTE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE;

    /* Invalidate TLB to ensure the new mapping takes effect */
    __asm__ volatile("mov %0, %%cr3" : : "r"((uint64_t)pml4_phys) : "memory");

    s_direct_map_established = true;

    klog_info("[PAGE] Direct mapping established at PML4[256]\n");
    klog_info("[PAGE]   Virt base: 0x%016llX\n", DIRECT_MAP_BASE);
    klog_info("[PAGE]   Phys range: 0x00000000 - 0x1FFFFFFF (512MB)\n");

    return PAGE_OK;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

page_result_t page_init(void) {
    /* Debug: print function entry */
    sync_serial_puts("[DEBUG] page_init: entry\n");

    if (s_initialized) {
        klog_warn("[PAGE] Already initialized\n");
        return PAGE_OK;
    }

    /* Debug: before reading CR3 */
    sync_serial_puts("[DEBUG] page_init: before page_get_cr3\n");

    /* Read the current PML4 from CR3 */
    s_kernel_pml4 = page_get_cr3() & ~0xFFFULL; /* CR3 low 12 bits are flags */

    /* Debug: after reading CR3 */
    sync_serial_puts("[DEBUG] page_init: after page_get_cr3\n");

    klog_info("[PAGE] Initializing with PML4 at 0x%016llX\n", s_kernel_pml4);
    klog_info("[PAGE] Page size: %d bytes, %d entries per table\n", PAGE_SIZE, PT_ENTRIES);

    /* Establish direct physical mapping */
    page_result_t result = establish_direct_map();
    if (result != PAGE_OK) {
        klog_error("[PAGE] Failed to establish direct mapping!\n");
        return result;
    }

    /* Now that direct mapping is established, we can clear the .lbss section
     * which contains the 2MB bitmap storage.
     *
     * IMPORTANT: The kernel is linked at low physical addresses (identity mapped),
     * so &__lbss_start gives us the physical address directly. We need to use
     * the direct physical mapping to access it safely, avoiding potential stack
     * corruption issues.
     */
    extern uint8_t __lbss_start;
    extern uint8_t __lbss_end;

    /* Get physical address of .lbss section
     * Since kernel is identity mapped (linked at physical address),
     * the symbol address IS the physical address.
     */
    physical_addr_t lbss_phys_start = (physical_addr_t)&__lbss_start;
    physical_addr_t lbss_phys_end = (physical_addr_t)&__lbss_end;
    uint64_t lbss_size = lbss_phys_end - lbss_phys_start;

    sync_serial_puts("[PAGE] Skipping .lbss clear for now...\n");

    /* TODO: Fix .lbss clearing - it's causing crashes
     * The .lbss section will be used uninitialized (zeros from bootloader)
     */
    (void)lbss_size;

    s_initialized = true;
    klog_info("[PAGE] Initialization complete\n");

    return PAGE_OK;
}

physical_addr_t page_get_pml4(void) {
    CCOS_ASSERT(s_initialized);
    return s_kernel_pml4;
}

page_result_t page_create_table(physical_addr_t* out_phys) {
    CCOS_ASSERT(s_initialized);
    CCOS_ASSERT(s_direct_map_established); /* Must check before using phys_to_virt! */
    CCOS_ASSERT(out_phys != NULL);

    /* Allocate a physical frame */
    pframe_result_t result = pframe_alloc(out_phys);
    if (result != PFRAME_OK) {
        klog_error("[PAGE] Failed to allocate frame for page table\n");
        return PAGE_ERR_OOM;
    }

    /* Clear the new page table */
    void* virt = phys_to_virt(*out_phys);
    memset(virt, 0, PAGE_SIZE);

    return PAGE_OK;
}

page_result_t page_free_table(physical_addr_t phys) {
    CCOS_ASSERT(s_initialized);

    /* Free the physical frame */
    pframe_result_t result = pframe_free(phys);
    if (result != PFRAME_OK) {
        klog_error("[PAGE] Failed to free page table at 0x%X\n", phys);
        return PAGE_ERR_INVALID;
    }

    return PAGE_OK;
}

static page_result_t get_or_create_table(physical_addr_t* table_phys, bool alloc_missing) {
    if (*table_phys != 0) {
        return PAGE_OK;
    }

    if (!alloc_missing) {
        return PAGE_ERR_NOT_PRESENT;
    }

    /* Allocate new table */
    return page_create_table(table_phys);
}

page_result_t page_map_page(physical_addr_t pml4_phys, virtual_addr_t vaddr, physical_addr_t paddr,
                            uint64_t flags, bool alloc_missing) {
    CCOS_ASSERT(s_initialized);
    CCOS_ASSERT(s_direct_map_established);

    page_result_t ret = PAGE_OK;
    spinlock_flags_t lock_flags;

    /* Lock to protect concurrent page table modifications */
    PAGE_LOCK(lock_flags);

    /* Determine page size from flags */
    bool use_2mb = (flags & VMAP_FLAG_HUGE_2MB) != 0;
    bool use_1gb = (flags & VMAP_FLAG_HUGE_1GB) != 0;

    /* Validate only one huge page flag is set */
    if (use_2mb && use_1gb) {
        klog_error("[PAGE] Cannot set both HUG_2MB and HUG_1GB flags\n");
        ret = PAGE_ERR_INVALID;
        goto unlock;
    }

    /* Determine alignment requirements */
    uint64_t page_size = PAGE_SIZE;
    uint64_t page_align = PAGE_SIZE - 1;

    if (use_1gb) {
        page_size = PAGE_SIZE_1GB;
        page_align = page_size - 1;
    } else if (use_2mb) {
        page_size = PAGE_SIZE_2MB;
        page_align = page_size - 1;
    }

    /* Validate alignment */
    if ((vaddr & page_align) != 0) {
        klog_error("[PAGE] Virtual address not aligned to %llu bytes: 0x%llX\n", page_size, vaddr);
        ret = PAGE_ERR_ALIGNMENT;
        goto unlock;
    }
    if ((paddr & page_align) != 0) {
        klog_error("[PAGE] Physical address not aligned to %llu bytes: 0x%X\n", page_size, paddr);
        ret = PAGE_ERR_ALIGNMENT;
        goto unlock;
    }

    /* Convert flags to PTE format */
    uint64_t pte_flags = vmap_flags_to_pte(flags);

    /* Get PML4 */
    pml4_t* pml4 = (pml4_t*)phys_to_virt(pml4_phys);
    page_table_entry_t* pml4e = &pml4->entries[PML4_INDEX(vaddr)];

    /* Walk or create PDPT */
    physical_addr_t pdpt_phys = pml4e->bits.frame << PAGE_SHIFT;
    page_result_t result = get_or_create_table(&pdpt_phys, alloc_missing);
    if (result != PAGE_OK) {
        ret = result;
        goto unlock;
    }

    /* Update PML4 entry if we created a new PDPT */
    if (pdpt_phys != (pml4e->bits.frame << PAGE_SHIFT)) {
        pml4e->value = (pdpt_phys & PTE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }

    /* Get PDPT */
    pdpt_t* pdpt = (pdpt_t*)phys_to_virt(pdpt_phys);
    page_table_entry_t* pdpte = &pdpt->entries[PDPT_INDEX(vaddr)];

    /* Handle 1GB huge page mapping */
    if (use_1gb) {
        if (pdpte->bits.present) {
            klog_error("[PAGE] PDPT entry already present at 0x%llX\n", vaddr);
            ret = PAGE_ERR_ALREADY_MAPPED;
            goto unlock;
        }

        /* Create 1GB huge page mapping at PDPT level */
        pdpte->value = (paddr & PTE_ADDR_MASK) | pte_flags | PAGE_HUGE_PDPT;

        /* Invalidate TLB */
        page_invalidate_tlb(vaddr);

        goto unlock;
    }

    /* Walk or create PD */
    physical_addr_t pd_phys = pdpte->bits.frame << PAGE_SHIFT;
    result = get_or_create_table(&pd_phys, alloc_missing);
    if (result != PAGE_OK) {
        ret = result;
        goto unlock;
    }

    /* Update PDPT entry if we created a new PD */
    if (pd_phys != (pdpte->bits.frame << PAGE_SHIFT)) {
        pdpte->value = (pd_phys & PTE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }

    /* Get PD */
    pd_t* pd = (pd_t*)phys_to_virt(pd_phys);
    page_table_entry_t* pde = &pd->entries[PD_INDEX(vaddr)];

    /* Handle 2MB huge page mapping */
    if (use_2mb) {
        if (pde->bits.present) {
            klog_error("[PAGE] PD entry already present at 0x%llX\n", vaddr);
            ret = PAGE_ERR_ALREADY_MAPPED;
            goto unlock;
        }

        /* Create 2MB huge page mapping at PD level */
        pde->value = (paddr & PTE_ADDR_MASK) | pte_flags | PAGE_HUGE_PD;

        /* Invalidate TLB */
        page_invalidate_tlb(vaddr);

        goto unlock;
    }

    /* Check if PD entry is a huge page */
    if (pde->bits.present && pde->bits.pat) {
        /* PD entry is a 2MB huge page. We need to break it into 4KB pages. */
        /* For now, return an error as this is a complex operation. */
        klog_error("[PAGE] Cannot map 4KB page in 2MB huge page region at 0x%llX\n", vaddr);
        ret = PAGE_ERR_INVALID;
        goto unlock;
    }

    /* Walk or create PT */
    physical_addr_t pt_phys = pde->bits.frame << PAGE_SHIFT;
    result = get_or_create_table(&pt_phys, alloc_missing);
    if (result != PAGE_OK) {
        ret = result;
        goto unlock;
    }

    /* Update PD entry if we created a new PT */
    if (pt_phys != (pde->bits.frame << PAGE_SHIFT)) {
        pde->value = (pt_phys & PTE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }

    /* Get PT and set PTE */
    pt_t* pt = (pt_t*)phys_to_virt(pt_phys);
    page_table_entry_t* pte = &pt->entries[PT_INDEX(vaddr)];

    /* Check if already mapped */
    if (pte->bits.present) {
        klog_warn("[PAGE] Page already mapped at 0x%llX -> 0x%X\n", vaddr,
                  pte->bits.frame << PAGE_SHIFT);
        ret = PAGE_ERR_ALREADY_MAPPED;
        goto unlock;
    }

    /* Set the 4KB page PTE */
    pte->value = (paddr & PTE_ADDR_MASK) | pte_flags;

    /* Invalidate TLB for this address */
    page_invalidate_tlb(vaddr);

unlock:
    PAGE_UNLOCK(lock_flags);
    return ret;
}

page_result_t page_unmap_page(physical_addr_t pml4_phys, virtual_addr_t vaddr, bool free_table) {
    CCOS_ASSERT(s_initialized);
    CCOS_ASSERT(s_direct_map_established);

    (void)free_table; /* TODO: Implement free_table functionality */

    page_result_t ret = PAGE_OK;
    spinlock_flags_t lock_flags;

    /* Lock to protect concurrent page table modifications */
    PAGE_LOCK(lock_flags);

    /* Validate alignment */
    if ((vaddr & (PAGE_SIZE - 1)) != 0) {
        ret = PAGE_ERR_ALIGNMENT;
        goto unlock;
    }

    /* Get PML4 */
    pml4_t* pml4 = (pml4_t*)phys_to_virt(pml4_phys);
    page_table_entry_t* pml4e = &pml4->entries[PML4_INDEX(vaddr)];

    if (!pml4e->bits.present) {
        ret = PAGE_ERR_NOT_MAPPED;
        goto unlock;
    }

    /* Get PDPT */
    pdpt_t* pdpt = (pdpt_t*)phys_to_virt(pml4e->bits.frame << PAGE_SHIFT);
    page_table_entry_t* pdpte = &pdpt->entries[PDPT_INDEX(vaddr)];

    if (!pdpte->bits.present) {
        ret = PAGE_ERR_NOT_MAPPED;
        goto unlock;
    }

    /* Get PD */
    pd_t* pd = (pd_t*)phys_to_virt(pdpte->bits.frame << PAGE_SHIFT);
    page_table_entry_t* pde = &pd->entries[PD_INDEX(vaddr)];

    if (!pde->bits.present) {
        ret = PAGE_ERR_NOT_MAPPED;
        goto unlock;
    }

    /* Get PT */
    pt_t* pt = (pt_t*)phys_to_virt(pde->bits.frame << PAGE_SHIFT);
    page_table_entry_t* pte = &pt->entries[PT_INDEX(vaddr)];

    if (!pte->bits.present) {
        ret = PAGE_ERR_NOT_MAPPED;
        goto unlock;
    }

    /* Clear the PTE */
    pte->value = 0;

    /* Invalidate TLB */
    page_invalidate_tlb(vaddr);

    /* TODO: Free empty page tables if free_table is true */

unlock:
    PAGE_UNLOCK(lock_flags);
    return ret;
}

page_result_t page_query(physical_addr_t pml4_phys, virtual_addr_t vaddr,
                         page_query_result_t* result) {
    CCOS_ASSERT(s_initialized);
    CCOS_ASSERT(s_direct_map_established);
    CCOS_ASSERT(result != NULL);

    /* Initialize result */
    result->present = false;
    result->is_huge = false;
    result->phys_addr = 0;
    result->flags = 0;

    /* Get PML4 */
    pml4_t* pml4 = (pml4_t*)phys_to_virt(pml4_phys);
    page_table_entry_t* pml4e = &pml4->entries[PML4_INDEX(vaddr)];

    if (!pml4e->bits.present) {
        return PAGE_ERR_NOT_PRESENT;
    }

    /* Get PDPT */
    pdpt_t* pdpt = (pdpt_t*)phys_to_virt(pml4e->bits.frame << PAGE_SHIFT);
    page_table_entry_t* pdpte = &pdpt->entries[PDPT_INDEX(vaddr)];

    if (!pdpte->bits.present) {
        return PAGE_ERR_NOT_PRESENT;
    }

    /* Check for 1GB huge page */
    if (pdpte->bits.pat) { /* PS bit in PDPT */
        result->present = true;
        result->is_huge = true;
        result->phys_addr = pdpte->bits.frame << PAGE_SHIFT;
        result->flags = pdpte->value & 0xFFF;
        return PAGE_OK;
    }

    /* Get PD */
    pd_t* pd = (pd_t*)phys_to_virt(pdpte->bits.frame << PAGE_SHIFT);
    page_table_entry_t* pde = &pd->entries[PD_INDEX(vaddr)];

    if (!pde->bits.present) {
        return PAGE_ERR_NOT_PRESENT;
    }

    /* Check for 2MB huge page */
    if (pde->bits.pat) { /* PS bit in PD */
        result->present = true;
        result->is_huge = true;
        result->phys_addr = pde->bits.frame << PAGE_SHIFT;
        result->flags = pde->value & 0xFFF;
        return PAGE_OK;
    }

    /* Get PT */
    pt_t* pt = (pt_t*)phys_to_virt(pde->bits.frame << PAGE_SHIFT);
    page_table_entry_t* pte = &pt->entries[PT_INDEX(vaddr)];

    if (!pte->bits.present) {
        return PAGE_ERR_NOT_PRESENT;
    }

    /* Return 4KB page mapping info */
    result->present = true;
    result->is_huge = false;
    result->phys_addr = pte->bits.frame << PAGE_SHIFT;
    result->flags = pte->value & 0xFFF;

    return PAGE_OK;
}

page_result_t page_virt_to_phys(physical_addr_t pml4_phys, virtual_addr_t vaddr,
                                physical_addr_t* out_phys) {
    CCOS_ASSERT(out_phys != NULL);

    page_query_result_t result;
    page_result_t res = page_query(pml4_phys, vaddr, &result);

    if (res == PAGE_OK && result.present) {
        *out_phys = result.phys_addr | (vaddr & (PAGE_SIZE - 1));
        return PAGE_OK;
    }

    return PAGE_ERR_NOT_PRESENT;
}

page_result_t page_map_range(physical_addr_t pml4_phys, virtual_addr_t vaddr_base,
                             physical_addr_t paddr_base, uint64_t page_count, uint64_t flags) {
    CCOS_ASSERT(s_initialized);

    for (uint64_t i = 0; i < page_count; i++) {
        virtual_addr_t vaddr = vaddr_base + (i * PAGE_SIZE);
        physical_addr_t paddr = paddr_base + (i * PAGE_SIZE);

        page_result_t result = page_map_page(pml4_phys, vaddr, paddr, flags, true);
        if (result != PAGE_OK) {
            /* Rollback: unmap what we've mapped so far */
            page_unmap_range(pml4_phys, vaddr_base, i);
            return result;
        }
    }

    return PAGE_OK;
}

page_result_t page_unmap_range(physical_addr_t pml4_phys, virtual_addr_t vaddr_base,
                               uint64_t page_count) {
    CCOS_ASSERT(s_initialized);

    for (uint64_t i = 0; i < page_count; i++) {
        virtual_addr_t vaddr = vaddr_base + (i * PAGE_SIZE);
        page_unmap_page(pml4_phys, vaddr, false);
    }

    return PAGE_OK;
}

void page_flush_cache(virtual_addr_t vaddr, size_t size) {
    /* Flush data cache for the given range */
    __asm__ volatile("mfence" ::: "memory");

    /* Invalidate cache line by line (64 bytes per line) */
    virtual_addr_t end = vaddr + size;
    for (virtual_addr_t addr = vaddr; addr < end; addr += 64) {
        __asm__ volatile("clflush (%0)" : : "r"(addr) : "memory");
    }

    __asm__ volatile("mfence" ::: "memory");
}

/* ============================================================================
 * Page Table Dumping Functions
 * ============================================================================ */

/**
 * Helper to print page table entry flags
 */
static void dump_pte_flags(uint64_t flags) {
    klog_trace("    Flags: ");
    if (flags & PAGE_PRESENT)
        klog_trace("P ");
    if (flags & PAGE_WRITE)
        klog_trace("W ");
    if (flags & PAGE_USER)
        klog_trace("U ");
    if (flags & PAGE_NO_EXEC)
        klog_trace("NX ");
    if (flags & PAGE_GLOBAL)
        klog_trace("G ");
    if (flags & PAGE_ACCESSED)
        klog_trace("A ");
    if (flags & PAGE_DIRTY)
        klog_trace("D ");
    if (flags & PAGE_HUGE_PD)
        klog_trace("HUGE_PD ");
    if (flags & PAGE_HUGE_PDPT)
        klog_trace("HUGE_PDPT ");
    if (flags & PAGE_WRITE_THRU)
        klog_trace("WT ");
    if (flags & PAGE_NO_CACHE)
        klog_trace("CD ");
    klog_trace("\n");
}

void page_dump_mapping(virtual_addr_t vaddr, int levels) {
    CCOS_ASSERT(s_initialized);
    CCOS_ASSERT(s_direct_map_established);

    if (levels < 1 || levels > 4) {
        levels = 4;
    }

    klog_trace("\n=== Page Table Dump for 0x%016llX ===\n", vaddr);

    /* Level 1: PML4 */
    pml4_t* pml4 = (pml4_t*)phys_to_virt(s_kernel_pml4);
    page_table_entry_t* pml4e = &pml4->entries[PML4_INDEX(vaddr)];

    klog_trace("[PML4] Index: %d (0x%03llX)\n", PML4_INDEX(vaddr), PML4_INDEX(vaddr));
    klog_trace("       Entry: 0x%016llX\n", pml4e->value);
    if (!pml4e->bits.present) {
        klog_trace("       Status: NOT PRESENT\n");
        return;
    }
    klog_trace("       PDPT Phys: 0x%X\n", pml4e->bits.frame << PAGE_SHIFT);
    dump_pte_flags(pml4e->value);

    if (levels < 2)
        return;

    /* Level 2: PDPT */
    pdpt_t* pdpt = (pdpt_t*)phys_to_virt(pml4e->bits.frame << PAGE_SHIFT);
    page_table_entry_t* pdpte = &pdpt->entries[PDPT_INDEX(vaddr)];

    klog_trace("[PDPT] Index: %d (0x%03llX)\n", PDPT_INDEX(vaddr), PDPT_INDEX(vaddr));
    klog_trace("       Entry: 0x%016llX\n", pdpte->value);
    if (!pdpte->bits.present) {
        klog_trace("       Status: NOT PRESENT\n");
        return;
    }
    klog_trace("       PD Phys: 0x%X\n", pdpte->bits.frame << PAGE_SHIFT);
    if (pdpte->bits.pat) {
        klog_trace("       Type: 1GB Huge Page\n");
        klog_trace("       Mapped: 0x%016llX -> 0x%X\n", vaddr & ~0x3FFFFFFFULL,
                   pdpte->bits.frame << PAGE_SHIFT);
        dump_pte_flags(pdpte->value);
        return;
    }
    dump_pte_flags(pdpte->value);

    if (levels < 3)
        return;

    /* Level 3: PD */
    pd_t* pd = (pd_t*)phys_to_virt(pdpte->bits.frame << PAGE_SHIFT);
    page_table_entry_t* pde = &pd->entries[PD_INDEX(vaddr)];

    klog_trace("[PD]   Index: %d (0x%03llX)\n", PD_INDEX(vaddr), PD_INDEX(vaddr));
    klog_trace("       Entry: 0x%016llX\n", pde->value);
    if (!pde->bits.present) {
        klog_trace("       Status: NOT PRESENT\n");
        return;
    }
    klog_trace("       PT Phys: 0x%X\n", pde->bits.frame << PAGE_SHIFT);
    if (pde->bits.pat) {
        klog_trace("       Type: 2MB Huge Page\n");
        klog_trace("       Mapped: 0x%016llX -> 0x%X\n", vaddr & ~0x1FFFFFULL,
                   pde->bits.frame << PAGE_SHIFT);
        dump_pte_flags(pde->value);
        return;
    }
    dump_pte_flags(pde->value);

    if (levels < 4)
        return;

    /* Level 4: PT */
    pt_t* pt = (pt_t*)phys_to_virt(pde->bits.frame << PAGE_SHIFT);
    page_table_entry_t* pte = &pt->entries[PT_INDEX(vaddr)];

    klog_trace("[PT]   Index: %d (0x%03llX)\n", PT_INDEX(vaddr), PT_INDEX(vaddr));
    klog_trace("       Entry: 0x%016llX\n", pte->value);
    if (!pte->bits.present) {
        klog_trace("       Status: NOT PRESENT\n");
        return;
    }
    klog_trace("       Page Phys: 0x%X\n", pte->bits.frame << PAGE_SHIFT);
    klog_trace("       Mapped: 0x%016llX -> 0x%X\n", vaddr, pte->bits.frame << PAGE_SHIFT);
    dump_pte_flags(pte->value);
}

void page_dump_pml4(physical_addr_t pml4_phys) {
    CCOS_ASSERT(s_initialized);
    CCOS_ASSERT(s_direct_map_established);

    klog_trace("\n=== PML4 Dump (Phys: 0x%X) ===\n", pml4_phys);

    pml4_t* pml4 = (pml4_t*)phys_to_virt(pml4_phys);

    /* Count present entries */
    int present_count = 0;
    for (int i = 0; i < 512; i++) {
        if (pml4->entries[i].bits.present) {
            present_count++;
        }
    }

    klog_trace("Total PML4 entries present: %d/512\n", present_count);
    klog_trace("\n");

    /* Dump each present entry */
    for (int i = 0; i < 512; i++) {
        page_table_entry_t* entry = &pml4->entries[i];
        if (entry->bits.present) {
            const char* region_name = "";
            if (i == 0)
                region_name = " (Low Memory)";
            else if (i == 256)
                region_name = " (Direct Map)";
            else if (i == 511)
                region_name = " (Kernel High Half)";

            klog_trace("[PML4[%3d]%s -> PDPT at 0x%X\n", i, region_name,
                       entry->bits.frame << PAGE_SHIFT);
        }
    }

    klog_trace("=== End PML4 Dump ===\n\n");
}
