/**
 * @file ext2_dir.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief EXT2 Directory Operations
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
 * Directory Operations
 * ============================================================================ */

/**
 * @brief Find an entry in a directory
 *
 * @param dir Directory inode
 * @param name Name to look up
 * @param result Output inode pointer
 * @return 0 on success, negative error code on failure
 */
int ext2_lookup(vfs_inode_t* dir, const char* name, vfs_inode_t** result) {
    if (!dir || !name || !result) {
        return -1;
    }

    if (!vfs_is_dir(dir)) {
        return -1;  /* Not a directory */
    }

    ext2_fs_info_t* fs_info = (ext2_fs_info_t*)dir->i_sb->s_fs_info;
    if (!fs_info) {
        return -1;
    }

    ext2_inode_info_t* ei = (ext2_inode_info_t*)dir->i_private;
    if (!ei) {
        return -1;
    }

    /* Get first data block of directory */
    uint64_t dir_block;
    if (ext2_get_block(dir, 0, &dir_block) != 0) {
        klog_error("ext2: Failed to get directory block\n");
        return -1;
    }

    if (dir_block == 0) {
        klog_error("ext2: Directory has no data blocks\n");
        return -1;
    }

    /* Read directory block */
    uint32_t block_size = fs_info->block_size;
    char* block_data = (char*)kmalloc(block_size);
    if (!block_data) {
        return -1;
    }

    uint32_t sectors_per_block = block_size / 512;
    int read_result = block_read_sync(fs_info->device,
                                      dir_block * sectors_per_block,
                                      block_data,
                                      sectors_per_block);

    if (read_result != (int)sectors_per_block) {
        kfree(block_data);
        klog_error("ext2: Failed to read directory block\n");
        return -1;
    }

    /* Parse directory entries */
    uint32_t offset = 0;
    bool found = false;

    while (offset < block_size - 12) {  /* Minimum entry size is 12 bytes */
        ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(block_data + offset);

        if (entry->inode == 0) {
            break;  /* End of entries */
        }

        if (entry->rec_len == 0) {
            break;  /* Invalid entry */
        }

        /* Check name length */
        uint16_t name_len = entry->name_len;
        if (name_len == 0 || name_len > 255) {
            offset += entry->rec_len;
            continue;
        }

        /* Compare name */
        if (strncmp(entry->name, name, name_len) == 0 &&
            strlen(name) == name_len) {
            /* Found! Create/read the inode */
            vfs_inode_t* inode = vfs_iget(dir->i_sb, entry->inode);
            if (inode) {
                *result = inode;
                found = true;
            }
            break;
        }

        offset += entry->rec_len;
    }

    kfree(block_data);

    return found ? 0 : -1;  /* -1 = not found */
}

/**
 * @brief Read directory entries
 *
 * @param file File object for directory
 * @param dirent Directory entry buffer
 * @param filldir Callback to fill entries
 * @return 0 on success, negative error code on failure
 */
int ext2_readdir(file_t* file, void* dirent, filldir_t filldir) {
    if (!file || !file->f_inode) {
        return -1;
    }

    if (!vfs_is_dir(file->f_inode)) {
        return -1;  /* Not a directory */
    }

    ext2_fs_info_t* fs_info = (ext2_fs_info_t*)file->f_inode->i_sb->s_fs_info;
    if (!fs_info) {
        return -1;
    }

    ext2_inode_info_t* ei = (ext2_inode_info_t*)file->f_inode->i_private;
    if (!ei) {
        return -1;
    }

    /* Get first data block */
    uint64_t dir_block;
    if (ext2_get_block(file->f_inode, 0, &dir_block) != 0) {
        return -1;
    }

    if (dir_block == 0) {
        return 0;  /* End of directory */
    }

    /* Read directory block */
    uint32_t block_size = fs_info->block_size;
    char* block_data = (char*)kmalloc(block_size);
    if (!block_data) {
        return -1;
    }

    uint32_t sectors_per_block = block_size / 512;
    int read_result = block_read_sync(fs_info->device,
                                      dir_block * sectors_per_block,
                                      block_data,
                                      sectors_per_block);

    if (read_result != (int)sectors_per_block) {
        kfree(block_data);
        klog_error("ext2: Failed to read directory block\n");
        return -1;
    }

    /* Parse directory entries and call filldir */
    uint32_t offset = 0;
    int entries = 0;

    while (offset < block_size - 12) {
        ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(block_data + offset);

        if (entry->inode == 0 || entry->rec_len == 0) {
            break;
        }

        uint16_t name_len = entry->name_len;
        if (name_len > 255) {
            name_len = 255;
        }

        /* Null-terminate name for safety */
        char name[256];
        memcpy(name, entry->name, name_len);
        name[name_len] = '\0';

        /* Call filldir callback */
        if (filldir(dirent, name, name_len,
                     file->f_pos, entry->inode,
                     entry->file_type) < 0) {
            break;
        }

        file->f_pos += entry->rec_len;
        entries++;

        offset += entry->rec_len;
    }

    kfree(block_data);

    return 0;
}
