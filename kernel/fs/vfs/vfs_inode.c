/**
 * @file vfs_inode.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VFS Inode Management and Cache
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 */

#include "vfs.h"
#include "klogs/kprintf.h"
#include "mm/heap/heap.h"
#include "base/memory.h"

/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief Inode cache hash table size
 */
#define INODE_HASH_BITS    8
#define INODE_HASH_SIZE    (1 << INODE_HASH_BITS)

/**
 * @brief Global inode cache state
 */
static struct {
    list_head hash_table[INODE_HASH_SIZE];  /* Inode hash table */
    list_head inodes;                       /* List of all inodes */
    uint32_t count;                         /* Number of cached inodes */
} inode_cache;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Hash inode number and superblock
 */
static uint32_t inode_hash(uint64_t ino, vfs_superblock_t* sb) {
    /* Simple hash combining inode number and superblock pointer */
    uint64_t sb_ptr = (uint64_t)(uintptr_t)sb;
    uint32_t hash = (uint32_t)(ino ^ (sb_ptr >> 3));
    return hash & (INODE_HASH_SIZE - 1);
}

/**
 * @brief Insert inode into hash table
 */
static void inode_hash_insert(vfs_inode_t* inode) {
    uint32_t hash = inode_hash(inode->i_ino, inode->i_sb);
    list_add(&inode->i_hash, &inode_cache.hash_table[hash]);
}

/**
 * @brief Remove inode from hash table
 */
static void inode_hash_remove(vfs_inode_t* inode) {
    list_del(&inode->i_hash);
}

/* ============================================================================
 * Inode Allocation and Management
 * ============================================================================ */

/**
 * @brief Allocate a new inode
 */
vfs_inode_t* vfs_alloc_inode(vfs_superblock_t* sb) {
    if (!sb) {
        return NULL;
    }

    vfs_inode_t* inode = (vfs_inode_t*)kmalloc(sizeof(vfs_inode_t));
    if (!inode) {
        klog_error("vfs: Failed to allocate inode\n");
        return NULL;
    }

    /* Initialize inode */
    memset(inode, 0, sizeof(vfs_inode_t));
    INIT_LIST_HEAD(&inode->i_hash);
    INIT_LIST_HEAD(&inode->i_list);
    INIT_LIST_HEAD(&inode->i_sb_list);

    inode->i_sb = sb;
    inode->i_ino = 0;
    inode->i_nlink = 1;
    inode->i_mode = 0;
    inode->i_size = 0;
    inode->i_blocks = 0;
    inode->i_atime = 0;
    inode->i_mtime = 0;
    inode->i_ctime = 0;
    inode->i_count = 1;
    inode->i_state = 0;
    inode->i_op = NULL;
    inode->i_fop = NULL;
    inode->i_private = NULL;

    /* Add to global list */
    list_add(&inode->i_list, &inode_cache.inodes);
    inode_cache.count++;

    return inode;
}

/**
 * @brief Free an inode
 */
void vfs_free_inode(vfs_inode_t* inode) {
    if (!inode) {
        return;
    }

    /* Remove from hash table */
    inode_hash_remove(inode);

    /* Remove from global list */
    list_del(&inode->i_list);
    inode_cache.count--;

    /* Call filesystem-specific destroy if exists */
    if (inode->i_sb && inode->i_sb->s_op &&
        inode->i_sb->s_op->destroy_inode) {
        inode->i_sb->s_op->destroy_inode(inode);
    }

    /* Free private data */
    if (inode->i_private) {
        kfree(inode->i_private);
    }

    kfree(inode);
}

/**
 * @brief Clear an inode (mark as unused)
 */
void vfs_clear_inode(vfs_inode_t* inode) {
    if (!inode) {
        return;
    }

    inode->i_state |= I_FREEING;
}

/* ============================================================================
 * Inode Cache Operations
 * ============================================================================ */

/**
 * @brief Get inode by number (from cache or read from disk)
 */
vfs_inode_t* vfs_iget(vfs_superblock_t* sb, uint64_t ino) {
    if (!sb) {
        return NULL;
    }

    uint32_t hash = inode_hash(ino, sb);

    /* Search in cache */
    vfs_inode_t* inode;
    list_for_each_entry(inode, &inode_cache.hash_table[hash], i_hash) {
        if (inode->i_sb == sb && inode->i_ino == ino) {
            /* Found in cache, increment reference */
            inode->i_count++;
            return inode;
        }
    }

    /* Not in cache, allocate new inode */
    inode = vfs_alloc_inode(sb);
    if (!inode) {
        return NULL;
    }

    inode->i_ino = ino;

    /* Read inode from filesystem */
    if (sb->s_op && sb->s_op->read_inode) {
        int result = sb->s_op->read_inode(sb, inode);
        if (result != 0) {
            vfs_free_inode(inode);
            klog_error("vfs: Failed to read inode %lu\n", ino);
            return NULL;
        }
    }

    /* Add to hash table */
    inode_hash_insert(inode);

    return inode;
}

/**
 * @brief Release inode reference
 */
void vfs_iput(vfs_inode_t* inode) {
    if (!inode) {
        return;
    }

    if (inode->i_count > 0) {
        inode->i_count--;
    }

    /* If no more references and inode is dirty, write back */
    if (inode->i_count == 0 && (inode->i_state & I_DIRTY)) {
        if (inode->i_sb && inode->i_sb->s_op &&
            inode->i_sb->s_op->write_inode) {
            inode->i_sb->s_op->write_inode(inode->i_sb, inode);
        }
    }

    /* Free inode if no references */
    if (inode->i_count == 0) {
        vfs_free_inode(inode);
    }
}

/**
 * @brief Mark inode as dirty
 */
void vfs_mark_inode_dirty(vfs_inode_t* inode) {
    if (inode) {
        inode->i_state |= I_DIRTY;
    }
}

/**
 * @brief Get file type from inode mode
 */
vtype_t vfs_mode_to_type(uint32_t mode) {
    switch (mode & V_S_IFMT) {
        case V_FIFO: return V_FIFO;
        case V_CHR:  return V_CHR;
        case V_DIR:  return V_DIR;
        case V_BLK:  return V_BLK;
        case V_REG:  return V_REG;
        case V_LNK:  return V_LNK;
        case V_SOCK: return V_SOCK;
        default:     return V_UNKNOWN;
    }
}

/**
 * @brief Check if inode is a directory
 */
bool vfs_is_dir(vfs_inode_t* inode) {
    return inode && vfs_mode_to_type(inode->i_mode) == V_DIR;
}

/**
 * @brief Check if inode is a regular file
 */
bool vfs_is_reg(vfs_inode_t* inode) {
    return inode && vfs_mode_to_type(inode->i_mode) == V_REG;
}

/**
 * @brief Check if inode is a symbolic link
 */
bool vfs_is_symlink(vfs_inode_t* inode) {
    return inode && vfs_mode_to_type(inode->i_mode) == V_LNK;
}

/* ============================================================================
 * Inode Cache Initialization
 * ============================================================================ */

/**
 * @brief Initialize inode cache
 */
void vfs_inode_cache_init(void) {
    /* Initialize hash table */
    for (int i = 0; i < INODE_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&inode_cache.hash_table[i]);
    }
    INIT_LIST_HEAD(&inode_cache.inodes);
    inode_cache.count = 0;
}
