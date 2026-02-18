/* ==============================================================================
 * CCOS - Generic Hash Map Implementation
 * ==============================================================================
 * This module provides a generic hash map data structure that can be reused
 * across different kernel components. Uses chaining for collision resolution.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"

/* ============================================================================
 * Hash Map Types
 * ============================================================================ */

/**
 * @brief Hash function type
 * @param key Pointer to the key
 * @return Hash value (should be size_t)
 */
typedef size_t (*hash_fn_t)(const void* key);

/**
 * @brief Key equality function type
 * @param a First key
 * @param b Second key
 * @return true if keys are equal, false otherwise
 */
typedef bool (*hash_eq_fn_t)(const void* a, const void* b);

/**
 * @brief Hash map entry (internal)
 */
typedef struct hash_entry {
    const void* key;           /* Pointer to key (not owned by hashmap) */
    void* value;               /* Pointer to value (not owned by hashmap) */
    struct hash_entry* next;   /* Next entry in chain */
} hash_entry_t;

/**
 * @brief Hash map structure
 */
typedef struct {
    hash_entry_t** buckets;    /* Array of bucket heads */
    size_t num_buckets;        /* Number of buckets (power of 2 recommended) */
    size_t size;               /* Number of entries currently stored */
    hash_fn_t hash_func;       /* Hash function */
    hash_eq_fn_t eq_func;      /* Equality function */
} hashmap_t;

/* ============================================================================
 * Hash Map API
 * ============================================================================ */

/**
 * @brief Create a new hash map
 * @param num_buckets Number of buckets (should be power of 2 for efficiency)
 * @param hash_fn Hash function to use
 * @param eq_fn Equality function to use
 * @return Pointer to new hash map, or NULL on failure
 */
hashmap_t* hashmap_create(size_t num_buckets, hash_fn_t hash_fn, hash_eq_fn_t eq_fn);

/**
 * @brief Destroy a hash map
 * @param map Hash map to destroy (does not free keys or values)
 * @note This only frees internal structures. Keys and values are not freed.
 */
void hashmap_destroy(hashmap_t* map);

/**
 * @brief Insert or update a key-value pair
 * @param map Hash map
 * @param key Key (must remain valid for lifetime of entry)
 * @param value Value to associate with key
 * @return 0 on success, negative on error (e.g., OOM)
 */
int hashmap_put(hashmap_t* map, const void* key, void* value);

/**
 * @brief Get value associated with key
 * @param map Hash map
 * @param key Key to look up
 * @return Value associated with key, or NULL if not found
 */
void* hashmap_get(hashmap_t* map, const void* key);

/**
 * @brief Remove a key-value pair
 * @param map Hash map
 * @param key Key to remove
 * @return Previous value, or NULL if key was not found
 */
void* hashmap_remove(hashmap_t* map, const void* key);

/**
 * @brief Get number of entries in hash map
 * @param map Hash map
 * @return Number of entries
 */
size_t hashmap_size(hashmap_t* map);

/**
 * @brief Check if hash map contains key
 * @param map Hash map
 * @param key Key to check
 * @return true if key is present, false otherwise
 */
bool hashmap_contains(hashmap_t* map, const void* key);

/**
 * @brief Clear all entries from hash map
 * @param map Hash map
 * @note Does not free keys or values, just removes entries
 */
void hashmap_clear(hashmap_t* map);

/* ============================================================================
 * Common Hash Functions
 * ============================================================================ */

/**
 * @brief Hash function for 64-bit integers (direct cast)
 * @param key Pointer to uint64_t
 * @return Hash value
 */
size_t hash_uint64(const void* key);

/**
 * @brief Hash function for pointers (direct cast)
 * @param key Pointer to address
 * @return Hash value
 */
size_t hash_ptr(const void* key);

/**
 * @brief Equality function for 64-bit integers
 */
bool eq_uint64(const void* a, const void* b);

/**
 * @brief Equality function for pointers
 */
bool eq_ptr(const void* a, const void* b);

/* ============================================================================
 * Iteration Support
 * ============================================================================ */

/**
 * @brief Iterator callback function type
 * @param key Current key
 * @param value Current value
 * @param context User-provided context
 * @return true to continue iteration, false to stop
 */
typedef bool (*hashmap_iterator_fn)(const void* key, void* value, void* context);

/**
 * @brief Iterate over all entries in hash map
 * @param map Hash map
 * @param iter_fn Callback function for each entry
 * @param context User context passed to callback
 * @return Number of entries iterated
 */
size_t hashmap_foreach(hashmap_t* map, hashmap_iterator_fn iter_fn, void* context);
