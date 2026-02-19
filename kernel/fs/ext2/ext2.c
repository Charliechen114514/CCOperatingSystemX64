/**
 * @file ext2.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief EXT2 Filesystem Driver - Main Initialization
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 */

#include "ext2.h"
#include "vfs/vfs.h"
#include "klogs/kprintf.h"

/* ============================================================================
 * External mount function (from ext2_superblock.c)
 * ============================================================================ */

extern int ext2_mount(vfs_superblock_t* sb, const char* data);

/* ============================================================================
 * EXT2 Filesystem Registration
 * ============================================================================ */

/**
 * @brief Initialize EXT2 filesystem driver
 *
 * Registers the EXT2 filesystem type with VFS.
 *
 * @return 0 on success, negative error code on failure
 */
int ext2_init(void) {
    /* Register EXT2 filesystem type with VFS */
    int result = vfs_register_fs_type("ext2", ext2_mount);

    if (result == 0) {
        klog_info("ext2: EXT2 filesystem driver initialized\n");
    } else {
        klog_error("ext2: Failed to register filesystem type\n");
    }

    return result;
}
