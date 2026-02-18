/* ==============================================================================
 * CCOS - Copy-on-Write (COW) Implementation
 * ==============================================================================
 */

#include "mm/vmm/cow.h"
#include "mm/vmm/vmm.h"
#include "mm/vmm/page.h"
#include "mm/pframe/pframe.h"
#include "mm/heap/heap.h"
#include "base/hashmap.h"
#include "base/memory.h"
#include "klogs/kprintf.h"
#include "assert/assert.h"

/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief Global COW state
 */
static struct {
    hashmap_t*    page_map;       /* cow_block_t* -> cow_block_t (identity map) */
    cow_region_t* regions;        /* List of COW regions */
    cow_stats_t   stats;
    bool          initialized;
} s_cow_state = {
    .page_map = NULL,
    .regions = NULL,
    .stats = {0},
    .initialized = false
};

/* ============================================================================
 * Hash Function for cow_block_t pointers
 * ============================================================================ */

/**
 * @brief Hash function for cow_block_t pointers (identity hash)
 * We use hash_ptr which directly hashes the pointer value
 */
static size_t hash_cow_block_ptr(const void* key) {
    return hash_ptr(key);
}

/**
 * @brief Equality function for cow_block_t pointers (pointer equality)
 */
static bool eq_cow_block_ptr(const void* a, const void* b) {
    return a == b;
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Context for physical address search during iteration
 */
static struct search_ctx {
    physical_addr_t target;
    cow_block_t* result;
} s_search_ctx;

/**
 * @brief Iterator callback for searching by physical address
 */
static bool search_by_phys_iterator(const void* key, void* value, void* context) {
    (void)key;
    struct search_ctx* ctx = (struct search_ctx*)context;
    cow_block_t* block = (cow_block_t*)value;
    if (block->orig_phys == ctx->target) {
        ctx->result = block;
        return false;  /* Stop iteration */
    }
    return true;  /* Continue iteration */
}

/**
 * @brief Look up a COW entry by physical address
 *
 * Since we use pointer-based hashing, we need to iterate through
 * all entries to find one matching the physical address.
 */
static cow_block_t* find_block_by_phys(physical_addr_t phys) {
    if (!s_cow_state.initialized || s_cow_state.page_map == NULL) {
        return NULL;
    }

    phys = phys & ~(PAGE_SIZE - 1);  /* Align to page boundary */

    s_search_ctx.target = phys;
    s_search_ctx.result = NULL;

    hashmap_foreach(s_cow_state.page_map, search_by_phys_iterator, &s_search_ctx);

    return s_search_ctx.result;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

cow_result_t cow_init(void) {
    if (s_cow_state.initialized) {
        klog_warn("[COW] Already initialized\n");
        return COW_OK;
    }

    klog_info("[COW] Initializing Copy-on-Write subsystem...\n");

    /* Create hash map for tracking COW pages using pointer-based hashing */
    s_cow_state.page_map = hashmap_create(COW_HASH_SIZE,
                                         hash_cow_block_ptr,
                                         eq_cow_block_ptr);
    if (s_cow_state.page_map == NULL) {
        klog_error("[COW] Failed to create hash map\n");
        return COW_ERR_OOM;
    }

    /* Initialize stats */
    memset(&s_cow_state.stats, 0, sizeof(cow_stats_t));

    /* Initialize region list */
    s_cow_state.regions = NULL;

    s_cow_state.initialized = true;

    klog_info("[COW] Initialized with %d buckets\n", COW_HASH_SIZE);
    return COW_OK;
}

bool cow_is_initialized(void) {
    return s_cow_state.initialized;
}

cow_result_t cow_get_stats(cow_stats_t* stats) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    if (stats == NULL) {
        return COW_ERR_INVALID;
    }

    *stats = s_cow_state.stats;
    return COW_OK;
}

/* ============================================================================
 * COW Page Management
 * ============================================================================ */

cow_result_t cow_add_page(physical_addr_t phys) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    /* Align to page boundary */
    phys = phys & ~(PAGE_SIZE - 1);

    /* Check if already tracked */
    cow_block_t* existing = find_block_by_phys(phys);
    if (existing != NULL) {
        /* Already in COW tracking, just increment refcount */
        return cow_inc_refcount(phys);
    }

    /* Create new COW block */
    cow_block_t* block = (cow_block_t*)kmalloc(sizeof(cow_block_t));
    if (block == NULL) {
        return COW_ERR_OOM;
    }

    block->orig_phys = phys;
    block->refcount = 1;

    /* Add to hash map using block pointer as both key and value */
    int result = hashmap_put(s_cow_state.page_map, block, block);
    if (result != 0) {
        kfree(block);
        return COW_ERR_OOM;
    }

    s_cow_state.stats.cow_pages_allocated++;
    s_cow_state.stats.cow_current_blocks++;

    return COW_OK;
}

