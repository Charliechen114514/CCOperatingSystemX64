/**
 * @file test_hashmap.c
 * @brief Comprehensive unit tests for hashmap module
 */

// Standard types for hosted environment
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Include kernel types and hashmap header
#include "defines/types.h"
#include "base/hashmap.h"

// Define global test counters
int g_test_passed = 0;
int g_test_failed = 0;

#include "host_support.h"

// External heap stub functions (provided by heap_stub.c)
extern void* kmalloc(size_t size);
extern void kfree(void* ptr);
extern void heap_dump_leaks(void);

// ============================================================================
// Test Data Structures
// ============================================================================

/**
 * @brief Test key-value pair for uint64 keys
 */
typedef struct {
    uint64_t key;
    int value;
} test_kv_pair;

/**
 * @brief String key test structure
 */
typedef struct {
    char key[32];
    int value;
} test_str_kv;

// ============================================================================
// Test Hash Functions for Different Key Types
// ============================================================================

/**
 * @brief Simple string hash function (djb2)
 */
static size_t hash_string(const void* key) {
    const char* str = (const char*)key;
    size_t hash = 5381;
    int c;
    while ((c = *str++) != '\0') {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

/**
 * @brief String equality function
 */
static bool eq_string(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b) == 0;
}

// ============================================================================
// Global Test Keys (stable pointers)
// ============================================================================

#define NUM_TEST_KEYS 128
#define NUM_COLLISION_KEYS 500  // For collision and large map tests
static uint64_t g_uint64_keys[NUM_TEST_KEYS];
static uint64_t g_collision_keys[NUM_COLLISION_KEYS];  // Keys with values 0, 1, 2, ...
static test_str_kv g_str_keys[NUM_TEST_KEYS];

/**
 * @brief Initialize test keys
 */
static void init_test_keys(void) {
    // Initialize uint64 keys
    for (int i = 0; i < NUM_TEST_KEYS; i++) {
        g_uint64_keys[i] = (uint64_t)i * 7 + 13; // Some pattern
    }

    // Initialize collision keys (simple sequential values)
    for (int i = 0; i < NUM_COLLISION_KEYS; i++) {
        g_collision_keys[i] = (uint64_t)i;
    }

    // Initialize string keys
    for (int i = 0; i < NUM_TEST_KEYS; i++) {
        snprintf(g_str_keys[i].key, sizeof(g_str_keys[i].key), "key_%d", i);
        g_str_keys[i].value = i * 10;
    }
}

// ============================================================================
// 1. Creation and Destruction Tests
// ============================================================================

static void test_create_destroy(void) {
    TEST_INFO("Testing hashmap creation and destruction");

    // Test basic creation
    hashmap_t* map = hashmap_create(16, hash_uint64, eq_uint64);
    TEST_ASSERT_NOT_NULL(map, "hashmap_create: non-NULL map");
    TEST_ASSERT_EQ(hashmap_size(map), (size_t)0, "hashmap_create: new map is empty");

    // Test destruction
    hashmap_destroy(map);
    TEST_PASS("hashmap_create and hashmap_destroy");

    // Test various bucket sizes (power of 2 rounding)
    hashmap_t* map5 = hashmap_create(5, hash_uint64, eq_uint64);   // Should round to 8
    hashmap_t* map17 = hashmap_create(17, hash_uint64, eq_uint64); // Should round to 32
    TEST_ASSERT_NOT_NULL(map5, "hashmap_create: size 5 rounds to 8");
    TEST_ASSERT_NOT_NULL(map17, "hashmap_create: size 17 rounds to 32");

    hashmap_destroy(map5);
    hashmap_destroy(map17);

    // Test NULL parameters
    hashmap_t* null_map = hashmap_create(0, hash_uint64, eq_uint64);
    TEST_ASSERT_NULL(null_map, "hashmap_create: returns NULL for 0 buckets");

    null_map = hashmap_create(16, NULL, eq_uint64);
    TEST_ASSERT_NULL(null_map, "hashmap_create: returns NULL for NULL hash_fn");

    null_map = hashmap_create(16, hash_uint64, NULL);
    TEST_ASSERT_NULL(null_map, "hashmap_create: returns NULL for NULL eq_fn");

    TEST_PASS("hashmap creation parameter validation");
}

// ============================================================================
// 2. Basic Put and Get Tests
// ============================================================================

static void test_put_get(void) {
    TEST_INFO("Testing basic put and get operations");

    init_test_keys();

    hashmap_t* map = hashmap_create(16, hash_uint64, eq_uint64);
    TEST_ASSERT_NOT_NULL(map, "put/get: map created");

    // Test single put/get
    int result = hashmap_put(map, &g_uint64_keys[0], (void*)100);
    TEST_ASSERT_EQ(result, 0, "hashmap_put: successful insert returns 0");
    TEST_ASSERT_EQ(hashmap_size(map), (size_t)1, "hashmap_put: size incremented");

    void* value = hashmap_get(map, &g_uint64_keys[0]);
    TEST_ASSERT_EQ((uintptr_t)value, (uintptr_t)100, "hashmap_get: retrieves correct value");

    // Test multiple puts
    hashmap_put(map, &g_uint64_keys[1], (void*)200);
    hashmap_put(map, &g_uint64_keys[2], (void*)300);

    TEST_ASSERT_EQ(hashmap_size(map), (size_t)3, "hashmap_put: multiple inserts");

    value = hashmap_get(map, &g_uint64_keys[1]);
    TEST_ASSERT_EQ((uintptr_t)value, (uintptr_t)200, "hashmap_get: second value correct");

    value = hashmap_get(map, &g_uint64_keys[2]);
    TEST_ASSERT_EQ((uintptr_t)value, (uintptr_t)300, "hashmap_get: third value correct");

    // Test get of non-existent key
    uint64_t nonexistent = 99999;
    value = hashmap_get(map, &nonexistent);
    TEST_ASSERT_NULL(value, "hashmap_get: returns NULL for non-existent key");

    hashmap_destroy(map);

    TEST_PASS("basic put and get");
}

// ============================================================================
// 3. Update Existing Key Tests
// ============================================================================

static void test_update_key(void) {
    TEST_INFO("Testing key update (put existing key)");

    init_test_keys();

    hashmap_t* map = hashmap_create(8, hash_uint64, eq_uint64);

    // Insert a key
    hashmap_put(map, &g_uint64_keys[0], (void*)100);
    TEST_ASSERT_EQ(hashmap_size(map), (size_t)1, "update: initial size");

    // Update the same key
    int result = hashmap_put(map, &g_uint64_keys[0], (void*)200);
    TEST_ASSERT_EQ(result, 0, "hashmap_put: update returns 0");
    TEST_ASSERT_EQ(hashmap_size(map), (size_t)1, "hashmap_put: size unchanged on update");

    void* value = hashmap_get(map, &g_uint64_keys[0]);
    TEST_ASSERT_EQ((uintptr_t)value, (uintptr_t)200, "hashmap_put: value updated");

    hashmap_destroy(map);

    TEST_PASS("key update");
}

// ============================================================================
// 4. Remove Tests
// ============================================================================

static void test_remove(void) {
    TEST_INFO("Testing remove operations");

    init_test_keys();

    hashmap_t* map = hashmap_create(16, hash_uint64, eq_uint64);

    // Add some entries
    hashmap_put(map, &g_uint64_keys[0], (void*)100);
    hashmap_put(map, &g_uint64_keys[1], (void*)200);
    hashmap_put(map, &g_uint64_keys[2], (void*)300);

    TEST_ASSERT_EQ(hashmap_size(map), (size_t)3, "remove: initial size");

    // Remove middle entry
    void* value = hashmap_remove(map, &g_uint64_keys[1]);
    TEST_ASSERT_EQ((uintptr_t)value, (uintptr_t)200, "hashmap_remove: returns removed value");
    TEST_ASSERT_EQ(hashmap_size(map), (size_t)2, "hashmap_remove: size decremented");

    // Verify entry is gone
    value = hashmap_get(map, &g_uint64_keys[1]);
    TEST_ASSERT_NULL(value, "hashmap_remove: entry no longer accessible");

    // Verify other entries still exist
    value = hashmap_get(map, &g_uint64_keys[0]);
    TEST_ASSERT_EQ((uintptr_t)value, (uintptr_t)100, "hashmap_remove: other entries intact");

    // Remove non-existent key
    uint64_t nonexistent = 99999;
    value = hashmap_remove(map, &nonexistent);
    TEST_ASSERT_NULL(value, "hashmap_remove: returns NULL for non-existent key");
    TEST_ASSERT_EQ(hashmap_size(map), (size_t)2, "hashmap_remove: size unchanged");

    // Remove all entries
    hashmap_remove(map, &g_uint64_keys[0]);
    hashmap_remove(map, &g_uint64_keys[2]);
    TEST_ASSERT_EQ(hashmap_size(map), (size_t)0, "hashmap_remove: all entries removed");

    hashmap_destroy(map);

    TEST_PASS("remove operations");
}

// ============================================================================
// 5. Contains Tests
// ============================================================================

static void test_contains(void) {
    TEST_INFO("Testing contains operation");

    init_test_keys();

    hashmap_t* map = hashmap_create(16, hash_uint64, eq_uint64);

    // Empty map
    TEST_ASSERT_FALSE(hashmap_contains(map, &g_uint64_keys[0]), "contains: empty map returns false");

    // Add entry
    hashmap_put(map, &g_uint64_keys[0], (void*)100);
    TEST_ASSERT_TRUE(hashmap_contains(map, &g_uint64_keys[0]), "contains: returns true for existing key");
    TEST_ASSERT_FALSE(hashmap_contains(map, &g_uint64_keys[1]), "contains: returns false for non-existent key");

    // After remove
    hashmap_remove(map, &g_uint64_keys[0]);
    TEST_ASSERT_FALSE(hashmap_contains(map, &g_uint64_keys[0]), "contains: returns false after remove");

    hashmap_destroy(map);

    TEST_PASS("contains operation");
}

// ============================================================================
// 6. Clear Tests
// ============================================================================

static void test_clear(void) {
    TEST_INFO("Testing clear operation");

    init_test_keys();

    hashmap_t* map = hashmap_create(16, hash_uint64, eq_uint64);

    // Add multiple entries
    for (int i = 0; i < 10; i++) {
        hashmap_put(map, &g_uint64_keys[i], (void*)(uintptr_t)(i * 100));
    }
    TEST_ASSERT_EQ(hashmap_size(map), (size_t)10, "clear: 10 entries added");

    // Clear the map
    hashmap_clear(map);
    TEST_ASSERT_EQ(hashmap_size(map), (size_t)0, "clear: size is 0");
    TEST_ASSERT_FALSE(hashmap_contains(map, &g_uint64_keys[0]), "clear: entries removed");

    // Should be able to add entries after clear
    hashmap_put(map, &g_uint64_keys[0], (void*)999);
    TEST_ASSERT_EQ(hashmap_size(map), (size_t)1, "clear: can add after clear");

    hashmap_destroy(map);

    TEST_PASS("clear operation");
}

// ============================================================================
// 7. Collision Handling Tests
// ============================================================================

static void test_collisions(void) {
    TEST_INFO("Testing collision handling");

    init_test_keys();

    // Create a small map to force collisions
    hashmap_t* map = hashmap_create(4, hash_uint64, eq_uint64);

    // Add many entries to the same bucket (small map)
    // Add more than bucket count to ensure collisions
    for (int i = 0; i < 20; i++) {
        hashmap_put(map, &g_collision_keys[i], (void*)(uintptr_t)(i * 10));
    }

    TEST_ASSERT_EQ(hashmap_size(map), (size_t)20, "collision: all entries stored despite collisions");

    // Verify all entries are retrievable
    for (int i = 0; i < 20; i++) {
        void* value = hashmap_get(map, &g_collision_keys[i]);
        TEST_ASSERT_EQ((uintptr_t)value, (uintptr_t)(i * 10), "collision: correct value under collision");
    }

    hashmap_destroy(map);

    TEST_PASS("collision handling");
}

// ============================================================================
// 8. String Keys Tests
// ============================================================================

static void test_string_keys(void) {
    TEST_INFO("Testing string key operations");

    init_test_keys();

    hashmap_t* map = hashmap_create(16, hash_string, eq_string);

    // Add string keys
    hashmap_put(map, g_str_keys[0].key, (void*)&g_str_keys[0]);
    hashmap_put(map, g_str_keys[1].key, (void*)&g_str_keys[1]);
    hashmap_put(map, g_str_keys[2].key, (void*)&g_str_keys[2]);

    TEST_ASSERT_EQ(hashmap_size(map), (size_t)3, "string: 3 entries added");

    // Retrieve by string key
    test_str_kv* kv = (test_str_kv*)hashmap_get(map, "key_0");
    TEST_ASSERT_NOT_NULL(kv, "string: retrieve by string key");
    TEST_ASSERT_EQ(kv->value, 0, "string: correct value retrieved");

    kv = (test_str_kv*)hashmap_get(map, "key_1");
    TEST_ASSERT_EQ(kv->value, 10, "string: second value correct");

    // Test non-existent string key
    kv = (test_str_kv*)hashmap_get(map, "nonexistent");
    TEST_ASSERT_NULL(kv, "string: NULL for non-existent key");

    hashmap_destroy(map);

    TEST_PASS("string key operations");
}

// ============================================================================
// 9. Pointer Keys Tests
// ============================================================================

static void test_pointer_keys(void) {
    TEST_INFO("Testing pointer key operations");


    hashmap_t* map = hashmap_create(8, hash_ptr, eq_ptr);

    // Use addresses as keys
    int a = 10, b = 20, c = 30;
    hashmap_put(map, &a, (void*)100);
    hashmap_put(map, &b, (void*)200);
    hashmap_put(map, &c, (void*)300);

    TEST_ASSERT_EQ(hashmap_size(map), (size_t)3, "pointer: 3 entries");

    void* value = hashmap_get(map, &a);
    TEST_ASSERT_EQ((uintptr_t)value, (uintptr_t)100, "pointer: correct value for &a");

    value = hashmap_get(map, &b);
    TEST_ASSERT_EQ((uintptr_t)value, (uintptr_t)200, "pointer: correct value for &b");

    hashmap_destroy(map);

    TEST_PASS("pointer key operations");
}

// ============================================================================
// 10. Iteration Tests
// ============================================================================

/**
 * @brief Context for foreach test
 */
typedef struct {
    int count;
    int sum;
} foreach_context;

/**
 * @brief Iterator callback for uint64 keys
 */
static bool foreach_sum_uint64(const void* key, void* value, void* context) {
    (void)key; // Unused
    foreach_context* ctx = (foreach_context*)context;
    ctx->count++;
    ctx->sum += (int)(uintptr_t)value;
    return true; // Continue iteration
}

/**
 * @brief Iterator callback that stops early
 */
static bool foreach_stop_early(const void* key, void* value, void* context) {
    (void)key;
    (void)value;
    int* count = (int*)context;
    (*count)++;
    return *count < 5; // Stop after 5 iterations
}

static void test_iteration(void) {
    TEST_INFO("Testing foreach iteration");

    init_test_keys();

    hashmap_t* map = hashmap_create(16, hash_uint64, eq_uint64);

    // Add entries
    for (int i = 0; i < 10; i++) {
        hashmap_put(map, &g_uint64_keys[i], (void*)(uintptr_t)(i * 10));
    }

    // Test full iteration
    foreach_context ctx = {0, 0};
    size_t iterated = hashmap_foreach(map, foreach_sum_uint64, &ctx);

    TEST_ASSERT_EQ(ctx.count, 10, "foreach: visited all 10 entries");
    TEST_ASSERT_EQ(ctx.sum, 450, "foreach: sum is correct (0+10+...+90=450)"); // Wait, i*10 for i=0..9 = 0,10,20,...,90 sum = 450
    TEST_ASSERT_EQ(iterated, (size_t)10, "foreach: returns correct count");

    // Test early stop
    int stop_count = 0;
    iterated = hashmap_foreach(map, foreach_stop_early, &stop_count);
    TEST_ASSERT_EQ(stop_count, 5, "foreach: stopped after 5 iterations");
    TEST_ASSERT_EQ(iterated, (size_t)5, "foreach: returns count at stop");

    // Test with NULL callback
    iterated = hashmap_foreach(map, NULL, &ctx);
    TEST_ASSERT_EQ(iterated, (size_t)0, "foreach: returns 0 for NULL callback");

    hashmap_destroy(map);

    TEST_PASS("foreach iteration");
}

// ============================================================================
// 11. NULL Handling Tests
// ============================================================================

static void test_null_handling(void) {
    TEST_INFO("Testing NULL parameter handling");

    init_test_keys();

    hashmap_t* map = hashmap_create(16, hash_uint64, eq_uint64);

    // Test operations on NULL map
    void* value = hashmap_get(NULL, &g_uint64_keys[0]);
    TEST_ASSERT_NULL(value, "null: hashmap_get returns NULL for NULL map");

    value = hashmap_remove(NULL, &g_uint64_keys[0]);
    TEST_ASSERT_NULL(value, "null: hashmap_remove returns NULL for NULL map");

    size_t size = hashmap_size(NULL);
    TEST_ASSERT_EQ(size, (size_t)0, "null: hashmap_size returns 0 for NULL map");

    bool contains = hashmap_contains(NULL, &g_uint64_keys[0]);
    TEST_ASSERT_FALSE(contains, "null: hashmap_contains returns false for NULL map");

    int result = hashmap_put(NULL, &g_uint64_keys[0], (void*)100);
    TEST_ASSERT_EQ(result, -1, "null: hashmap_put returns error for NULL map");

    result = hashmap_put(map, NULL, (void*)100);
    TEST_ASSERT_EQ(result, -1, "null: hashmap_put returns error for NULL key");

    hashmap_clear(NULL);
    hashmap_destroy(NULL); // Should not crash

    hashmap_destroy(map);

    TEST_PASS("NULL parameter handling");
}

// ============================================================================
// 12. Large Map Tests
// ============================================================================

static void test_large_map(void) {
    TEST_INFO("Testing large map operations");

    init_test_keys();

    // Create a map with many buckets
    hashmap_t* map = hashmap_create(256, hash_uint64, eq_uint64);

    // Add many entries
    int num_entries = 500;
    for (int i = 0; i < num_entries; i++) {
        int result = hashmap_put(map, &g_collision_keys[i], (void*)(uintptr_t)(i * 2));
        TEST_ASSERT_EQ(result, 0, "large: insert succeeds");
    }

    TEST_ASSERT_EQ(hashmap_size(map), (size_t)num_entries, "large: all entries stored");

    // Verify all entries
    int errors = 0;
    for (int i = 0; i < num_entries; i++) {
        void* value = hashmap_get(map, &g_collision_keys[i]);
        if ((uintptr_t)value != (uintptr_t)(i * 2)) {
            errors++;
        }
    }
    TEST_ASSERT_EQ(errors, 0, "large: all values correct");

    // Remove every other entry
    for (int i = 0; i < num_entries; i += 2) {
        hashmap_remove(map, &g_collision_keys[i]);
    }

    TEST_ASSERT_EQ(hashmap_size(map), (size_t)(num_entries / 2), "large: correct size after removals");

    hashmap_destroy(map);

    TEST_PASS("large map operations");
}

// ============================================================================
// 13. Edge Cases Tests
// ============================================================================

static void test_edge_cases(void) {
    TEST_INFO("Testing edge cases");


    // Test with minimum bucket count (should round to 1, but actually to power of 2 = 1)
    hashmap_t* map1 = hashmap_create(1, hash_uint64, eq_uint64);
    TEST_ASSERT_NOT_NULL(map1, "edge: map with 1 bucket created");
    hashmap_destroy(map1);

    // Test put same value multiple times
    hashmap_t* map = hashmap_create(8, hash_uint64, eq_uint64);
    uint64_t key = 42;

    for (int i = 0; i < 10; i++) {
        hashmap_put(map, &key, (void*)(uintptr_t)(100 + i));
    }

    TEST_ASSERT_EQ(hashmap_size(map), (size_t)1, "edge: duplicate key only stores once");
    void* value = hashmap_get(map, &key);
    TEST_ASSERT_EQ((uintptr_t)value, (uintptr_t)109, "edge: last value stored");

    // Test remove from empty map
    hashmap_t* empty = hashmap_create(8, hash_uint64, eq_uint64);
    void* removed = hashmap_remove(empty, &key);
    TEST_ASSERT_NULL(removed, "edge: remove from empty map returns NULL");
    TEST_ASSERT_EQ(hashmap_size(empty), (size_t)0, "edge: empty map size unchanged");
    hashmap_destroy(empty);

    hashmap_destroy(map);

    TEST_PASS("edge cases");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
    TEST_RUNNER_BEGIN("Hashmap Test Suite");

    TEST_INFO("Running creation and destruction tests...");
    test_create_destroy();

    TEST_INFO("Running basic put and get tests...");
    test_put_get();

    TEST_INFO("Running key update tests...");
    test_update_key();

    TEST_INFO("Running remove tests...");
    test_remove();

    TEST_INFO("Running contains tests...");
    test_contains();

    TEST_INFO("Running clear tests...");
    test_clear();

    TEST_INFO("Running collision handling tests...");
    test_collisions();

    TEST_INFO("Running string key tests...");
    test_string_keys();

    TEST_INFO("Running pointer key tests...");
    test_pointer_keys();

    TEST_INFO("Running iteration tests...");
    test_iteration();

    TEST_INFO("Running NULL handling tests...");
    test_null_handling();

    TEST_INFO("Running large map tests...");
    test_large_map();

    TEST_INFO("Running edge case tests...");
    test_edge_cases();

    TEST_RUNNER_END();
}
