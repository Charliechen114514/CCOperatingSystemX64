/* ==============================================================================
 * CCOS - Virtual Memory Manager Implementation
 * ==============================================================================
 */

#include "mm/vmm/vmm.h"
#include "assert/assert.h"
#include "driver/serial/serial.h"
#include "klogs/kprintf.h"
#include "mm/pframe/pframe.h"
#include "sync/spinlock.h"

/* ==============================================================================
 * Internal State
 * ============================================================================== */

/* Kernel's PML4 (set by bootloader) */
static physical_addr_t s_kernel_pml4 = 0;

/* VMM statistics */
static vmm_stats_t s_stats = {0};

/* Memory region tracking */
#define MAX_REGIONS 64
static memory_region_t s_regions[MAX_REGIONS];
static uint32_t s_region_count = 0;

/* Virtual address allocation hint for kernel mappings */
static virtual_addr_t s_kernel_virt_hint = KERNEL_GENERAL_BASE;

/* Initialization flag */
static bool s_initialized = false;

/* Spinlock protecting VMM state (regions, stats, virt_hint) */
static spinlock_t s_vmm_lock = SPIN_LOCK_INIT;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * check_region_collision - Check if a range collides with any existing region
 */
static bool check_region_collision(virtual_addr_t start, virtual_addr_t end) {
    for (uint32_t i = 0; i < s_region_count; i++) {
        memory_region_t* region = &s_regions[i];
        /* Check for overlap: two ranges [a,b) and [c,d) overlap if a < d && c < b */
        if (start < region->end && end > region->start) {
            return true;
        }
    }
    return false;
}

/**
 * find_free_virt_range - Find a free virtual address range for kernel mapping
 * @param size Size of the range to find (determines alignment requirement)
 * @param out_vaddr Output pointer for the found virtual address
 * @return VMM_OK on success, error code otherwise
 *
 * NOTE: Caller must hold s_vmm_lock
 */
static vmm_result_t find_free_virt_range(uint64_t size, virtual_addr_t* out_vaddr) {
    virtual_addr_t hint = s_kernel_virt_hint;
    virtual_addr_t end = KERNEL_GENERAL_MAX; /* Don't go beyond general allocation region */

    /* Determine alignment based on size - use the size itself as alignment */
    uint64_t alignment = (size >= PAGE_SIZE_2MB) ? PAGE_SIZE_2MB : PAGE_SIZE;

    /* Align hint to the alignment boundary */
    hint = (hint + alignment - 1) & ~(alignment - 1);

    /* Search for a free range, checking for collisions with existing regions */
    while (hint + size <= end) {
        if (!check_region_collision(hint, hint + size)) {
            /* Found a free range */
            *out_vaddr = hint;
            s_kernel_virt_hint = hint + size;
            return VMM_OK;
        }
        /* Move to next alignment boundary */
        hint += alignment;
    }

    klog_error("[VMM] Out of kernel virtual address space\n");
    return VMM_ERR_OOM;
}

/**
 * add_region - Add a memory region to the tracking list
 *
 * NOTE: Caller must hold s_vmm_lock
 */
static void add_region(virtual_addr_t start, virtual_addr_t end, physical_addr_t phys_start,
                       uint64_t flags, const char* name) {
    if (s_region_count >= MAX_REGIONS) {
        klog_warn("[VMM] Region table full, cannot track %s\n", name);
        return;
    }

    memory_region_t* region = &s_regions[s_region_count++];
    region->start = start;
    region->end = end;
    region->phys_start = phys_start;
    region->flags = flags;

    /* Copy name (truncate if needed) */
    int i = 0;
    while (name[i] != '\0' && i < 31) {
        region->name[i] = name[i];
        i++;
    }
    region->name[i] = '\0';
}

/**
 * find_region - Find the region containing a virtual address
 */
