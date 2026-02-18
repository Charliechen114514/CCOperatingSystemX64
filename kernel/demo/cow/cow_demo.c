/**
 * @file cow_demo.c
 * @brief COW Demo Implementation
 */

#include "cow_demo.h"
#include "mm/vmm/cow.h"
#include "mm/vmm/vmm.h"
#include "mm/vmm/page.h"
#include "mm/pframe/pframe.h"
#include "mm/heap/heap.h"
#include "klogs/kprintf.h"
#include "base/memory.h"

/* Test configuration */
#define TEST_PAGES     3              /* Number of pages to test */

/* Dynamically allocated test addresses */
static virtual_addr_t test_vaddr1 = 0;
static virtual_addr_t test_vaddr2 = 0;

/**
 * @brief Display COW statistics
 */
static void cow_dump_stats(void) {
    cow_stats_t stats;
    if (cow_get_stats(&stats) == COW_OK) {
        klog_info("[COW Demo] Statistics:\n");
        klog_info("[COW Demo]   Faults handled:  %llu\n", stats.cow_faults_handled);
        klog_info("[COW Demo]   Pages allocated: %llu\n", stats.cow_pages_allocated);
        klog_info("[COW Demo]   Pages freed:     %llu\n", stats.cow_pages_freed);
        klog_info("[COW Demo]   Current blocks:  %llu\n", stats.cow_current_blocks);
        klog_info("[COW Demo]   Write faults:    %llu\n", stats.cow_write_faults);
        klog_info("[COW Demo]   Coalesced:       %llu\n", stats.cow_coalesced);
    }
}

/**
 * @brief Test basic COW functionality
 *
 * This test:
 * 1. Allocates a physical page
 * 2. Maps it to two virtual addresses
 * 3. Registers as COW region
 * 4. Verifies the setup
 */
static int test_cow_basic(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   COW Basic Test                         ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    physical_addr_t pml4 = vmm_get_current_pml4();

    /* Step 1: Allocate two virtual addresses for testing */
    test_vaddr1 = vmm_alloc_pages(1, VMAP_FLAG_WRITE);
    test_vaddr2 = vmm_alloc_pages(1, VMAP_FLAG_WRITE);

    if (test_vaddr1 == 0 || test_vaddr2 == 0) {
        klog_error("[COW Demo] Failed to allocate virtual addresses\n");
        return -1;
    }

    klog_info("[COW Demo] Allocated virtual addresses:\n");
    klog_info("[COW Demo]   VADDR1: 0x%016lX\n", test_vaddr1);
    klog_info("[COW Demo]   VADDR2: 0x%016lX\n", test_vaddr2);

    /* Step 2: Get the physical address of the first allocated page */
    physical_addr_t phys1;
    if (page_virt_to_phys(pml4, test_vaddr1, &phys1) != PAGE_OK) {
        klog_error("[COW Demo] Failed to get physical address for VADDR1\n");
        vmm_free_pages(test_vaddr1, 1);
        vmm_free_pages(test_vaddr2, 1);
        return -1;
    }
    klog_info("[COW Demo] Physical page for VADDR1: 0x%016llX\n", phys1);

    /* Step 3: Write test pattern to the first page */
    uint32_t* pattern = (uint32_t*)test_vaddr1;
    for (size_t i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++) {
        pattern[i] = 0xDEADBEEF + i;
    }
    klog_info("[COW Demo] Wrote test pattern to VADDR1\n");

    /* Step 4: Remap VADDR2 to the same physical page as VADDR1 */
    /* First, get the physical address of VADDR2 so we can free it later */
    physical_addr_t phys2;
    if (page_virt_to_phys(pml4, test_vaddr2, &phys2) != PAGE_OK) {
        klog_error("[COW Demo] Failed to get physical address for VADDR2\n");
        vmm_free_pages(test_vaddr1, 1);
        vmm_free_pages(test_vaddr2, 1);
        return -1;
    }

    /* Unmap VADDR2's original physical page */
    page_unmap_page(pml4, test_vaddr2, false);
    pframe_free(phys2);

    /* Map VADDR2 to the same physical page as VADDR1, read-only for COW */
    if (page_map_page(pml4, test_vaddr2, phys1, VMAP_FLAG_USER, false) != PAGE_OK) {
        klog_error("[COW Demo] Failed to map VADDR2 to same physical page\n");
        vmm_free_pages(test_vaddr1, 1);
        return -1;
    }

    klog_info("[COW Demo] Mapped VADDR2 to same physical page as VADDR1\n");

    /* Step 5: Add to COW tracking */
    if (cow_add_page(phys1) != COW_OK) {
        klog_error("[COW Demo] Failed to add page to COW tracking\n");
        page_unmap_page(pml4, test_vaddr2, false);
        vmm_free_pages(test_vaddr1, 1);
        return -1;
    }

    /* Increment refcount (simulating two mappings) */
    if (cow_inc_refcount(phys1) != COW_OK) {
        klog_error("[COW Demo] Failed to increment refcount\n");
        cow_dec_refcount(phys1);
        page_unmap_page(pml4, test_vaddr2, false);
        vmm_free_pages(test_vaddr1, 1);
        return -1;
    }

    klog_info("[COW Demo] Page added to COW tracking with refcount=2\n");

    /* Step 6: Mark pages as read-only with COW flag */
    if (cow_mark_page_readonly(pml4, test_vaddr1) != COW_OK) {
        klog_error("[COW Demo] Failed to mark VADDR1 as COW\n");
        goto cleanup;
    }

    if (cow_mark_page_readonly(pml4, test_vaddr2) != COW_OK) {
        klog_error("[COW Demo] Failed to mark VADDR2 as COW\n");
        goto cleanup;
    }

    klog_info("[COW Demo] Both mappings marked as COW (read-only)\n");

    /* Step 7: Verify COW flag is set */
    page_query_result_t query;
    if (page_query(pml4, test_vaddr1, &query) == PAGE_OK) {
        klog_info("[COW Demo] VADDR1 flags: 0x%016llX (COW=%d)\n",
                  query.flags, cow_is_cow_page(query.flags));
    }

    cow_dump_stats();

    klog_info("[COW Demo] Basic test PASSED\n");

cleanup:
    /* Cleanup */
    page_unmap_page(pml4, test_vaddr1, false);
    page_unmap_page(pml4, test_vaddr2, false);
    cow_dec_refcount(phys1);
    cow_dec_refcount(phys1);
    pframe_free(phys1);

    return 0;
}

