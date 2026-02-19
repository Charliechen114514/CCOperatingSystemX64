/**
 * @file ext2_file.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief EXT2 File Operations
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

/* ============================================================================
 * File Read Operations
 * ============================================================================ */

/**
 * @brief Read file data using block mapping
 *
 * @param file File object
 * @param buffer Buffer to store data
 * @param count Number of bytes to read
 * @param pos File position (updated after read)
 * @return Number of bytes read, negative error code on failure
 */
ssize_t ext2_file_read(file_t* file, char* buffer, size_t count, uint64_t* pos) {
    if (!file || !buffer || !pos) {
        return -1;
    }

    vfs_inode_t* inode = file->f_inode;
    if (!inode) {
        return -1;
    }

    if (!vfs_is_reg(inode)) {
        return -1;  /* Not a regular file */
    }

    ext2_fs_info_t* fs_info = (ext2_fs_info_t*)inode->i_sb->s_fs_info;
    if (!fs_info) {
        return -1;
    }

    uint64_t file_size = inode->i_size;
    uint64_t current_pos = *pos;

    /* Check bounds */
    if (current_pos >= file_size) {
        return 0;  /* EOF */
    }

    /* Adjust count if beyond file size */
    uint64_t remaining = file_size - current_pos;
    if (count > remaining) {
        count = remaining;
    }

    uint32_t block_size = fs_info->block_size;

    /* Read data block by block */
    uint64_t bytes_read = 0;
    uint64_t total_read = 0;

    while (total_read < count) {
        uint64_t block_num;
        int result = ext2_get_block(inode, current_pos + total_read, &block_num);
        if (result < 0) {
            break;  /* Error */
        }
        if (result == 0 && block_num == 0) {
            /* Sparse file - zero fill */
            break;
        }

        /* Calculate offset within block */
        uint64_t block_offset = (current_pos + total_read) % block_size;
        uint32_t bytes_in_block = block_size - block_offset;
        uint32_t bytes_to_read = count - total_read;

        if (bytes_to_read > bytes_in_block) {
            bytes_to_read = bytes_in_block;
        }

        /* Read the block */
        char* block_data = (char*)kmalloc(block_size);
        if (!block_data) {
            break;
        }

        uint32_t sectors_per_block = block_size / 512;
        int read_result = block_read_sync(fs_info->device,
                                           block_num * sectors_per_block,
                                           block_data,
                                           sectors_per_block);

        if (read_result != (int)sectors_per_block) {
            kfree(block_data);
            klog_error("ext2: Failed to read data block %lu\n", block_num);
            break;
        }

        /* Copy to user buffer */
        memcpy(buffer + total_read, block_data + block_offset, bytes_to_read);

        kfree(block_data);

        bytes_read = bytes_to_read;
        total_read += bytes_read;
    }

    /* Update file position */
    *pos += total_read;

    return (ssize_t)total_read;
}

/**
 * @brief Write file data
 *
 * @param file File object
 * @param buffer Data to write
 * @param count Number of bytes to write
 * @param pos File position (updated after write)
 * @return Number of bytes written, negative error code on failure
 */
ssize_t ext2_file_write(file_t* file, const char* buffer, size_t count, uint64_t* pos) {
    (void)file;
    (void)buffer;
    (void)count;
    (void)pos;

    /* TODO: Implement write */
    klog_warn("ext2: File write not implemented yet\n");
    return -1;
}

/* ============================================================================
 * File Operations Table
 * ============================================================================ */

const file_operations_t ext2_file_ops = {
    .llseek = NULL,  /* Use default */
    .read = ext2_file_read,
    .write = ext2_file_write,
    .readdir = NULL,
    .ioctl = NULL,
    .open = NULL,
    .release = NULL,
};

const file_operations_t ext2_dir_ops = {
    .llseek = NULL,
    .read = NULL,  /* Directories can't be read like files */
    .write = NULL,
    .readdir = ext2_readdir,
    .ioctl = NULL,
    .open = NULL,
    .release = NULL,
};

/* ============================================================================
 * Inode Operations Table
 * ============================================================================ */

const inode_operations_t ext2_inode_ops = {
    .lookup = NULL,
    .create = NULL,
    .mkdir = NULL,
    .mknod = NULL,
    .unlink = NULL,
    .rmdir = NULL,
    .rename = NULL,
};

const inode_operations_t ext2_dir_inode_ops = {
    .lookup = ext2_lookup,
    .create = NULL,
    .mkdir = NULL,
    .mknod = NULL,
    .unlink = NULL,
    .rmdir = NULL,
    .rename = NULL,
};

/* ============================================================================
 * Superblock Operations Table
 * ============================================================================ */

const super_operations_t ext2_super_ops = {
    .read_inode = ext2_read_inode,
    .write_inode = ext2_write_inode,
    .statfs = ext2_statfs,
    .alloc_inode = ext2_alloc_inode,
    .destroy_inode = ext2_destroy_inode,
    .put_super = ext2_put_super,
};
