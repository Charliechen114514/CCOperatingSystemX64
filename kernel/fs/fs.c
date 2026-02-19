/**
 * @file fs.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Main Filesystem Subsystem Implementation
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 */

#include "fs.h"
#include "klogs/kprintf.h"

/* External initialization functions */
extern int block_init(void);
extern int vfs_init(void);

/* EXT2 initialization */
extern int ext2_init(void);

/* ============================================================================
 * Filesystem Subsystem Initialization
 * ============================================================================ */

/**
 * @brief Initialize the filesystem subsystem
 */
int fs_init(void) {
    klog_info("fs: Initializing filesystem subsystem\n");

    /* Initialize block device layer */
    int result = block_init();
    if (result < 0) {
        klog_error("fs: Failed to initialize block device layer\n");
        return -1;
    }

    /* Initialize VFS */
    result = vfs_init();
    if (result != 0) {
        klog_error("fs: Failed to initialize VFS\n");
        return -1;
    }

    /* Initialize EXT2 filesystem driver */
    result = ext2_init();
    if (result != 0) {
        klog_error("fs: Failed to initialize EXT2 driver\n");
        /* Not fatal - system can still work */
    }

    klog_info("fs: Filesystem subsystem initialized successfully\n");

    return 0;
}

/**
 * @brief Get filesystem subsystem information
 */
void fs_info(void) {
    vfs_superblock_t* sb = vfs_get_root_sb();

    klog_info("=== Filesystem Subsystem Info ===\n");

    if (sb) {
        klog_info("Root filesystem: %s\n", sb->s_fsname);
        klog_info("  Block size: %lu bytes\n", sb->s_blocksize);
        klog_info("  Total blocks: %lu\n", sb->s_blocks);
        klog_info("  Free blocks: %lu\n", sb->s_bfree);
        klog_info("  Total inodes: %lu\n", sb->s_files);
        klog_info("  Free inodes: %lu\n", sb->s_ffree);
    } else {
        klog_info("No root filesystem mounted\n");
    }

    klog_info("=================================\n");
}
