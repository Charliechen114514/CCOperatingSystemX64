/**
 * @file vmm_demo.c
 * @brief VMM Demo - Demonstrates Virtual Memory Management functionality
 */

#include "vmm_demo.h"
#include "klogs/kprintf.h"
#include "mm/pframe/pframe.h"
#include "mm/vmm/fault.h"
#include "mm/vmm/page.h"
#include "mm/vmm/vmm.h"

/* ============================================================================
 * Demo State
 * ============================================================================ */

static virtual_addr_t demo_allocated_pages[16];
static uint64_t demo_allocated_count = 0;
static physical_addr_t demo_user_pml4 = 0;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Helper to print page table entry flags
 */
static void print_pte_flags(uint64_t flags) {
    klog_trace("  Flags: ");
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
    klog_trace("\n");
}

/* ============================================================================
 * Demo Functions
 * ============================================================================ */

/**
 * Demo 1: Display page table and VMM statistics
 */
static void demo_statistics(void) {
    klog_trace("\n=== Demo 1: VMM Statistics ===\n");

    /* Page table stats */
    physical_addr_t pml4 = page_get_pml4();
    klog_trace("Kernel PML4: 0x%016llX\n", pml4);

    /* VMM stats */
    vmm_stats_t stats;
    if (vmm_get_stats(&stats) == VMM_OK) {
        klog_trace("Total virtual pages: %llu\n", stats.total_pages);
        klog_trace("Mapped pages: %llu\n", stats.mapped_pages);
        klog_trace("Kernel pages: %llu\n", stats.kernel_pages);
        klog_trace("User pages: %llu\n", stats.user_pages);
        klog_trace("Page tables allocated: %llu\n", stats.page_tables);
    }

    /* PMM stats */
    pframe_stats_t pframe_stats;
    if (pframe_get_stats(&pframe_stats) == PFRAME_OK) {
        klog_trace("Physical frames:\n");
        klog_trace("  Total: %llu\n", pframe_stats.total_frames);
        klog_trace("  Free: %llu\n", pframe_stats.free_frames);
        klog_trace("  Allocated: %llu\n", pframe_stats.allocated_frames);
    }
}

/**
 * Demo 2: Test address translation
 */
static void demo_address_translation(void) {
    klog_trace("\n=== Demo 2: Address Translation Tests ===\n");

    /* Test some known kernel addresses */
    virtual_addr_t test_addrs[] = {
        KERNEL_TEXT_BASE,                /* Kernel code base */
        (virtual_addr_t)demo_statistics, /* Current function */
    };

    for (int i = 0; i < 2; i++) {
        virtual_addr_t vaddr = test_addrs[i];
        physical_addr_t paddr;

        klog_trace("Virtual:  0x%016llX\n", vaddr);

        page_result_t result = page_virt_to_phys(page_get_pml4(), vaddr, &paddr);
        if (result == PAGE_OK) {
            klog_trace("Physical: 0x%016llX\n", paddr);
        } else {
            klog_trace("Translation: FAILED (not mapped)\n");
        }

        /* Also try query for more details */
        page_query_result_t query;
        result = page_query(page_get_pml4(), vaddr, &query);
        if (result == PAGE_OK) {
            klog_trace("Present: %s, Huge: %s\n", query.present ? "yes" : "no",
                       query.is_huge ? "yes" : "no");
            print_pte_flags(query.flags);
        }
        klog_trace("\n");
    }
}

/**
 * Demo 3: Allocate and map virtual pages
 */
static void demo_page_allocation(void) {
    klog_trace("\n=== Demo 3: Page Allocation ===\n");

    /* Allocate 4 pages */
    uint64_t count = 4;
    klog_trace("Allocating %llu pages (%llu KB)...\n", count, count * 4);

    virtual_addr_t vaddr = vmm_alloc_pages(count, VMAP_FLAG_WRITE);
    if (vaddr == 0) {
        klog_error("Failed to allocate pages!\n");
        return;
    }

    klog_trace("Allocated at virtual address: 0x%016llX\n", vaddr);

    /* Store for cleanup */
    if (demo_allocated_count < 16) {
        demo_allocated_pages[demo_allocated_count++] = vaddr;
    }

    /* Write a pattern to verify the mapping works */
    klog_trace("Testing write access...\n");
    volatile uint64_t* ptr = (volatile uint64_t*)vaddr;
    for (uint64_t i = 0; i < count * (PAGE_SIZE / sizeof(uint64_t)); i++) {
        ptr[i] = 0xDEADBEEF01234567ULL + i;
    }
    klog_trace("Write test passed!\n");

    /* Read back and verify */
    klog_trace("Testing read access...\n");
    bool success = true;
    for (uint64_t i = 0; i < count * (PAGE_SIZE / sizeof(uint64_t)); i++) {
        uint64_t expected = 0xDEADBEEF01234567ULL + i;
        if (ptr[i] != expected) {
            klog_error("Mismatch at index %llu: got 0x%llX, expected 0x%llX\n", i, ptr[i],
                       expected);
            success = false;
            break;
        }
    }
    if (success) {
        klog_trace("Read test passed!\n");
    }

    /* Query the mapping details */
    page_query_result_t query;
    if (page_query(page_get_pml4(), vaddr, &query) == PAGE_OK) {
        klog_trace("Mapping details:\n");
        klog_trace("  Physical: 0x%016llX\n", query.phys_addr);
        print_pte_flags(query.flags);
    }
}

