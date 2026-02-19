/**
 * @file ext2_superblock.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief EXT2 Superblock Operations
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 */

#include "ext2_internal.h"
#include "ext2.h"
#include "block/block.h"
#include "vfs/vfs.h"
#include "klogs/kprintf.h"
#include "mm/heap/heap.h"
#include "base/string.h"
#include "base/memory.h"

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Read EXT2 superblock from device
 *
 * @param dev Block device
 * @param ext2_sb Buffer to store superblock
 * @return 0 on success, negative error code on failure
 */
static int ext2_read_sb_raw(block_device_t* dev, ext2_superblock_t* ext2_sb) {
    if (!dev || !ext2_sb) {
        return -1;
    }

    /* EXT2 superblock is at byte offset 1024 from device start
     * s_magic is at offset 56 (0x38) within the superblock
     * So absolute offset of s_magic is 1024 + 56 = 1080 bytes */
    uint64_t sector = EXT2_SUPERBLOCK_OFFSET / 512;  /* Sector 2 (1024 / 512) */

    /* Read two sectors (1024 bytes) - enough for the full superblock */
    int result = block_read_sync(dev, sector, ext2_sb, 2);
    if (result != 2) {
        klog_error("ext2: Failed to read superblock (got %d sectors)\n", result);
        return -1;
    }

    /* Debug: print raw bytes at magic number location
     * s_magic is at offset 56 (0x38) within the superblock structure */
    uint8_t* raw = (uint8_t*)ext2_sb;
    klog_trace("ext2: Superblock bytes at offset 56-57 (s_magic): %02x %02x\n",
               raw[56], raw[57]);
    klog_trace("ext2: s_magic value (as uint16_t): 0x%x\n", ext2_sb->s_magic);

    /* Validate magic number (0xEF53 in little-endian)
     * On disk: 53 EF
     * As uint16_t: 0xEF53 */
    uint16_t magic = ext2_sb->s_magic;
    if (magic != EXT2_SUPERBLOCK_MAGIC) {
        klog_error("ext2: Invalid magic number 0x%x (expected 0x%x)\n",
                   magic, EXT2_SUPERBLOCK_MAGIC);
        klog_error("ext2: Raw bytes at offset 56-57: %02x %02x\n",
                   raw[56], raw[57]);
        return -1;
    }

    return 0;
}

/**
 * @brief Read block group descriptors
 *
 * Block group descriptors start immediately after the superblock.
 * For 1024-byte block size: superblock at block 1, descriptors start at block 2
 * For larger block sizes: superblock at block 0, descriptors start at block 1
 *
 * @param fs_info EXT2 filesystem info structure
 * @return 0 on success, negative error code on failure
 */
static int ext2_read_bg_desc(ext2_fs_info_t* fs_info) {
    if (!fs_info) {
        return -1;
    }

    ext2_superblock_t* ext2_sb = fs_info->sb;
    uint32_t block_size = fs_info->block_size;
    uint32_t ngroups = fs_info->ngroups;

    /* Calculate number of block group descriptor blocks needed */
    uint32_t bg_desc_blocks = EXT2_BG_DESC_BLOCKS(ext2_sb, ngroups);
    uint32_t bg_desc_size = bg_desc_blocks * block_size;

    klog_trace("ext2: Block group descriptors: ngroups=%u, blocks=%u, size=%u\n",
               ngroups, bg_desc_blocks, bg_desc_size);

    /* Allocate memory for block group descriptors */
    ext2_block_group_desc_t* bg_desc = (ext2_block_group_desc_t*)kmalloc(bg_desc_size);
    if (!bg_desc) {
        klog_error("ext2: Failed to allocate block group descriptors (%u bytes)\n", bg_desc_size);
        return -1;
    }

    memset(bg_desc, 0, bg_desc_size);

    /* Calculate starting block for block group descriptors
     * They start immediately after the superblock
     * Superblock is at byte offset 1024 = block 1 for 1K blocks, block 0 for larger blocks */
    uint32_t start_block;
    if (block_size == 1024) {
        /* Superblock occupies block 1, descriptors start at block 2 */
        start_block = 2;
    } else {
        /* Superblock at offset 1024 is within block 0, descriptors start at block 1 */
        start_block = 1;
    }

    /* Convert to 512-byte sectors */
    uint32_t sectors_per_block = block_size / 512;
    uint64_t start_sector = (uint64_t)start_block * sectors_per_block;
    uint32_t num_sectors = bg_desc_blocks * sectors_per_block;

    klog_trace("ext2: Reading bg descriptors: start_block=%u, start_sector=%u, num_sectors=%u\n",
               start_block, (uint32_t)start_sector, num_sectors);

    int result = block_read_sync(fs_info->device, start_sector, bg_desc, num_sectors);

    if (result != (int)num_sectors) {
        klog_error("ext2: Failed to read block group descriptors (got %d, expected %d)\n",
                   result, num_sectors);
        kfree(bg_desc);
        return -1;
    }

    fs_info->bg_desc = bg_desc;

    klog_info("ext2: Read %u block group descriptors (%u blocks)\n",
              ngroups, bg_desc_blocks);

    return 0;
}

