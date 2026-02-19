/**
 * @file ext2_inode.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief EXT2 Inode Operations
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
#include "base/memory.h"
#include "base/string.h"

/* ============================================================================
 * Block Addressing - Convert file offset to block number
 * ============================================================================ */

/**
 * @brief Get block number for a file offset
 *
 * Handles direct blocks, single, double, and triple indirection.
 *
 * @param inode VFS inode
 * @param offset File offset in bytes
 * @param block Output block number (0 if error)
 * @return 0 on success, -1 on error
 */
int ext2_get_block(vfs_inode_t* inode, uint64_t offset, uint64_t* block) {
    if (!inode || !block) {
        return -1;
    }

    ext2_inode_info_t* ei = (ext2_inode_info_t*)inode->i_private;
    if (!ei) {
        return -1;
    }

    ext2_fs_info_t* fs_info = (ext2_fs_info_t*)inode->i_sb->s_fs_info;
    if (!fs_info) {
        return -1;
    }

    uint32_t block_size = fs_info->block_size;
    uint32_t block_in_file = offset / block_size;

    /* Check if file is within size limits */
    if (offset >= inode->i_size) {
        return -1;
    }

    /* Direct blocks (0-11) */
    if (block_in_file < EXT2_NDIR_BLOCKS) {
        *block = ei->i_data[block_in_file];
        if (*block == 0) {
            /* Sparse file - hole */
            return 0;
        }
        return 0;
    }

    /* Indirect block (12) - single indirection */
    block_in_file -= EXT2_NDIR_BLOCKS;
    if (block_in_file < block_size / 4) {  /* Number of pointers per block */
        uint32_t* indirect = (uint32_t*)kmalloc(block_size);
        if (!indirect) {
            return -1;
        }

        /* Read indirect block */
        uint64_t ind_block = ei->i_data[EXT2_IND_BLOCK];
        uint32_t sectors_per_block = block_size / 512;

        if (ind_block == 0) {
            kfree(indirect);
            return -1;
        }

        int result = block_read_sync(fs_info->device,
                                     ind_block * sectors_per_block,
                                     indirect,
                                     sectors_per_block);
        if (result != (int)sectors_per_block) {
            kfree(indirect);
            return -1;
        }

        *block = indirect[block_in_file];
        kfree(indirect);

        if (*block == 0) {
            return 0;  /* Sparse */
        }
        return 0;
    }

    /* TODO: Double indirect block (13) */
    /* TODO: Triple indirect block (14) */

    klog_warn("ext2: Large file support not implemented yet\n");
    return -1;
}

/**
 * @brief Read inode from disk
 */