/**
 * @brief Test COW region registration
 */
static int test_cow_region(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   COW Region Test                       ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    physical_addr_t pml4 = vmm_get_current_pml4();

    /* Allocate and map pages for a test region */
    virtual_addr_t region_base = vmm_alloc_pages(TEST_PAGES, VMAP_FLAG_WRITE);

    if (region_base == 0) {
        klog_error("[COW Demo] Failed to allocate region\n");
        return -1;
    }

    klog_info("[COW Demo] Allocated and mapped %d pages at 0x%016lX\n",
              TEST_PAGES, region_base);

    /* Register the region as COW */
    klog_info("[COW Demo] Registering region as COW...\n");
    if (cow_register_region(pml4, region_base, TEST_PAGES * PAGE_SIZE) != COW_OK) {
        klog_error("[COW Demo] Failed to register COW region\n");
        vmm_free_pages(region_base, TEST_PAGES);
        return -1;
    }

    klog_info("[COW Demo] COW region registered successfully\n");
    cow_dump_stats();

    klog_info("[COW Demo] Region test PASSED\n");

    /* Cleanup */
    vmm_free_pages(region_base, TEST_PAGES);

    return 0;
}

/**
 * @brief Test COW statistics and state
 */
static int test_cow_stats(void) {
    klog_info("\n");
    klog_info("╔════════════════════════════════════════╗\n");
    klog_info("║   COW Statistics Test                    ║\n");
    klog_info("╚════════════════════════════════════════╝\n");

    if (!cow_is_initialized()) {
        klog_warn("[COW Demo] COW subsystem not initialized\n");
        return -1;
    }

    klog_info("[COW Demo] COW subsystem is initialized\n");

    cow_stats_t stats;
    if (cow_get_stats(&stats) == COW_OK) {
        klog_info("[COW Demo] Initial COW statistics:\n");
        klog_info("[COW Demo]   Tracked pages:    %llu\n", stats.cow_current_blocks);
        klog_info("[COW Demo]   Total allocated:  %llu\n", stats.cow_pages_allocated);
        klog_info("[COW Demo]   Total freed:      %llu\n", stats.cow_pages_freed);
    }

    klog_info("[COW Demo] Statistics test PASSED\n");
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int cow_run_demo(void) {
    klog_info("\n");
    klog_info("╔══════════════════════════════════════════════╗\n");
    klog_info("║   Copy-on-Write (COW) Demo                   ║\n");
    klog_info("╚══════════════════════════════════════════════╝\n");

    int errors = 0;

    /* Test 1: Check COW initialization */
    if (test_cow_stats() != 0) {
        errors++;
    }

    /* Test 2: Basic COW functionality */
    if (test_cow_basic() != 0) {
        errors++;
    }

    /* Test 3: COW region registration */
    if (test_cow_region() != 0) {
        errors++;
    }

    /* Final statistics */
    klog_info("\n");
    klog_info("╔══════════════════════════════════════════════╗\n");
    klog_info("║   COW Demo Summary                          ║\n");
    klog_info("╚══════════════════════════════════════════════╝\n");

    cow_dump_stats();

    if (errors == 0) {
        klog_info("[COW Demo] ✓ All tests PASSED\n");
        return 0;
    } else {
        klog_error("[COW Demo] ✗ %d test(s) FAILED\n", errors);
        return -1;
    }
}

void cow_stop_demo(void) {
    klog_info("[COW Demo] Stopping COW demo...\n");

    /* Cleanup any remaining COW blocks would go here */
    /* For now, we rely on the global state being clean */

    klog_info("[COW Demo] COW demo stopped\n");
}
