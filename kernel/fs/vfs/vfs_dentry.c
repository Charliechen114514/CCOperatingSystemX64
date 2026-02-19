/**
 * @file vfs_dentry.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VFS Directory Entry Cache
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 */

#include "vfs.h"
#include "klogs/kprintf.h"
#include "mm/heap/heap.h"
#include "base/string.h"
#include "base/memory.h"

/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief Dentry cache hash table size
 */
#define DENTRY_HASH_BITS   8
#define DENTRY_HASH_SIZE   (1 << DENTRY_HASH_BITS)

/**
 * @brief Global dentry cache state
 */
static struct {
    list_head hash_table[DENTRY_HASH_SIZE];  /* Dentry hash table */
    list_head lru_list;                      /* LRU list */
    uint32_t count;                          /* Number of cached dentries */
} dentry_cache;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Hash dentry name and parent
 */
static uint32_t dentry_hash(const char* name, vfs_dentry_t* parent) {
    uint32_t hash = 0;
    uint64_t parent_ptr = (uint64_t)(uintptr_t)parent;

    /* Simple string hash */
    while (*name) {
        hash = (hash << 5) + hash + *name++;
    }

    /* Combine with parent pointer */
    hash ^= (uint32_t)(parent_ptr >> 3);
    hash ^= (uint32_t)parent_ptr;

    return hash & (DENTRY_HASH_SIZE - 1);
}

/**
 * @brief Insert dentry into hash table
 */
static void dentry_hash_insert(vfs_dentry_t* dentry) {
    uint32_t hash = dentry_hash(dentry->d_name, dentry->d_parent);
    list_add(&dentry->d_hash, &dentry_cache.hash_table[hash]);
}

/**
 * @brief Remove dentry from hash table
 */
static void dentry_hash_remove(vfs_dentry_t* dentry) {
    list_del(&dentry->d_hash);
}

/* ============================================================================
 * Dentry Allocation and Management
 * ============================================================================ */

/**
 * @brief Allocate a new dentry
 */
vfs_dentry_t* vfs_d_alloc(vfs_dentry_t* parent, const char* name) {
    vfs_dentry_t* dentry = (vfs_dentry_t*)kmalloc(sizeof(vfs_dentry_t));
    if (!dentry) {
        klog_error("vfs: Failed to allocate dentry\n");
        return NULL;
    }

    /* Initialize dentry */
    memset(dentry, 0, sizeof(vfs_dentry_t));
    INIT_LIST_HEAD(&dentry->d_hash);
    INIT_LIST_HEAD(&dentry->d_lru);
    INIT_LIST_HEAD(&dentry->d_subdirs);

    dentry->d_parent = parent;
    dentry->d_inode = NULL;
    dentry->d_flags = DENTRY_FLAG_CACHED;
    dentry->d_count = 1;
    dentry->d_time = 0;

    /* Copy name */
    if (name) {
        strncpy(dentry->d_name, name, sizeof(dentry->d_name) - 1);
    } else {
        dentry->d_name[0] = '\0';
    }

    dentry->d_op = NULL;

    /* Set superblock from parent */
    if (parent) {
        dentry->d_sb = parent->d_sb;
        /* Add to parent's children list */
        list_add(&dentry->d_subdirs, &parent->d_subdirs);
    } else {
        dentry->d_sb = NULL;
    }

    /* Add to hash table */
    dentry_hash_insert(dentry);

    /* Add to LRU list */
    list_add(&dentry->d_lru, &dentry_cache.lru_list);
    dentry_cache.count++;

    return dentry;
}

/**
 * @brief Free a dentry
 */
void vfs_d_free(vfs_dentry_t* dentry) {
    if (!dentry) {
        return;
    }

    /* Remove from hash table */
    dentry_hash_remove(dentry);

    /* Remove from LRU list */
    list_del(&dentry->d_lru);
    dentry_cache.count--;

    /* Remove from parent's children list */
    if (dentry->d_parent) {
        list_del(&dentry->d_subdirs);
    }

    /* Release inode reference */
    if (dentry->d_inode) {
        vfs_iput(dentry->d_inode);
    }

    /* Call dentry release operation */
    if (dentry->d_op && dentry->d_op->d_release) {
        dentry->d_op->d_release(dentry);
    }

    kfree(dentry);
}