static memory_region_t* find_region(virtual_addr_t addr) {
    for (uint32_t i = 0; i < s_region_count; i++) {
        memory_region_t* region = &s_regions[i];
        if (addr >= region->start && addr < region->end) {
            return region;
        }
    }
    return NULL;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

vmm_result_t vmm_init(void) {
    klog_info("[VMM] Start Virtual Memory Manager Inits...\n");

    if (s_initialized) {
        /* Use sync_serial_puts for debugging instead of klog_warn */
        sync_serial_puts("[VMM] Already initialized (debug check)\n");
        return VMM_OK;
    }

    klog_info("[VMM] Initializing Virtual Memory Manager...\n");

    /* Get the kernel PML4 from page module */
    s_kernel_pml4 = page_get_pml4();
    /* Debug: Check s_kernel_virt_hint value before setting initialized flag */
    const char* should_never_empty = "III\n";
    klog_info("%p -> %d", should_never_empty, *should_never_empty);

    klog_info("[VMM] Kernel PML4 at 0x%016llX\n", s_kernel_pml4);
    klog_info("[VMM] Address space layout:\n");
    klog_info("[VMM]   Direct map:  0x%016llX - 0x%016llX\n", KERNEL_VIRT_BASE, KERNEL_VIRT_END);
    klog_info("[VMM]   Kernel text: 0x%016llX - 0x%016llX\n", KERNEL_TEXT_BASE,
              KERNEL_TEXT_BASE + KERNEL_TEXT_SIZE);
    klog_info("[VMM]   Kernel data: 0x%016llX - 0x%016llX\n", KERNEL_DATA_BASE,
              KERNEL_DATA_BASE + KERNEL_DATA_SIZE);
    klog_info("[VMM]   Kernel heap: 0x%016llX - 0x%016llX\n", KERNEL_HEAP_BASE, KERNEL_HEAP_MAX);
    klog_info("[VMM]   General alloc: 0x%016llX - 0x%016llX\n", KERNEL_GENERAL_BASE,
              KERNEL_GENERAL_MAX);
    klog_info("[VMM]   User space:  0x%016llX - 0x%016llX\n", USER_BASE, USER_END);

    /* Register fixed kernel regions */
    add_region(KERNEL_TEXT_BASE, KERNEL_TEXT_BASE + KERNEL_TEXT_SIZE, 0, VMAP_FLAG_NONE,
               "kernel_text");
    add_region(KERNEL_DATA_BASE, KERNEL_DATA_BASE + KERNEL_DATA_SIZE, 0, VMAP_FLAG_WRITE,
               "kernel_data");

    /* Register heap region as reserved so vmm_alloc_pages doesn't use it
     * The heap allocator will use vmm_alloc_pages_at to get fixed addresses
     * within this region, ensuring heap contiguity */
    add_region(KERNEL_HEAP_BASE, KERNEL_HEAP_MAX, 0, VMAP_FLAG_NONE, "kernel_heap");

    /* Debug: Check s_kernel_virt_hint value before setting initialized flag */
    // const char* should_never_empty = "[VMM] Checking s_kernel_virt_hint...\n";
    // klog_info(should_never_empty);

    klog_info("[VMM] s_kernel_virt_hint addr: %p\n", &s_kernel_virt_hint);
    klog_info("[VMM] s_kernel_virt_hint value: %p\n", (void*)s_kernel_virt_hint);
    klog_info("[VMM] s_kernel_pml4: %p\n", (void*)s_kernel_pml4);
    klog_info("[VMM] s_initialized: %p\n", (void*)s_initialized);
    klog_info("[VMM] s_region_count: %p\n", (void*)(uint64_t)s_region_count);
    klog_info("[VMM] Dump nearby bytes:\n");
    uint8_t* ptr = (uint8_t*)&s_kernel_virt_hint;
    for (int i = -16; i < 32; i++) {
        klog_info("[VMM]   [%+2d] %02X\n", i, ptr[i]);
    }

    s_initialized = true;
    klog_info("[VMM] Initialization complete\n");
    /* Use %p instead of %016llX to avoid potential formatting issues */
    klog_info("[VMM] General page allocation starts at %p\n", (void*)s_kernel_virt_hint);

    /* Dump the memory map for debugging */
    // vmm_dump_memory_map(0, 64);

    return VMM_OK;
}

physical_addr_t vmm_get_kernel_pml4(void) {
    CCOS_ASSERT(s_initialized);
    return s_kernel_pml4;
}

physical_addr_t vmm_get_current_pml4(void) {
    /* Read CR3 to get current PML4 physical address */
    uint64_t cr3 = page_get_cr3();
    /* Clear lower 12 bits (flags) and 4KB alignment requirement */
    return cr3 & ~0xFFFULL;
}

void vmm_load_pml4(physical_addr_t pml4_phys) {
    /* Load PML4 into CR3, switching address space */
    /* Clear lower 12 bits to ensure alignment */
    uint64_t cr3 = pml4_phys & ~0xFFFULL;
    klog_trace("[VMM] Loading CR3: 0x%016llX (from pml4_phys=0x%016llX)\n", cr3, pml4_phys);
    page_set_cr3(cr3);
}

virtual_addr_t vmm_map_physical(physical_addr_t phys, uint64_t flags) {
    CCOS_ASSERT(s_initialized);

    spinlock_flags_t flags_lock;
    spin_lock_irqsave(&s_vmm_lock, &flags_lock);

    /* Find a free virtual address */
    virtual_addr_t vaddr;
    vmm_result_t result = find_free_virt_range(PAGE_SIZE, &vaddr);
    if (result != VMM_OK) {
        spin_unlock_irqrestore(&s_vmm_lock, flags_lock);
        return 0;
    }

    spin_unlock_irqrestore(&s_vmm_lock, flags_lock);

    /* Map the page */
    page_result_t page_result =
        page_map_page(s_kernel_pml4, vaddr, phys, flags | VMAP_FLAG_WRITE, true);
    if (page_result != PAGE_OK) {
        klog_error("[VMM] Failed to map physical page 0x%X\n", phys);
        return 0;
    }

    /* Re-acquire lock for region tracking and stats update */
    spin_lock_irqsave(&s_vmm_lock, &flags_lock);

    /* Track the region */
    add_region(vaddr, vaddr + PAGE_SIZE, phys, flags, "temp_mapping");

    /* Update statistics */
    s_stats.mapped_pages++;
    s_stats.kernel_pages++;

    spin_unlock_irqrestore(&s_vmm_lock, flags_lock);

    return vaddr;
}

vmm_result_t vmm_unmap_physical(virtual_addr_t virt) {
    CCOS_ASSERT(s_initialized);

    spinlock_flags_t flags_lock;
    spin_lock_irqsave(&s_vmm_lock, &flags_lock);

    /* Find and remove the region */
    memory_region_t* region = find_region(virt);
    if (region == NULL) {
        spin_unlock_irqrestore(&s_vmm_lock, flags_lock);
        klog_warn("[VMM] Attempted to unmap untracked region at 0x%llX\n", virt);
        return VMM_ERR_NOT_MAPPED;
    }

    spin_unlock_irqrestore(&s_vmm_lock, flags_lock);

    /* Unmap the page */
    page_result_t result = page_unmap_page(s_kernel_pml4, virt, false);
    if (result != PAGE_OK) {
        return VMM_ERR_INVALID;
    }

    /* Re-acquire lock for stats update */
    spin_lock_irqsave(&s_vmm_lock, &flags_lock);

    /* Update statistics */
    s_stats.mapped_pages--;
    s_stats.kernel_pages--;

    spin_unlock_irqrestore(&s_vmm_lock, flags_lock);

    return VMM_OK;
}

virtual_addr_t vmm_alloc_pages(uint64_t count, uint64_t flags) {
    CCOS_ASSERT(s_initialized);

    if (count == 0) {
        return 0;
    }

    /* Determine page size from flags */
    uint64_t page_size = PAGE_SIZE;
    const char* page_type = "4KB";

    if (flags & VMAP_FLAG_HUGE_1GB) {
        page_size = PAGE_SIZE_1GB;
        page_type = "1GB";
    } else if (flags & VMAP_FLAG_HUGE_2MB) {
        page_size = PAGE_SIZE_2MB;
        page_type = "2MB";
    }

    spinlock_flags_t flags_lock;
    spin_lock_irqsave(&s_vmm_lock, &flags_lock);

    /* Find a free virtual address range */
    virtual_addr_t vaddr;
    vmm_result_t result = find_free_virt_range(count * page_size, &vaddr);
    if (result != VMM_OK) {
        spin_unlock_irqrestore(&s_vmm_lock, flags_lock);
        return 0;
    }

    spin_unlock_irqrestore(&s_vmm_lock, flags_lock);

    /* Allocate and map each page */
    for (uint64_t i = 0; i < count; i++) {
        physical_addr_t paddr;
        pframe_result_t pf_result;

        /* For huge pages, we need to allocate contiguous physical frames with proper alignment */
        uint64_t frames_needed = page_size / PAGE_SIZE;

        if (frames_needed > 1) {
            /* For huge pages, allocate with alignment retry logic
             * We allocate 2x the frames needed to ensure we can find an aligned region
             * For 2MB page (512 frames), we allocate 1024 frames to find 512-aligned boundary
             */
            uint64_t alloc_frames = frames_needed * 2;
            physical_addr_t temp_paddr;
            bool found_aligned = false;
            int retry_count = 0;
            const int max_retries = 10;

            while (!found_aligned && retry_count < max_retries) {
                pf_result = pframe_alloc_n(&temp_paddr, alloc_frames);
                if (pf_result != PFRAME_OK) {
                    /* Try with just the required frames as fallback */
                    pf_result = pframe_alloc_n(&temp_paddr, frames_needed);
                    if (pf_result != PFRAME_OK) {
                        break;
                    }
                }

                /* Find aligned address within the allocated range */
                paddr = (temp_paddr + page_size - 1) & ~(page_size - 1);

                /* Check if aligned address fits within our allocation */
                if (paddr + page_size <= temp_paddr + (alloc_frames * PAGE_SIZE)) {
                    found_aligned = true;
                    /* Free the unused frames before the aligned address */
                    physical_addr_t unused_start = temp_paddr;
                    uint64_t unused_count = (paddr - temp_paddr) / PAGE_SIZE;
                    if (unused_count > 0) {
                        pframe_free_n(unused_start, unused_count);
                    }
                    /* Free the unused frames after the aligned region */
                    physical_addr_t unused_end = paddr + page_size;
                    uint64_t unused_end_count = alloc_frames - unused_count - frames_needed;
                    if (unused_end_count > 0) {
                        pframe_free_n(unused_end, unused_end_count);
                    }
                } else {
                    /* Not enough space, free and retry */
                    pframe_free_n(temp_paddr, alloc_frames);
                    retry_count++;
                }
            }

            if (!found_aligned) {
                klog_error("[VMM] Failed to allocate aligned %s page after %d retries\n", page_type,
                           retry_count);
                vmm_free_pages(vaddr, i);
                return 0;
            }
        } else {
            pf_result = pframe_alloc(&paddr);
        }

        if (pf_result != PFRAME_OK) {
            /* Out of memory - rollback what we've allocated */
            klog_error("[VMM] Out of memory allocating %s pages\n", page_type);
            vmm_free_pages(vaddr, i);
            return 0;
        }

        page_result_t page_result = page_map_page(s_kernel_pml4, vaddr + (i * page_size), paddr,
                                                  flags | VMAP_FLAG_WRITE, true);
        if (page_result != PAGE_OK) {
            /* Rollback */
            if (frames_needed > 1) {
                pframe_free_n(paddr, frames_needed);
            } else {
                pframe_free(paddr);
            }
            vmm_free_pages(vaddr, i);
            return 0;
        }
    }

    /* Re-acquire lock for region tracking and stats update */
    spin_lock_irqsave(&s_vmm_lock, &flags_lock);

    /* Track the region */
    add_region(vaddr, vaddr + (count * page_size), 0, flags, "allocated_pages");

    /* Update statistics */
    s_stats.mapped_pages += count;
    s_stats.kernel_pages += count;
    s_stats.total_pages += count;

    spin_unlock_irqrestore(&s_vmm_lock, flags_lock);

    klog_trace("[VMM] Allocated %llu %s pages at 0x%016llX\n", count, page_type, vaddr);

    /* Perform a full TLB flush to ensure all mappings are active. */
    uint64_t current_cr3 = page_get_cr3();
    __asm__ volatile("mov %0, %%cr3" : : "r"(current_cr3) : "memory");

    return vaddr;
}

vmm_result_t vmm_alloc_pages_at(virtual_addr_t vaddr, uint64_t count, uint64_t flags) {
    CCOS_ASSERT(s_initialized);

    if (count == 0) {
        return VMM_ERR_INVALID;
    }

    /* Determine page size from flags */
    uint64_t page_size = PAGE_SIZE;
    const char* page_type = "4KB";

    if (flags & VMAP_FLAG_HUGE_1GB) {
        page_size = PAGE_SIZE_1GB;
        page_type = "1GB";
    } else if (flags & VMAP_FLAG_HUGE_2MB) {
        page_size = PAGE_SIZE_2MB;
        page_type = "2MB";
    }

    /* Validate address is page-aligned */
    if ((vaddr & (page_size - 1)) != 0) {
        klog_error("[VMM] Address 0x%llX not aligned to %s\n", vaddr, page_type);
        return VMM_ERR_INVALID;
    }

    /* Check if address is in kernel space */
    if (!vmm_is_kernel_addr(vaddr)) {
        klog_error("[VMM] Address 0x%llX is not in kernel space\n", vaddr);
        return VMM_ERR_INVALID;
    }

    /* Acquire VMM lock early to protect the entire allocation and mapping process.
     * This prevents race conditions where multiple threads/interrupts try to
     * allocate or map pages concurrently, which could corrupt page tables. */
    spinlock_flags_t flags_lock;
    spin_lock_irqsave(&s_vmm_lock, &flags_lock);

    /* Allocate and map each page */
    for (uint64_t i = 0; i < count; i++) {
        physical_addr_t paddr;
        pframe_result_t pf_result;

        /* For huge pages, we need to allocate contiguous physical frames with proper alignment */
        uint64_t frames_needed = page_size / PAGE_SIZE;

        if (frames_needed > 1) {
            /* For huge pages, allocate with alignment retry logic */
            uint64_t alloc_frames = frames_needed * 2;
            physical_addr_t temp_paddr;
            bool found_aligned = false;
            int retry_count = 0;
            const int max_retries = 10;

            while (!found_aligned && retry_count < max_retries) {
                pf_result = pframe_alloc_n(&temp_paddr, alloc_frames);
                if (pf_result != PFRAME_OK) {
                    /* Try with just the required frames as fallback */
                    pf_result = pframe_alloc_n(&temp_paddr, frames_needed);
                    if (pf_result != PFRAME_OK) {
                        break;
                    }
                }

                /* Find aligned address within the allocated range */
                paddr = (temp_paddr + page_size - 1) & ~(page_size - 1);

                /* Check if aligned address fits within our allocation */
                if (paddr + page_size <= temp_paddr + (alloc_frames * PAGE_SIZE)) {
                    found_aligned = true;
                    /* Free the unused frames before the aligned address */
                    physical_addr_t unused_start = temp_paddr;
                    uint64_t unused_count = (paddr - temp_paddr) / PAGE_SIZE;
                    if (unused_count > 0) {
                        pframe_free_n(unused_start, unused_count);
                    }
                    /* Free the unused frames after the aligned region */
                    physical_addr_t unused_end = paddr + page_size;
                    uint64_t unused_end_count = alloc_frames - unused_count - frames_needed;
                    if (unused_end_count > 0) {
                        pframe_free_n(unused_end, unused_end_count);
                    }
                } else {
                    /* Not enough space, free and retry */
                    pframe_free_n(temp_paddr, alloc_frames);
                    retry_count++;
                }
            }

            if (!found_aligned) {
                klog_error("[VMM] Failed to allocate aligned %s page after %d retries\n", page_type,
                           retry_count);
                /* Rollback and unlock */
                spin_unlock_irqrestore(&s_vmm_lock, flags_lock);
                vmm_free_pages(vaddr, i);
                return VMM_ERR_OOM;
            }
        } else {
            pf_result = pframe_alloc(&paddr);
        }

        if (pf_result != PFRAME_OK) {
            /* Out of memory - rollback */
            klog_error("[VMM] Out of memory allocating %s pages\n", page_type);
            spin_unlock_irqrestore(&s_vmm_lock, flags_lock);
            vmm_free_pages(vaddr, i);
            return VMM_ERR_OOM;
        }

        page_result_t page_result = page_map_page(s_kernel_pml4, vaddr + (i * page_size), paddr,
                                                  flags | VMAP_FLAG_WRITE, true);
        if (page_result != PAGE_OK) {
            /* Rollback */
            if (frames_needed > 1) {
                pframe_free_n(paddr, frames_needed);
            } else {
                pframe_free(paddr);
            }
            spin_unlock_irqrestore(&s_vmm_lock, flags_lock);
            vmm_free_pages(vaddr, i);
            return VMM_ERR_OOM;
        }
    }

    /* Track the region (still holding lock) */
    add_region(vaddr, vaddr + (count * page_size), 0, flags, "allocated_pages");

    /* Update allocation hint to avoid allocating in this range again */
    virtual_addr_t region_end = vaddr + (count * page_size);
    if (s_kernel_virt_hint < region_end) {
        s_kernel_virt_hint = region_end;
    }

    /* Update statistics */
    s_stats.mapped_pages += count;
    s_stats.kernel_pages += count;
    s_stats.total_pages += count;

    spin_unlock_irqrestore(&s_vmm_lock, flags_lock);

    klog_trace("[VMM] Allocated %llu %s pages at 0x%016llX\n", count, page_type, vaddr);

    /* Perform a full TLB flush to ensure all mappings are active.
     * This is necessary because INVLPG only invalidates individual entries,
     * and there might be timing issues with concurrent accesses. */
    uint64_t current_cr3 = page_get_cr3();
    __asm__ volatile("mov %0, %%cr3" : : "r"(current_cr3) : "memory");

    return VMM_OK;
}

vmm_result_t vmm_free_pages(virtual_addr_t virt, uint64_t count) {
    CCOS_ASSERT(s_initialized);

    if (count == 0) {
        return VMM_ERR_INVALID;
    }

    /* Validate address is page-aligned */
    if ((virt & (PAGE_SIZE - 1)) != 0) {
        return VMM_ERR_INVALID;
    }

    /* Acquire VMM lock to protect the entire free operation.
     * This prevents race conditions with concurrent allocations. */
    spinlock_flags_t flags_lock;
    spin_lock_irqsave(&s_vmm_lock, &flags_lock);

    /* Unmap and free each page */
    for (uint64_t i = 0; i < count; i++) {
        virtual_addr_t vaddr = virt + (i * PAGE_SIZE);

        /* Get physical address */
        physical_addr_t paddr;
        page_result_t result = page_virt_to_phys(s_kernel_pml4, vaddr, &paddr);

        if (result == PAGE_OK) {
            /* Free the physical frame */
            pframe_free(paddr);
        }

        /* Unmap the page */
        page_unmap_page(s_kernel_pml4, vaddr, false);
    }

    /* Update statistics (still holding lock) */
    s_stats.mapped_pages -= count;
    s_stats.kernel_pages -= count;

    spin_unlock_irqrestore(&s_vmm_lock, flags_lock);

    klog_trace("[VMM] Freed %llu pages at 0x%016llX\n", count, virt);

    return VMM_OK;
}

vmm_result_t vmm_get_stats(vmm_stats_t* stats) {
    CCOS_ASSERT(s_initialized);
    CCOS_ASSERT(stats != NULL);

    spinlock_flags_t flags_lock;
    spin_lock_irqsave(&s_vmm_lock, &flags_lock);

    *stats = s_stats;

    spin_unlock_irqrestore(&s_vmm_lock, flags_lock);

    return VMM_OK;
}

void vmm_dump(void) {
    spinlock_flags_t flags_lock;
    spin_lock_irqsave(&s_vmm_lock, &flags_lock);

    if (!s_initialized) {
        spin_unlock_irqrestore(&s_vmm_lock, flags_lock);
        klog_error("[VMM] Not initialized\n");
        return;
    }

    klog_info("[VMM] Virtual Memory Manager State:\n");
    klog_info("[VMM]   Kernel PML4:     0x%016llX\n", s_kernel_pml4);
    klog_info("[VMM]   Total pages:     %llu\n", s_stats.total_pages);
    klog_info("[VMM]   Mapped pages:    %llu\n", s_stats.mapped_pages);
    klog_info("[VMM]   Kernel pages:    %llu\n", s_stats.kernel_pages);
    klog_info("[VMM]   User pages:      %llu\n", s_stats.user_pages);
    klog_info("[VMM]   Page tables:     %llu\n", s_stats.page_tables);
    klog_info("[VMM]   Tracked regions: %u\n", s_region_count);

    /* Dump regions */
    for (uint32_t i = 0; i < s_region_count; i++) {
        memory_region_t* region = &s_regions[i];
        klog_info("[VMM]   Region %u: 0x%016llX - 0x%016llX %s\n", i, region->start, region->end,
                  region->name);
    }

    spin_unlock_irqrestore(&s_vmm_lock, flags_lock);
}

vmm_result_t vmm_create_user_space(physical_addr_t* out_pml4) {
    CCOS_ASSERT(s_initialized);
    CCOS_ASSERT(out_pml4 != NULL);

    /* Allocate a new PML4 */
    page_result_t result = page_create_table(out_pml4);
    if (result != PAGE_OK) {
        return VMM_ERR_OOM;
    }

    /* Copy kernel mappings from the kernel's PML4 */
    /* In x86_64, the upper half of the address space is shared */
    pml4_t* kernel_pml4_virt = (pml4_t*)phys_to_virt_offset(s_kernel_pml4);
    pml4_t* user_pml4_virt = (pml4_t*)phys_to_virt_offset(*out_pml4);

    /* Copy entries 256-511 (kernel space) from kernel PML4 to user PML4 */
    for (int i = 256; i < 512; i++) {
        user_pml4_virt->entries[i] = kernel_pml4_virt->entries[i];
    }

    /* Update statistics */
    spinlock_flags_t flags_lock;
    spin_lock_irqsave(&s_vmm_lock, &flags_lock);
    s_stats.page_tables++;
    spin_unlock_irqrestore(&s_vmm_lock, flags_lock);

    klog_trace("[VMM] Created user address space with PML4 at 0x%X\n", *out_pml4);

    return VMM_OK;
}

vmm_result_t vmm_destroy_user_space(physical_addr_t pml4) {
    CCOS_ASSERT(s_initialized);

    klog_trace("[VMM] vmm_destroy_user_space: entry, pml4=0x%X\n", pml4);

    /* Walk the page tables and free user-space entries (0-255) */
    pml4_t* user_pml4 = (pml4_t*)phys_to_virt_offset(pml4);

    for (int i = 0; i < 256; i++) {
        page_table_entry_t* pml4e = &user_pml4->entries[i];

        if (pml4e->bits.present) {
            /* Free the PDPT */
            pdpt_t* pdpt = (pdpt_t*)phys_to_virt_offset(pml4e->bits.frame << PAGE_SHIFT);

            for (int j = 0; j < 512; j++) {
                page_table_entry_t* pdpte = &pdpt->entries[j];

                if (pdpte->bits.present) {
                    /* Free the PD */
                    pd_t* pd = (pd_t*)phys_to_virt_offset(pdpte->bits.frame << PAGE_SHIFT);

                    for (int k = 0; k < 512; k++) {
                        page_table_entry_t* pde = &pd->entries[k];

                        if (pde->bits.present) {
                            /* Check for huge page */
                            if (pde->bits.pat) {
                                /* NOTE: We don't free data pages here - they're owned
                                 * by the process/memory manager, not the VMM. */
                                /* physical_addr_t phys = pde->bits.frame << PAGE_SHIFT; */
                                /* pframe_free_n(phys, 512); */
                            } else {
                                /* Free the PT */
                                pt_t* pt =
                                    (pt_t*)phys_to_virt_offset(pde->bits.frame << PAGE_SHIFT);

                                for (int l = 0; l < 512; l++) {
                                    page_table_entry_t* pte = &pt->entries[l];

                                    if (pte->bits.present) {
                                        /* NOTE: We don't free data pages here - they're owned
                                         * by the process/memory manager, not the VMM. In a real
                                         * system, the process cleanup would free these separately.
                                         * The VMM only frees the page table structures. */
                                        /* physical_addr_t phys = pte->bits.frame << PAGE_SHIFT; */
                                        /* pframe_free(phys); */
                                    }
                                }

                                /* Free the PT itself */
                                page_free_table(pde->bits.frame << PAGE_SHIFT);
                            }
                        }
                    }

                    /* Free the PD itself */
                    page_free_table(pdpte->bits.frame << PAGE_SHIFT);
                }
            }

            /* Free the PDPT itself */
            page_free_table(pml4e->bits.frame << PAGE_SHIFT);
        }
    }

    /* Finally, free the PML4 itself */
    page_free_table(pml4);

    /* Update statistics */
    spinlock_flags_t flags_lock;
    spin_lock_irqsave(&s_vmm_lock, &flags_lock);
    s_stats.page_tables--;
    spin_unlock_irqrestore(&s_vmm_lock, flags_lock);

    klog_trace("[VMM] Destroyed user address space with PML4 at 0x%X\n", pml4);

    return VMM_OK;
}

vmm_result_t vmm_map_to_user(physical_addr_t pml4, virtual_addr_t vaddr, physical_addr_t paddr,
                             uint64_t count, uint64_t flags) {
    CCOS_ASSERT(s_initialized);

    /* Validate user address */
    if (!vmm_is_user_addr(vaddr)) {
        klog_error("[VMM] Address 0x%llX is not in user space\n", vaddr);
        return VMM_ERR_INVALID;
    }

    /* Ensure USER flag is set */
    flags |= VMAP_FLAG_USER;

    /* Map pages */
    for (uint64_t i = 0; i < count; i++) {
        page_result_t result =
            page_map_page(pml4, vaddr + (i * PAGE_SIZE), paddr + (i * PAGE_SIZE), flags, true);
        if (result != PAGE_OK) {
            /* Rollback */
            page_unmap_range(pml4, vaddr, i);
            return VMM_ERR_OOM;
        }
    }

    /* Update statistics */
    spinlock_flags_t flags_lock;
    spin_lock_irqsave(&s_vmm_lock, &flags_lock);
    s_stats.mapped_pages += count;
    s_stats.user_pages += count;
    spin_unlock_irqrestore(&s_vmm_lock, flags_lock);

    return VMM_OK;
}