cow_result_t cow_inc_refcount(physical_addr_t phys) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    phys = phys & ~(PAGE_SIZE - 1);

    cow_block_t* block = cow_lookup_block(phys);
    if (block == NULL) {
        return COW_ERR_NOT_COW;
    }

    if (block->refcount >= COW_MAX_REFCOUNT) {
        klog_warn("[COW] Max refcount reached for phys=0x%016llX\n", phys);
        return COW_ERR_MAX_REFCOUNT;
    }

    block->refcount++;
    return COW_OK;
}

cow_result_t cow_dec_refcount(physical_addr_t phys) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    phys = phys & ~(PAGE_SIZE - 1);

    cow_block_t* block = cow_lookup_block(phys);
    if (block == NULL) {
        return COW_ERR_NOT_COW;
    }

    block->refcount--;

    if (block->refcount == 0) {
        /* Remove from tracking */
        hashmap_remove(s_cow_state.page_map, &phys);
        kfree(block);

        s_cow_state.stats.cow_pages_freed++;
        s_cow_state.stats.cow_current_blocks--;
    }

    return COW_OK;
}

cow_result_t cow_get_refcount(physical_addr_t phys, uint16_t* out_refcount) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    if (out_refcount == NULL) {
        return COW_ERR_INVALID;
    }

    phys = phys & ~(PAGE_SIZE - 1);

    cow_block_t* block = cow_lookup_block(phys);
    if (block == NULL) {
        return COW_ERR_NOT_COW;
    }

    *out_refcount = block->refcount;
    return COW_OK;
}

cow_block_t* cow_lookup_block(physical_addr_t phys) {
    return find_block_by_phys(phys);
}

/* ============================================================================
 * COW Region Management
 * ============================================================================ */

cow_result_t cow_register_region(physical_addr_t pml4,
                                 virtual_addr_t base,
                                 size_t size) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    /* Validate alignment */
    if ((base & (PAGE_SIZE - 1)) != 0) {
        klog_error("[COW] Base address not page-aligned: 0x%016llX\n", base);
        return COW_ERR_INVALID;
    }

    if ((size & (PAGE_SIZE - 1)) != 0) {
        klog_error("[COW] Size not page-aligned: %llu\n", size);
        return COW_ERR_INVALID;
    }

    uint64_t page_count = size / PAGE_SIZE;

    klog_debug("[COW] Registering region: 0x%016llX - 0x%016llX (%llu pages)\n",
               base, base + size, page_count);

    /* For each page in the region */
    for (uint64_t i = 0; i < page_count; i++) {
        virtual_addr_t vaddr = base + (i * PAGE_SIZE);

        /* Query current mapping */
        page_query_result_t query;
        page_result_t result = page_query(pml4, vaddr, &query);

        if (result != PAGE_OK || !query.present) {
            /* Skip unmapped pages */
            continue;
        }

        /* Add to COW tracking */
        cow_add_page(query.phys_addr);

        /* Increment refcount (fork scenario: parent + child = refcount 2) */
        cow_inc_refcount(query.phys_addr);

        /* Mark as read-only with COW flag */
        cow_mark_page_readonly(pml4, vaddr);
    }

    /* Create region descriptor */
    cow_region_t* region = (cow_region_t*)kmalloc(sizeof(cow_region_t));
    if (region == NULL) {
        return COW_ERR_OOM;
    }

    region->start = base;
    region->page_count = page_count;
    region->pml4 = pml4;
    region->next = s_cow_state.regions;
    s_cow_state.regions = region;

    return COW_OK;
}

cow_result_t cow_unregister_region(physical_addr_t pml4,
                                   virtual_addr_t base) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    cow_region_t** prev = &s_cow_state.regions;
    cow_region_t* region = s_cow_state.regions;

    while (region != NULL) {
        if (region->pml4 == pml4 && region->start == base) {
            /* Found it - remove from list */
            *prev = region->next;

            /* Decrement refcounts for all pages */
            for (uint64_t i = 0; i < region->page_count; i++) {
                virtual_addr_t vaddr = region->start + (i * PAGE_SIZE);

                page_query_result_t query;
                if (page_query(pml4, vaddr, &query) == PAGE_OK) {
                    cow_dec_refcount(query.phys_addr);
                }
            }

            kfree(region);
            return COW_OK;
        }

        prev = &region->next;
        region = region->next;
    }

    return COW_ERR_NOT_COW;
}

/* ============================================================================
 * COW Page Fault Handling
 * ============================================================================ */

cow_result_t cow_handle_fault(physical_addr_t pml4, virtual_addr_t fault_addr) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    s_cow_state.stats.cow_write_faults++;

    /* Query the current mapping */
    page_query_result_t query;
    page_result_t result = page_query(pml4, fault_addr, &query);

    if (result != PAGE_OK || !query.present) {
        return COW_ERR_NOT_COW;
    }

    /* Check if this is a COW page (read-only, COW flag set) */
    if ((query.flags & PAGE_WRITE) || !cow_is_cow_page(query.flags)) {
        return COW_ERR_NOT_COW;
    }

    /* Look up COW block */
    physical_addr_t orig_phys = query.phys_addr;
    cow_block_t* block = cow_lookup_block(orig_phys);

    if (block == NULL) {
        klog_error("[COW] Page marked COW but not in tracking table!\n");
        klog_error("[COW] This indicates inconsistent state\n");
        return COW_ERR_INVALID;
    }

    /* Handle based on refcount */
    if (block->refcount == 1) {
        /* Exclusive access - just make writable */
        s_cow_state.stats.cow_coalesced++;
        return cow_make_writable(pml4, fault_addr, orig_phys);
    } else {
        /* Shared - need to copy */
        return cow_copy_on_write(pml4, fault_addr, orig_phys, block);
    }
}