/**
 * Demo 4: Create user address space
 */
static void demo_user_space(void) {
    klog_trace("\n=== Demo 4: User Address Space ===\n");

    klog_trace("Creating new user address space...\n");

    vmm_result_t result = vmm_create_user_space(&demo_user_pml4);
    if (result != VMM_OK) {
        klog_error("Failed to create user address space!\n");
        return;
    }

    klog_trace("User PML4 created: 0x%016llX\n", demo_user_pml4);

    /* Map a page into user space */
    physical_addr_t paddr;
    if (pframe_alloc(&paddr) == PFRAME_OK) {
        virtual_addr_t user_vaddr = 0x400000; /* 4MB */

        klog_trace("Mapping page to user space 0x%016llX...\n", user_vaddr);

        result =
            vmm_map_to_user(demo_user_pml4, user_vaddr, paddr, 1, VMAP_FLAG_WRITE | VMAP_FLAG_USER);
        if (result == VMM_OK) {
            klog_trace("User space mapping successful!\n");

            /* Verify the mapping */
            page_query_result_t query;
            if (page_query(demo_user_pml4, user_vaddr, &query) == PAGE_OK) {
                klog_trace("  Physical: 0x%016llX\n", query.phys_addr);
                klog_trace("  User accessible: %s\n", (query.flags & PAGE_USER) ? "yes" : "no");
            }
        } else {
            klog_error("Failed to map to user space!\n");
            pframe_free(paddr);
        }
    }
}

/**
 * Demo 5: Map physical memory
 */
static void demo_physical_mapping(void) {
    klog_trace("\n=== Demo 5: Physical Memory Mapping ===\n");

    /* Map VGA memory as an example */
    physical_addr_t vga_phys = 0xB8000;
    klog_trace("Mapping VGA memory (0x%X) to kernel space...\n", vga_phys);

    virtual_addr_t vaddr = vmm_map_physical(vga_phys, VMAP_FLAG_WRITE);
    if (vaddr == 0) {
        klog_error("Failed to map physical memory!\n");
        return;
    }

    klog_trace("Mapped to virtual address: 0x%016llX\n", vaddr);

    /* Try to write to VGA through the mapped address */
    volatile uint16_t* vga_ptr = (volatile uint16_t*)vaddr;

    /* Save original character */
    uint16_t original = vga_ptr[0];

    /* Write a test character (green 'A') */
    vga_ptr[0] = (uint16_t)'A' | 0x0200; /* green on black */
    klog_trace("Wrote test character to VGA\n");

    /* Restore original */
    vga_ptr[0] = original;

    /* Unmap */
    vmm_unmap_physical(vaddr);
    klog_trace("Mapping unmapped\n");
}

/**
 * Demo 6: Huge page mapping test
 */