/* ============================================================================
 * EXT2 Mount Operation
 * ============================================================================ */

/**
 * @brief Mount EXT2 filesystem
 *
 * This is called by vfs_mount() when mounting an EXT2 filesystem.
 *
 * @param sb VFS superblock to populate
 * @param data Mount options (not used yet)
 * @return 0 on success, negative error code on failure
 */
int ext2_mount(vfs_superblock_t* sb, const char* data) {
    (void)data;

    if (!sb) {
        return -1;
    }

    /* Get block device */
    block_device_t* dev = block_device_get(sb->s_dev);
    if (!dev) {
        klog_error("ext2: Block device %d not found\n", sb->s_dev);
        return -1;
    }

    /* Allocate EXT2 filesystem info */
    ext2_fs_info_t* fs_info = (ext2_fs_info_t*)kmalloc(sizeof(ext2_fs_info_t));
    if (!fs_info) {
        klog_error("ext2: Failed to allocate fs info\n");
        block_device_put(dev);
        return -1;
    }

    memset(fs_info, 0, sizeof(ext2_fs_info_t));
    fs_info->device = dev;

    /* Allocate superblock copy */
    ext2_superblock_t* ext2_sb = (ext2_superblock_t*)kmalloc(sizeof(ext2_superblock_t));
    if (!ext2_sb) {
        klog_error("ext2: Failed to allocate superblock\n");
        kfree(fs_info);
        block_device_put(dev);
        return -1;
    }

    /* Read superblock from disk */
    if (ext2_read_sb_raw(dev, ext2_sb) != 0) {
        kfree(ext2_sb);
        kfree(fs_info);
        block_device_put(dev);
        return -1;
    }

    fs_info->sb = ext2_sb;

    /* Calculate filesystem parameters */
    fs_info->block_size = EXT2_BLOCK_SIZE(ext2_sb);
    fs_info->inode_size = ext2_sb->s_inode_size ? ext2_sb->s_inode_size : EXT2_GOOD_OLD_INODE_SIZE;

    klog_info("ext2: Block size: %u, Inode size: %u\n", fs_info->block_size, fs_info->inode_size);
    klog_info("ext2: Blocks per group: %u, Inodes per group: %u\n",
              ext2_sb->s_blocks_per_group, ext2_sb->s_inodes_per_group);

    /* Validate revision level */
    klog_info("ext2: Revision level: 0x%x\n", ext2_sb->s_rev_level);
    if (ext2_sb->s_rev_level == EXT2_GOOD_OLD_REV) {
        /* Old revision - some fields may not be present */
        klog_info("ext2: Old revision filesystem detected\n");
    }

    /* Log feature flags */
    klog_trace("ext2: Features: compat=0x%x, incompat=0x%x, ro_compat=0x%x\n",
               ext2_sb->s_feature_compat,
               ext2_sb->s_feature_incompat,
               ext2_sb->s_feature_ro_compat);

    /* Check for incompatible features we don't support */
    if (ext2_sb->s_feature_incompat & ~(EXT2_FEATURE_INCOMPAT_FILETYPE)) {
        klog_warn("ext2: Warning - unsupported incompatible features: 0x%x\n",
                  ext2_sb->s_feature_incompat & ~EXT2_FEATURE_INCOMPAT_FILETYPE);
    }

    /* Calculate number of block groups */
    uint32_t blocks_per_group = ext2_sb->s_blocks_per_group;
    uint32_t blocks_count = ext2_sb->s_blocks_count;
    fs_info->ngroups = (blocks_count + blocks_per_group - 1) / blocks_per_group;

    klog_info("ext2: Calculated number of block groups: %u\n", fs_info->ngroups);

    fs_info->inodes_per_group = ext2_sb->s_inodes_per_group;
    fs_info->blocks_per_group = blocks_per_group;

    /* Read block group descriptors */
    if (ext2_read_bg_desc(fs_info) != 0) {
        kfree(ext2_sb);
        kfree(fs_info);
        block_device_put(dev);
        return -1;
    }

    /* Set first data block */
    fs_info->first_data_block = (ext2_sb->s_first_data_block != 0) ?
                                   ext2_sb->s_first_data_block :
                                   (ext2_sb->s_log_block_size > 0) ? 0 : 1;

    /* Store in VFS superblock */
    sb->s_blocksize = fs_info->block_size;
    sb->s_magic = EXT2_SUPERBLOCK_MAGIC;
    strcpy(sb->s_fsname, "ext2");
    sb->s_fs_info = fs_info;

    /* Update statistics */
    sb->s_blocks = ext2_sb->s_blocks_count;
    sb->s_bfree = ext2_sb->s_free_blocks_count;
    sb->s_files = ext2_sb->s_inodes_count;
    sb->s_ffree = ext2_sb->s_free_inodes_count;

    /* Set superblock operations (required for vfs_iget to work) */
    sb->s_op = &ext2_super_ops;

    /* Read and set root inode (EXT2 root inode is always inode 2) */
    klog_trace("ext2: Reading root inode (inode %d)...\n", EXT2_ROOT_INO);
    vfs_inode_t* root_inode = vfs_iget(sb, EXT2_ROOT_INO);
    if (!root_inode) {
        klog_error("ext2: Failed to read root inode\n");
        kfree(ext2_sb);
        kfree(fs_info);
        block_device_put(dev);
        return -1;
    }

    /* Verify root inode is a directory */
    if (!vfs_is_dir(root_inode)) {
        klog_error("ext2: Root inode is not a directory!\n");
        vfs_iput(root_inode);
        kfree(ext2_sb);
        kfree(fs_info);
        block_device_put(dev);
        return -1;
    }

    sb->s_root = root_inode;
    klog_info("ext2: Root inode set (ino=%lu, size=%lu)\n",
              root_inode->i_ino, root_inode->i_size);

    klog_info("ext2: Mounted EXT2 filesystem\n");
    klog_info("ext2:   Block size: %u bytes\n", fs_info->block_size);
    klog_info("ext2:   Total blocks: %u\n", ext2_sb->s_blocks_count);
    klog_info("ext2:   Free blocks: %u\n", ext2_sb->s_free_blocks_count);
    klog_info("ext2:   Total inodes: %u\n", ext2_sb->s_inodes_count);
    klog_info("ext2:   Free inodes: %u\n", ext2_sb->s_free_inodes_count);
    klog_info("ext2:   Block groups: %u\n", fs_info->ngroups);

    /* Keep device reference (don't call block_device_put) */

    return 0;
}