cow_result_t cow_mark_page_readonly(physical_addr_t pml4, virtual_addr_t vaddr) {
    /* Query current mapping */
    page_query_result_t query;
    page_result_t result = page_query(pml4, vaddr, &query);

    if (result != PAGE_OK) {
        return COW_ERR_INVALID;
    }

    /* Get the PTE by walking the page tables */
    pml4_t* pml4_virt = (pml4_t*)phys_to_virt_offset(pml4);
    page_table_entry_t* pml4e = &pml4_virt->entries[PML4_INDEX(vaddr)];

    if (!pml4e->bits.present) {
        return COW_ERR_INVALID;
    }

    pdpt_t* pdpt = (pdpt_t*)phys_to_virt_offset(pml4e->bits.frame << PAGE_SHIFT);
    page_table_entry_t* pdpte = &pdpt->entries[PDPT_INDEX(vaddr)];

    if (!pdpte->bits.present) {
        return COW_ERR_INVALID;
    }

    pd_t* pd = (pd_t*)phys_to_virt_offset(pdpte->bits.frame << PAGE_SHIFT);
    page_table_entry_t* pde = &pd->entries[PD_INDEX(vaddr)];

    if (!pde->bits.present) {
        return COW_ERR_INVALID;
    }

    pt_t* pt = (pt_t*)phys_to_virt_offset(pde->bits.frame << PAGE_SHIFT);
    page_table_entry_t* pte = &pt->entries[PT_INDEX(vaddr)];

    /* Remove write permission and set COW flag directly in the PTE */
    pte->value &= ~PAGE_WRITE;           /* Remove write permission */
    pte->value = cow_set_cow_flag(pte->value);  /* Add COW flag */

    /* Invalidate TLB for this page */
    page_invalidate_tlb(vaddr);

    return COW_OK;
}

cow_result_t cow_make_writable(physical_addr_t pml4,
                               virtual_addr_t vaddr,
                               physical_addr_t phys) {
    /* Query current mapping */
    page_query_result_t query;
    page_result_t result = page_query(pml4, vaddr, &query);

    if (result != PAGE_OK) {
        return COW_ERR_INVALID;
    }

    /* Calculate new flags: add write, remove COW flag */
    uint64_t new_flags = query.flags;
    new_flags |= PAGE_WRITE;            /* Add write permission */
    new_flags = cow_clear_cow_flag(new_flags);

    /* Remap with new flags */
    result = page_map_page(pml4, vaddr, phys, new_flags, false);

    /* Invalidate TLB for this page */
    page_invalidate_tlb(vaddr);

    return (result == PAGE_OK) ? COW_OK : COW_ERR_INVALID;
}

cow_result_t cow_copy_on_write(physical_addr_t pml4,
                               virtual_addr_t vaddr,
                               physical_addr_t orig_phys,
                               cow_block_t* block) {
    (void)block;  /* Will be used for future optimizations */

    /* Allocate new physical frame */
    physical_addr_t new_phys;
    pframe_result_t result = pframe_alloc(&new_phys);

    if (result != PFRAME_OK) {
        klog_error("[COW] Failed to allocate new frame for COW\n");
        return COW_ERR_OOM;
    }

    /* Get kernel virtual addresses */
    virtual_addr_t orig_virt = phys_to_virt_offset(orig_phys);
    virtual_addr_t new_virt = phys_to_virt_offset(new_phys);

    /* Copy page content */
    memcpy((void*)new_virt, (const void*)orig_virt, PAGE_SIZE);

    /* Decrement refcount on original page */
    cow_dec_refcount(orig_phys);

    /* Map new page with write permission */
    uint64_t flags = PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    /* Check if original was user page */
    page_query_result_t query;
    if (page_query(pml4, vaddr, &query) == PAGE_OK) {
        if (query.flags & PAGE_USER) {
            flags |= PAGE_USER;
        }
        if (query.flags & PAGE_NO_EXEC) {
            flags |= PAGE_NO_EXEC;
        }
    }

    /* Map the new page */
    page_map_page(pml4, vaddr, new_phys, flags, false);

    /* Invalidate TLB for this page */
    page_invalidate_tlb(vaddr);

    s_cow_state.stats.cow_faults_handled++;

    klog_debug("[COW] Copied page 0x%016llX -> 0x%016llX\n",
               orig_phys, new_phys);

    return COW_OK;
}