static void demo_huge_pages(void) {
    klog_trace("\n=== Demo 6: Huge Page Mapping ===\n");

    /* Test 2MB huge page allocation */
    klog_trace("Testing 2MB huge page allocation...\n");

    /* Allocate 2 2MB pages (4MB total) */
    uint64_t count_2mb = 2;
    virtual_addr_t vaddr_2mb = vmm_alloc_pages(count_2mb, VMAP_FLAG_WRITE | VMAP_FLAG_HUGE_2MB);

    if (vaddr_2mb == 0) {
        klog_error("Failed to allocate 2MB huge pages!\n");
        klog_trace("This might be due to:\n");
        klog_trace("  - Insufficient contiguous physical memory\n");
        klog_trace("  - pframe_alloc_n not implemented\n");
        return;
    }

    klog_trace("Allocated %llu 2MB pages at: 0x%016llX\n", count_2mb, vaddr_2mb);
    klog_trace("Total size: %llu MB\n", count_2mb * 2);

    /* Store for cleanup */
    if (demo_allocated_count < 16) {
        demo_allocated_pages[demo_allocated_count++] = vaddr_2mb;
    }

    /* Write test pattern to first 2MB page */
    klog_trace("Testing write access to 2MB page...\n");
    volatile uint64_t* ptr_2mb = (volatile uint64_t*)vaddr_2mb;

    /* Write at start, middle, and end of first 2MB page */
    uint64_t size_2mb = 2 * 1024 * 1024;
    ptr_2mb[0] = 0x1111111111111111ULL;
    ptr_2mb[size_2mb / sizeof(uint64_t) / 2] = 0x2222222222222222ULL;
    ptr_2mb[size_2mb / sizeof(uint64_t) - 1] = 0x3333333333333333ULL;

    /* Write to second 2MB page */
    ptr_2mb[size_2mb / sizeof(uint64_t)] = 0x4444444444444444ULL;

    klog_trace("Write test passed!\n");

    /* Read back and verify */
    bool success = true;
    if (ptr_2mb[0] != 0x1111111111111111ULL) {
        klog_error("Mismatch at start of first 2MB page\n");
        success = false;
    }
    if (ptr_2mb[size_2mb / sizeof(uint64_t) / 2] != 0x2222222222222222ULL) {
        klog_error("Mismatch at middle of first 2MB page\n");
        success = false;
    }
    if (ptr_2mb[size_2mb / sizeof(uint64_t) - 1] != 0x3333333333333333ULL) {
        klog_error("Mismatch at end of first 2MB page\n");
        success = false;
    }
    if (ptr_2mb[size_2mb / sizeof(uint64_t)] != 0x4444444444444444ULL) {
        klog_error("Mismatch at start of second 2MB page\n");
        success = false;
    }

    if (success) {
        klog_trace("Read test passed!\n");
    }

    /* Query the mapping details to confirm it's a huge page */
    page_query_result_t query;
    if (page_query(page_get_pml4(), vaddr_2mb, &query) == PAGE_OK) {
        klog_trace("Mapping details:\n");
        klog_trace("  Physical: 0x%016llX\n", query.phys_addr);
        klog_trace("  Type: %s\n", query.is_huge ? "Huge Page (2MB)" : "4KB Page");
        print_pte_flags(query.flags);
    }

    /* Compare with 4KB page allocation */
    klog_trace("\nComparing with 4KB page allocation...\n");
    virtual_addr_t vaddr_4kb = vmm_alloc_pages(1, VMAP_FLAG_WRITE);

    if (vaddr_4kb != 0) {
        klog_trace("4KB page allocated at: 0x%016llX\n", vaddr_4kb);

        if (demo_allocated_count < 16) {
            demo_allocated_pages[demo_allocated_count++] = vaddr_4kb;
        }

        if (page_query(page_get_pml4(), vaddr_4kb, &query) == PAGE_OK) {
            klog_trace("  Physical: 0x%016llX\n", query.phys_addr);
            klog_trace("  Type: %s\n", query.is_huge ? "Huge Page" : "4KB Page");
        }
    }
}

/**
 * Demo 7: Page fault test (OPTIONAL - only run if explicitly requested)
 */
static void demo_page_fault_test(void) {
    klog_trace("\n=== Demo 7: Page Fault Test ===\n");
    klog_trace("WARNING: This will intentionally trigger a page fault!\n");
    klog_trace("The page fault handler should catch this.\n");

    klog_trace("Attempting to access unmapped memory at 0xDEADBEEF0000...\n");

    /* Flush any pending log output */
    __asm__ volatile("mfence" ::: "memory");

    /* Intentionally cause a page fault by accessing unmapped memory */
    volatile uint64_t* invalid_ptr = (volatile uint64_t*)0xDEADBEEF0000ULL;
    *invalid_ptr = 0x12345678; /* This will cause a page fault */

    /* Should never reach here */
    klog_error("ERROR: Page fault was not caught! System is in bad state.\n");
}

/* ============================================================================
 * Main Demo Entry Point
 * ============================================================================ */

void vmm_run_demo(bool test_page_fault) {
    klog_trace("\n");
    klog_trace("========================================\n");
    klog_trace("   VMM (Virtual Memory Manager) Demo\n");
    klog_trace("========================================\n");

    /* Demo 1: Show statistics */
    demo_statistics();

    /* Demo 2: Test address translation */
    demo_address_translation();

    /* Demo 3: Allocate pages */
    demo_page_allocation();

    /* Demo 4: Create user address space */
    demo_user_space();

    /* Demo 5: Map physical memory */
    demo_physical_mapping();

    /* Demo 6: Huge page mapping */
    demo_huge_pages();

    /* Demo 7: Page fault test (only if requested) */
    if (test_page_fault) {
        klog_trace("\n");
        klog_trace("*** PAGE FAULT TEST ENABLED ***\n");
        klog_trace("This will intentionally crash the kernel if the handler fails!\n");
        klog_trace("=========================================\n");

        /* Give user time to see the message */
        for (volatile int i = 0; i < 100000000; i++) {
            __asm__ volatile("nop");
        }

        demo_page_fault_test();
    } else {
        klog_trace("\n");
        klog_trace("Note: Page fault test is SKIPPED.\n");
        klog_trace("To enable it, pass 'true' to vmm_run_demo().\n");
    }

    klog_trace("\n=== VMM Demo Complete ===\n");
}

void vmm_stop_demo(void) {
    klog_trace("\n=== VMM Demo Cleanup ===\n");

    /* Free allocated pages */
    for (uint64_t i = 0; i < demo_allocated_count; i++) {
        klog_trace("Freeing pages at 0x%016llX\n", demo_allocated_pages[i]);
        vmm_free_pages(demo_allocated_pages[i], 4);
    }
    demo_allocated_count = 0;

    /* Destroy user address space */
    if (demo_user_pml4 != 0) {
        klog_trace("Destroying user address space...\n");
        vmm_destroy_user_space(demo_user_pml4);
        demo_user_pml4 = 0;
    }

    klog_trace("VMM Demo cleanup complete\n");
}
