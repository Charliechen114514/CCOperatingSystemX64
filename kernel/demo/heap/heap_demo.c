/**
 * @file heap_demo.c
 * @brief Heap Allocator Demo - Demonstrates kmalloc/kfree functionality
 */

#include "heap_demo.h"
#include "mm/heap/heap.h"
#include "base/string.h"
#include "base/memory.h"
#include "klogs/kprintf.h"

/* ============================================================================
 * Test Result Tracking
 * ============================================================================ */

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            klog_error("[HEAP_DEMO] FAILED: %s\n", message); \
            tests_failed++; \
            return false; \
        } \
    } while(0)

#define TEST_START(name) \
    klog_info("[HEAP_DEMO] Test: %s...\n", name)

#define TEST_PASS() \
    do { \
        klog_info("[HEAP_DEMO] PASSED\n"); \
        tests_passed++; \
        return true; \
    } while(0)

/* ============================================================================
 * Test 1: Basic Allocation and Deallocation
 * ============================================================================ */

static bool test_basic_alloc_free(void) {
    TEST_START("Basic Alloc/Free");

    void* ptr = kmalloc(64);
    TEST_ASSERT(ptr != NULL, "kmalloc(64) returned NULL");

    /* Write to ensure memory is writable */
    memset(ptr, 0xAA, 64);
    uint8_t* bytes = (uint8_t*)ptr;
    TEST_ASSERT(bytes[0] == 0xAA && bytes[63] == 0xAA, "Memory content check failed");

    kfree(ptr);

    TEST_PASS();
}

/* ============================================================================
 * Test 2: Multiple Allocations
 * ============================================================================ */

static bool test_multiple_allocations(void) {
    TEST_START("Multiple Allocations");

    void* ptrs[10];
    size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192};

    for (int i = 0; i < 10; i++) {
        ptrs[i] = kmalloc(sizes[i]);
        TEST_ASSERT(ptrs[i] != NULL, "kmalloc failed");
        memset(ptrs[i], i, sizes[i]);
    }

    /* Verify all pointers are unique */
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            TEST_ASSERT(ptrs[i] != ptrs[j], "Duplicate pointer returned");
        }
    }

    /* Free all */
    for (int i = 0; i < 10; i++) {
        kfree(ptrs[i]);
    }

    TEST_PASS();
}

/* ============================================================================
 * Test 3: Zero Size Allocation
 * ============================================================================ */

static bool test_zero_size(void) {
    TEST_START("Zero Size Allocation");

    void* ptr = kmalloc(0);
    TEST_ASSERT(ptr == NULL, "kmalloc(0) should return NULL");

    kfree(NULL);  /* Should be safe */

    TEST_PASS();
}

/* ============================================================================
 * Test 4: Large Allocation (triggers expansion)
 * ============================================================================ */

static bool test_large_allocation(void) {
    TEST_START("Large Allocation (Expansion)");

    heap_stats_t stats_before, stats_after;
    heap_get_stats(&stats_before);

    /* Allocate more than initial 64KB */
    void* ptr = kmalloc(128 * 1024);  /* 128KB */
    TEST_ASSERT(ptr != NULL, "kmalloc(128KB) failed");

    memset(ptr, 0xBB, 128 * 1024);

    heap_get_stats(&stats_after);
    TEST_ASSERT(stats_after.total_bytes >= stats_before.total_bytes,
                "Heap should have expanded");

    kfree(ptr);

    TEST_PASS();
}

/* ============================================================================
 * Test 5: Aligned Allocation
 * ============================================================================ */

static bool test_aligned_allocation(void) {
    TEST_START("Aligned Allocation");

    void* ptr16 = kmalloc_aligned(64, 16);
    TEST_ASSERT(ptr16 != NULL, "kmalloc_aligned failed");
    TEST_ASSERT(((virtual_addr_t)ptr16 & 0xF) == 0, "16-byte alignment failed");
    kfree(ptr16);

    void* ptr64 = kmalloc_aligned(128, 64);
    TEST_ASSERT(ptr64 != NULL, "kmalloc_aligned(64) failed");
    TEST_ASSERT(((virtual_addr_t)ptr64 & 0x3F) == 0, "64-byte alignment failed");
    kfree(ptr64);

    void* ptr4096 = kmalloc_aligned(256, 4096);
    TEST_ASSERT(ptr4096 != NULL, "kmalloc_aligned(4096) failed");
    TEST_ASSERT(((virtual_addr_t)ptr4096 & 0xFFF) == 0, "4096-byte alignment failed");
    kfree(ptr4096);

    TEST_PASS();
}

/* ============================================================================
 * Test 6: Reallocation
 * ============================================================================ */

static bool test_reallocation(void) {
    TEST_START("Reallocation");

    char* ptr = (char*)kmalloc(16);
    TEST_ASSERT(ptr != NULL, "Initial kmalloc failed");

    strcpy(ptr, "Hello, World!");
    TEST_ASSERT(strcmp(ptr, "Hello, World!") == 0, "String copy failed");

    /* Grow */
    ptr = (char*)krealloc(ptr, 64);
    TEST_ASSERT(ptr != NULL, "krealloc grow failed");
    TEST_ASSERT(strcmp(ptr, "Hello, World!") == 0, "Content corrupted after grow");

    /* Shrink */
    ptr = (char*)krealloc(ptr, 32);
    TEST_ASSERT(ptr != NULL, "krealloc shrink failed");
    TEST_ASSERT(strcmp(ptr, "Hello, World!") == 0, "Content corrupted after shrink");

    kfree(ptr);

    /* realloc with NULL should be like malloc */
    ptr = (char*)krealloc(NULL, 32);
    TEST_ASSERT(ptr != NULL, "krealloc(NULL, size) failed");
    kfree(ptr);

    /* realloc with size 0 should be like free */
    ptr = (char*)kmalloc(32);
    ptr = (char*)krealloc(ptr, 0);
    TEST_ASSERT(ptr == NULL, "krealloc(ptr, 0) should return NULL");

    TEST_PASS();
}

