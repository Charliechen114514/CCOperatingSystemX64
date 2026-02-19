/**
 * @file vfs_superblock.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VFS Superblock Management
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
 * @brief Global VFS state
 */
static struct {
    list_head superblocks;         /* List of all superblocks */
    vfs_superblock_t* root_sb;     /* Root filesystem superblock */
    uint32_t sb_count;             /* Number of superblocks */
} vfs_state;

/* ============================================================================
 * Superblock Management
 * ============================================================================ */

/**
 * @brief Allocate a new superblock
 */
vfs_superblock_t* vfs_alloc_super(void) {
    vfs_superblock_t* sb = (vfs_superblock_t*)kmalloc(sizeof(vfs_superblock_t));
    if (!sb) {
        klog_error("vfs: Failed to allocate superblock\n");
        return NULL;
    }

    /* Initialize superblock */
    memset(sb, 0, sizeof(vfs_superblock_t));
    INIT_LIST_HEAD(&sb->s_list);
    INIT_LIST_HEAD(&sb->s_mounts);

    sb->s_blocksize = 0;
    sb->s_magic = 0;
    sb->s_root = NULL;
    sb->s_fs_info = NULL;
    sb->s_op = NULL;
    sb->s_flags = 0;
    sb->s_count = 1;
    sb->s_mount = NULL;
    sb->s_mount_instance = NULL;

    return sb;
}

/**
 * @brief Free a superblock
 */
void vfs_free_super(vfs_superblock_t* sb) {
    if (!sb) {
        return;
    }

    /* Call filesystem's put_super if exists */
    if (sb->s_op && sb->s_op->put_super) {
        sb->s_op->put_super(sb);
    }

    /* Free filesystem-specific data */
    if (sb->s_fs_info) {
        kfree(sb->s_fs_info);
    }

    kfree(sb);
}

/**
 * @brief Add superblock to global list
 */
void vfs_add_super(vfs_superblock_t* sb) {
    if (!sb) {
        return;
    }

    list_add_tail(&sb->s_list, &vfs_state.superblocks);
    vfs_state.sb_count++;

    klog_info("vfs: Added superblock (magic=0x%x, fsname=%s)\n",
              sb->s_magic, sb->s_fsname);
}

/**
 * @brief Remove superblock from global list
 */
void vfs_remove_super(vfs_superblock_t* sb) {
    if (!sb) {
        return;
    }

    list_del(&sb->s_list);
    vfs_state.sb_count--;

    if (vfs_state.root_sb == sb) {
        vfs_state.root_sb = NULL;
    }
}

/**
 * @brief Get root superblock
 */
vfs_superblock_t* vfs_get_root_sb(void) {
    return vfs_state.root_sb;
}

/**
 * @brief Set root superblock
 */
void vfs_set_root_sb(vfs_superblock_t* sb) {
    vfs_state.root_sb = sb;
    klog_info("vfs: Set root superblock (fsname=%s)\n", sb ? sb->s_fsname : "NULL");
}

/**
 * @brief Find superblock by device
 */
vfs_superblock_t* vfs_find_super(uint32_t dev) {
    vfs_superblock_t* sb;

    list_for_each_entry(sb, &vfs_state.superblocks, s_list) {
        if (sb->s_dev == dev) {
            return sb;
        }
    }

    return NULL;
}

/**
 * @brief Get filesystem statistics
 */
int vfs_statfs(const char* path, struct statfs* buf) {
    (void)path;  /* Unused for now */
    if (!buf) {
        return -1;  /* EINVAL */
    }

    /* For now, use root superblock */
    vfs_superblock_t* sb = vfs_get_root_sb();
    if (!sb) {
        return -1;  /* ENOENT */
    }

    memset(buf, 0, sizeof(struct statfs));

    if (sb->s_op && sb->s_op->statfs) {
        return sb->s_op->statfs(sb, buf);
    }

    /* Fill in default values */
    buf->f_type = sb->s_magic;
    buf->f_bsize = sb->s_blocksize;
    buf->f_blocks = sb->s_blocks;
    buf->f_bfree = sb->s_bfree;
    buf->f_files = sb->s_files;
    buf->f_ffree = sb->s_ffree;

    return 0;
}

/* ============================================================================
 * VFS Initialization
 * ============================================================================ */

/**
 * @brief Initialize VFS subsystem
 */
int vfs_init(void) {
    INIT_LIST_HEAD(&vfs_state.superblocks);
    vfs_state.root_sb = NULL;
    vfs_state.sb_count = 0;

    /* Initialize VFS caches */
    vfs_inode_cache_init();
    vfs_dentry_cache_init();
    vfs_file_cache_init();
    vfs_mount_table_init();

    klog_info("vfs: VFS subsystem initialized\n");

    return 0;
}