/**
 * @brief Unmount EXT2 filesystem
 *
 * @param sb VFS superblock
 */
void ext2_put_super(vfs_superblock_t* sb) {
    if (!sb) {
        return;
    }

    ext2_fs_info_t* fs_info = (ext2_fs_info_t*)sb->s_fs_info;
    if (!fs_info) {
        return;
    }

    klog_info("ext2: Unmounting EXT2 filesystem\n");

    /* Free block group descriptors */
    if (fs_info->bg_desc) {
        kfree(fs_info->bg_desc);
    }

    /* Free superblock copy */
    if (fs_info->sb) {
        kfree(fs_info->sb);
    }

    /* Put block device reference */
    if (fs_info->device) {
        block_device_put(fs_info->device);
    }

    /* Free fs info */
    kfree(fs_info);
    sb->s_fs_info = NULL;
}

/**
 * @brief Get EXT2 filesystem statistics
 */
int ext2_statfs(vfs_superblock_t* sb, struct statfs* buf) {
    if (!sb || !buf) {
        return -1;
    }

    ext2_fs_info_t* fs_info = (ext2_fs_info_t*)sb->s_fs_info;
    if (!fs_info) {
        return -1;
    }

    ext2_superblock_t* ext2_sb = fs_info->sb;

    buf->f_type = EXT2_SUPERBLOCK_MAGIC;
    buf->f_bsize = fs_info->block_size;
    buf->f_blocks = ext2_sb->s_blocks_count;
    buf->f_bfree = ext2_sb->s_free_blocks_count;
    buf->f_files = ext2_sb->s_inodes_count;
    buf->f_ffree = ext2_sb->s_free_inodes_count;
    buf->f_namelen = 255;  /* EXT2 max filename length */

    return 0;
}