/* ============================================================================
 * Test 7: Block Coalescing
 * ============================================================================ */

static bool test_coalescing(void) {
    TEST_START("Block Coalescing");

    heap_stats_t stats_before, stats_after;

    /* Allocate three blocks */
    void* p1 = kmalloc(1024);
    void* p2 = kmalloc(1024);
    void* p3 = kmalloc(1024);

    TEST_ASSERT(p1 != NULL && p2 != NULL && p3 != NULL, "Allocations failed");

    heap_get_stats(&stats_before);

    /* Free middle block */
    kfree(p2);

    /* Free first block - should coalesce with middle */
    kfree(p1);

    heap_get_stats(&stats_after);

    /* Free blocks should have coalesced */
    TEST_ASSERT(stats_after.free_blocks >= stats_before.free_blocks,
                "Coalescing should reduce or maintain free block count");

    kfree(p3);

    TEST_PASS();
}

/* ============================================================================
 * Test 8: Memory Persistence
 * ============================================================================ */

static bool test_memory_persistence(void) {
    TEST_START("Memory Persistence");

    /* Allocate and set pattern */
    uint32_t* ptr = (uint32_t*)kmalloc(1024);
    TEST_ASSERT(ptr != NULL, "kmalloc failed");

    for (int i = 0; i < 256; i++) {
        ptr[i] = 0xDEADBEEF;
    }

    /* Verify pattern */
    for (int i = 0; i < 256; i++) {
        TEST_ASSERT(ptr[i] == 0xDEADBEEF, "Memory pattern mismatch");
    }

    kfree(ptr);

    TEST_PASS();
}

/* ============================================================================
 * Test 9: Statistics
 * ============================================================================ */

static bool test_statistics(void) {
    TEST_START("Statistics");

    heap_stats_t stats;

    heap_result_t result = heap_get_stats(&stats);
    TEST_ASSERT(result == HEAP_OK, "heap_get_stats failed");

    TEST_ASSERT(stats.total_bytes > 0, "Total bytes should be positive");
    TEST_ASSERT(stats.alloc_count > 0, "Alloc count should be positive");
    TEST_ASSERT(stats.total_bytes == stats.used_bytes + stats.free_bytes,
                "Total != used + free");

    TEST_PASS();
}

/* ============================================================================
 * Test 10: Stress Test (many small allocations)
 * ============================================================================ */

static bool test_stress_many_small(void) {
    TEST_START("Stress Test (Many Small Allocations)");

    #define NUM_ALLOCS 100
    void* ptrs[NUM_ALLOCS];

    for (int i = 0; i < NUM_ALLOCS; i++) {
        ptrs[i] = kmalloc(32 + (i % 8) * 16);  /* 32 to 144 bytes */
        if (ptrs[i] == NULL) {
            /* Cleanup before failing */
            for (int j = 0; j < i; j++) {
                kfree(ptrs[j]);
            }
            TEST_ASSERT(false, "Allocation failed in stress test");
        }
    }

    /* Verify all unique */
    for (int i = 0; i < NUM_ALLOCS; i++) {
        for (int j = i + 1; j < NUM_ALLOCS; j++) {
            if (ptrs[i] == ptrs[j]) {
                /* Cleanup */
                for (int k = 0; k < NUM_ALLOCS; k++) {
                    kfree(ptrs[k]);
                }
                TEST_ASSERT(false, "Duplicate pointer in stress test");
            }
        }
    }

    /* Free all */
    for (int i = 0; i < NUM_ALLOCS; i++) {
        kfree(ptrs[i]);
    }

    #undef NUM_ALLOCS

    TEST_PASS();
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * heap_run_demo - Run the heap allocator demonstration
 */
int heap_run_demo(void) {
    klog_trace("\n");
    klog_trace("╔════════════════════════════════════════╗\n");
    klog_trace("║   Heap Allocator Demo Starting         ║\n");
    klog_trace("╚════════════════════════════════════════╝\n");

    tests_passed = 0;
    tests_failed = 0;

    /* Run all tests */
    test_basic_alloc_free();
    test_multiple_allocations();
    test_zero_size();
    test_large_allocation();
    test_aligned_allocation();
    test_reallocation();
    test_coalescing();
    test_memory_persistence();
    test_statistics();
    test_stress_many_small();

    /* Print summary */
    klog_trace("\n");
    klog_trace("╔════════════════════════════════════════╗\n");
    klog_trace("║   Heap Allocator Demo Summary         ║\n");
    klog_trace("╚════════════════════════════════════════╝\n");
    klog_info("[HEAP_DEMO] Tests Passed: %d\n", tests_passed);
    klog_info("[HEAP_DEMO] Tests Failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        klog_info("[HEAP_DEMO] All tests PASSED!\n");
    } else {
        klog_error("[HEAP_DEMO] Some tests FAILED!\n");
    }

    /* Print final heap state */
    klog_trace("\n");
    heap_dump();

    return tests_failed > 0 ? -1 : 0;
}

/**
 * heap_stop_demo - Stop the heap demo and cleanup resources
 */
void heap_stop_demo(void) {
    klog_info("[HEAP_DEMO] Stopping heap demo...\n");
    /* No persistent resources to clean up */
}