/**
 * @brief Get dentry (increment reference)
 */
vfs_dentry_t* vfs_dget(vfs_dentry_t* dentry) {
    if (dentry) {
        dentry->d_count++;
    }
    return dentry;
}

/**
 * @brief Put dentry (decrement reference)
 */
void vfs_dput(vfs_dentry_t* dentry) {
    if (!dentry) {
        return;
    }

    if (dentry->d_count > 0) {
        dentry->d_count--;
    }

    /* Move to front of LRU list */
    list_del(&dentry->d_lru);
    list_add(&dentry->d_lru, &dentry_cache.lru_list);

    /* Free if no more references */
    if (dentry->d_count == 0) {
        vfs_d_free(dentry);
    }
}

/**
 * @brief Instantiate dentry with inode
 */
void vfs_d_instantiate(vfs_dentry_t* dentry, vfs_inode_t* inode) {
    if (!dentry) {
        return;
    }

    dentry->d_inode = inode;
    if (inode) {
        inode->i_count++;
    }

    /* Clear negative flag */
    dentry->d_flags &= ~DENTRY_FLAG_NEGATIVE;
}

/**
 * @brief Look up dentry in cache
 */
vfs_dentry_t* vfs_d_lookup(vfs_dentry_t* parent, const char* name) {
    if (!parent || !name) {
        return NULL;
    }

    uint32_t hash = dentry_hash(name, parent);

    /* Search in hash table */
    vfs_dentry_t* dentry;
    list_for_each_entry(dentry, &dentry_cache.hash_table[hash], d_hash) {
        if (dentry->d_parent == parent &&
            strcmp(dentry->d_name, name) == 0) {
            /* Found in cache, increment reference */
            vfs_dget(dentry);
            return dentry;
        }
    }

    return NULL;
}

/**
 * @brief Create negative dentry (file doesn't exist)
 */
vfs_dentry_t* vfs_d_negative(vfs_dentry_t* parent, const char* name) {
    vfs_dentry_t* dentry = vfs_d_alloc(parent, name);
    if (dentry) {
        dentry->d_flags |= DENTRY_FLAG_NEGATIVE;
        dentry->d_inode = NULL;
    }
    return dentry;
}

/**
 * @brief Check if dentry is negative (file doesn't exist)
 */
bool vfs_d_is_negative(vfs_dentry_t* dentry) {
    return dentry && (dentry->d_flags & DENTRY_FLAG_NEGATIVE);
}

/**
 * @brief Get full path for dentry
 */
int vfs_d_get_path(vfs_dentry_t* dentry, char* buffer, size_t size) {
    if (!dentry || !buffer || size == 0) {
        return -1;
    }

    /* Build path from root to dentry */
    char temp[512];
    int offset = sizeof(temp) - 1;
    temp[offset] = '\0';

    vfs_dentry_t* current = dentry;
    while (current && current->d_parent != current) {
        size_t name_len = strlen(current->d_name);
        offset -= name_len;
        if (offset < 0) {
            return -1;  /* Path too long */
        }
        memcpy(&temp[offset], current->d_name, name_len);

        offset--;
        if (offset < 0) {
            return -1;
        }
        temp[offset] = '/';

        current = current->d_parent;
    }

    /* Copy to buffer */
    if ((size_t)offset >= size) {
        return -1;  /* Buffer too small */
    }

    strcpy(buffer, &temp[offset]);

    return 0;
}

/* ============================================================================
 * Dentry Cache Initialization
 * ============================================================================ */

/**
 * @brief Initialize dentry cache
 */
void vfs_dentry_cache_init(void) {
    /* Initialize hash table */
    for (int i = 0; i < DENTRY_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&dentry_cache.hash_table[i]);
    }
    INIT_LIST_HEAD(&dentry_cache.lru_list);
    dentry_cache.count = 0;
}