int ext2_read_inode(vfs_superblock_t* sb, vfs_inode_t* inode) {
    if (!sb || !inode) {
        return -1;
    }

    ext2_fs_info_t* fs_info = (ext2_fs_info_t*)sb->s_fs_info;
    if (!fs_info) {
        return -1;
    }

    uint64_t ino = inode->i_ino;
    if (ino == 0 || ino > fs_info->sb->s_inodes_count) {
        klog_error("ext2: Invalid inode number %lu\n", ino);
        return -1;
    }

    /* Calculate which block group contains this inode */
    uint32_t group = EXT2_INODE_GROUP(fs_info->sb, ino);
    if (group >= fs_info->ngroups) {
        klog_error("ext2: Invalid group %u for inode %lu (ngroups=%u)\n",
                   group, ino, fs_info->ngroups);
        return -1;
    }

    /* Get the inode table start block from the block group descriptor */
    ext2_block_group_desc_t* bgd = &fs_info->bg_desc[group];
    uint32_t inode_table_block = bgd->bg_inode_table;

    /* Calculate local inode index within the group */
    uint32_t index = EXT2_LOCAL_INODE(fs_info->sb, ino);

    /* Calculate offset within the inode table */
    uint32_t inode_offset = index * fs_info->inode_size;

    klog_trace("ext2: Reading inode %lu: group=%u, index=%u, table_block=%u, offset=%u\n",
               ino, group, index, inode_table_block, inode_offset);

    /* Allocate a block-sized buffer to read the entire block */
    uint32_t block_size = fs_info->block_size;
    uint8_t* block_buffer = (uint8_t*)kmalloc(block_size);
    if (!block_buffer) {
        klog_error("ext2: Failed to allocate block buffer\n");
        return -1;
    }
    memset(block_buffer, 0, block_size);

    /* Calculate which block within the inode table contains the inode */
    uint32_t table_block_offset = inode_offset / block_size;
    uint32_t inode_block = inode_table_block + table_block_offset;

    /* Calculate offset within the block */
    uint32_t offset_in_block = inode_offset % block_size;

    /* Convert block to sector (block device API uses sectors) */
    uint32_t sectors_per_block = block_size / 512;
    uint64_t sector = (uint64_t)inode_block * sectors_per_block;

    /* Read the entire block containing the inode */
    int result = block_read_sync(fs_info->device, sector, block_buffer, sectors_per_block);

    if (result != (int)sectors_per_block) {
        klog_error("ext2: Failed to read inode %lu (sector=%u)\n", ino, (uint32_t)sector);
        kfree(block_buffer);
        return -1;
    }

    /* Access the inode at the correct offset within the block */
    ext2_inode_t* ext2_inode = (ext2_inode_t*)(block_buffer + offset_in_block);

    /* Debug: print raw bytes at offset to verify data */
    klog_trace("ext2: Raw bytes at inode offset: %02X %02X %02X %02X\n",
               block_buffer[offset_in_block],
               block_buffer[offset_in_block + 1],
               block_buffer[offset_in_block + 2],
               block_buffer[offset_in_block + 3]);

    /* Fill VFS inode */
    inode->i_mode = ext2_inode->i_mode;
    /* Note: vfs_inode_t doesn't have i_uid/i_gid, skipping for now */
    inode->i_size = ext2_inode->i_size;
    inode->i_blocks = ext2_inode->i_blocks;
    inode->i_nlink = ext2_inode->i_links_count;

    /* Allocate private data and copy block pointers */
    ext2_inode_info_t* ei = (ext2_inode_info_t*)kmalloc(sizeof(ext2_inode_info_t));
    if (!ei) {
        klog_error("ext2: Failed to allocate inode private data\n");
        kfree(block_buffer);
        return -1;
    }

    memcpy(ei->i_data, ext2_inode->i_block, sizeof(ei->i_data));
    ei->i_flags = ext2_inode->i_flags;
    ei->i_file_acl = ext2_inode->i_file_acl;
    ei->i_dir_acl = ext2_inode->i_dir_acl;
    ei->i_dtime = ext2_inode->i_dtime;

    inode->i_private = ei;

    /* Free the block buffer */
    kfree(block_buffer);

    /* Debug: print inode mode */
    klog_trace("ext2: Inode %lu mode: 0x%04X\n", ino, inode->i_mode);

    /* Set operations based on file type */
    vtype_t type = vfs_mode_to_type(inode->i_mode);
    klog_trace("ext2: Inode %lu vfs type: %d\n", ino, type);

    if (type == V_DIR) {
        inode->i_op = &ext2_dir_inode_ops;
        inode->i_fop = &ext2_dir_ops;
    } else if (type == V_REG) {
        inode->i_op = NULL;
        inode->i_fop = &ext2_file_ops;
    } else if (type == V_LNK) {
        /* TODO: Implement symlinks */
        inode->i_op = NULL;
        inode->i_fop = NULL;
    } else {
        inode->i_op = NULL;
        inode->i_fop = NULL;
    }

    return 0;
}

/**
 * @brief Write inode to disk
 */
int ext2_write_inode(vfs_superblock_t* sb, vfs_inode_t* inode) {
    (void)sb;
    (void)inode;
    /* TODO: Implement inode write */
    klog_warn("ext2: inode write not implemented yet\n");
    return 0;
}

/**
 * @brief Allocate a new inode
 */
int ext2_alloc_inode(vfs_superblock_t* sb, vfs_inode_t* inode) {
    (void)sb;
    (void)inode;
    /* TODO: Implement inode allocation */
    klog_warn("ext2: inode allocation not implemented yet\n");
    return -1;
}

/**
 * @brief Destroy inode private data
 */
void ext2_destroy_inode(vfs_inode_t* inode) {
    if (inode && inode->i_private) {
        kfree(inode->i_private);
        inode->i_private = NULL;
    }
}
