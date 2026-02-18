/* ==============================================================================
 * CCOS - Generic Hash Map Implementation
 * ==============================================================================
 */

#include "hashmap.h"
#include "memory.h"
#include "mm/heap/heap.h"

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get bucket index for a key
 */
static inline size_t bucket_index(hashmap_t* map, const void* key) {
    size_t hash = map->hash_func(key);
    return hash & (map->num_buckets - 1); /* Works for power-of-2 sizes */
}

/**
 * @brief Create a new hash entry
 */
static hash_entry_t* entry_create(const void* key, void* value) {
    hash_entry_t* entry = (hash_entry_t*)kmalloc(sizeof(hash_entry_t));
    if (entry == NULL) {
        return NULL;
    }
    entry->key = key;
    entry->value = value;
    entry->next = NULL;
    return entry;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

hashmap_t* hashmap_create(size_t num_buckets, hash_fn_t hash_fn, hash_eq_fn_t eq_fn) {
    if (num_buckets == 0 || hash_fn == NULL || eq_fn == NULL) {
        return NULL;
    }

    /* Round up to power of 2 if not already */
    size_t buckets = num_buckets;
    if (buckets & (buckets - 1)) {
        /* Find next power of 2 */
        buckets--;
        buckets |= buckets >> 1;
        buckets |= buckets >> 2;
        buckets |= buckets >> 4;
        buckets |= buckets >> 8;
        buckets |= buckets >> 16;
        buckets++;
    }

    /* Allocate hash map structure */
    hashmap_t* map = (hashmap_t*)kmalloc(sizeof(hashmap_t));
    if (map == NULL) {
        return NULL;
    }

    /* Allocate bucket array */
    map->buckets = (hash_entry_t**)kmalloc(sizeof(hash_entry_t*) * buckets);
    if (map->buckets == NULL) {
        kfree(map);
        return NULL;
    }

    /* Initialize buckets to NULL */
    for (size_t i = 0; i < buckets; i++) {
        map->buckets[i] = NULL;
    }

    map->num_buckets = buckets;
    map->size = 0;
    map->hash_func = hash_fn;
    map->eq_func = eq_fn;

    return map;
}

void hashmap_destroy(hashmap_t* map) {
    if (map == NULL) {
        return;
    }

    /* Free all entries */
    for (size_t i = 0; i < map->num_buckets; i++) {
        hash_entry_t* entry = map->buckets[i];
        while (entry != NULL) {
            hash_entry_t* next = entry->next;
            kfree(entry);
            entry = next;
        }
    }

    /* Free bucket array */
    kfree(map->buckets);

    /* Free map structure */
    kfree(map);
}

int hashmap_put(hashmap_t* map, const void* key, void* value) {
    if (map == NULL || key == NULL) {
        return -1;
    }

    size_t idx = bucket_index(map, key);
    hash_entry_t* entry = map->buckets[idx];

    /* Check if key already exists */
    while (entry != NULL) {
        if (map->eq_func(entry->key, key)) {
            /* Update existing entry */
            entry->value = value;
            return 0;
        }
        entry = entry->next;
    }

    /* Create new entry */
    hash_entry_t* new_entry = entry_create(key, value);
    if (new_entry == NULL) {
        return -2; /* OOM */
    }

    /* Insert at head of bucket */
    new_entry->next = map->buckets[idx];
    map->buckets[idx] = new_entry;
    map->size++;

    return 0;
}

void* hashmap_get(hashmap_t* map, const void* key) {
    if (map == NULL || key == NULL) {
        return NULL;
    }

    size_t idx = bucket_index(map, key);
    hash_entry_t* entry = map->buckets[idx];

    while (entry != NULL) {
        if (map->eq_func(entry->key, key)) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

void* hashmap_remove(hashmap_t* map, const void* key) {
    if (map == NULL || key == NULL) {
        return NULL;
    }

    size_t idx = bucket_index(map, key);
    hash_entry_t* entry = map->buckets[idx];
    hash_entry_t* prev = NULL;

    while (entry != NULL) {
        if (map->eq_func(entry->key, key)) {
            /* Found it - remove from chain */
            void* value = entry->value;

            if (prev == NULL) {
                map->buckets[idx] = entry->next;
            } else {
                prev->next = entry->next;
            }

            kfree(entry);
            map->size--;
            return value;
        }
        prev = entry;
        entry = entry->next;
    }

    return NULL;
}

size_t hashmap_size(hashmap_t* map) {
    if (map == NULL) {
        return 0;
    }
    return map->size;
}

bool hashmap_contains(hashmap_t* map, const void* key) {
    return hashmap_get(map, key) != NULL;
}

void hashmap_clear(hashmap_t* map) {
    if (map == NULL) {
        return;
    }

    /* Free all entries */
    for (size_t i = 0; i < map->num_buckets; i++) {
        hash_entry_t* entry = map->buckets[i];
        while (entry != NULL) {
            hash_entry_t* next = entry->next;
            kfree(entry);
            entry = next;
        }
        map->buckets[i] = NULL;
    }

    map->size = 0;
}

size_t hashmap_foreach(hashmap_t* map, hashmap_iterator_fn iter_fn, void* context) {
    if (map == NULL || iter_fn == NULL) {
        return 0;
    }

    size_t count = 0;

    for (size_t i = 0; i < map->num_buckets; i++) {
        hash_entry_t* entry = map->buckets[i];
        while (entry != NULL) {
            count++;
            if (!iter_fn(entry->key, entry->value, context)) {
                return count; /* Iterator requested stop */
            }
            entry = entry->next;
        }
    }

    return count;
}

/* ============================================================================
 * Common Hash Functions
 * ============================================================================ */

size_t hash_uint64(const void* key) {
    uint64_t val = *(const uint64_t*)key;
    /* FNV-1a style hash */
    size_t hash = 14695981039346656037ULL;
    hash ^= (val >> 32) & 0xFFFFFFFF;
    hash *= 1099511628211ULL;
    hash ^= val & 0xFFFFFFFF;
    hash *= 1099511628211ULL;
    return hash;
}

size_t hash_ptr(const void* key) {
    uintptr_t val = (uintptr_t)key;
    /* Simple pointer hash */
    return (size_t)(val >> 3) ^ (val << (sizeof(size_t) * 8 - 3));
}

bool eq_uint64(const void* a, const void* b) {
    return *(const uint64_t*)a == *(const uint64_t*)b;
}

bool eq_ptr(const void* a, const void* b) {
    return a == b;
}
