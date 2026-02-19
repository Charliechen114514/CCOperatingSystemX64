/**
 * @file ext2.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief EXT2 Filesystem Driver Public Interface
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include "defines/types.h"
#include "vfs/vfs.h"

/* Forward declarations */
typedef struct ext2_fs_info ext2_fs_info_t;
typedef struct ext2_inode_info ext2_inode_info_t;

/* From ext2_internal.h - needed by other files */
extern uint32_t ext2_bg_desc_blocks_cache;

/* ============================================================================
 * EXT2 Operations (for VFS)
 * ============================================================================ */

/**
 * @brief Mount EXT2 filesystem
 * @param sb VFS superblock
 * @param data Mount options
 * @return 0 on success, negative error code on failure
 */
int ext2_mount(vfs_superblock_t* sb, const char* data);

/**
 * @brief Unmount EXT2 filesystem
 * @param sb VFS superblock
 */
void ext2_put_super(vfs_superblock_t* sb);

/**
 * @brief Get filesystem statistics
 * @param sb VFS superblock
 * @param buf Statistics buffer
 * @return 0 on success, negative error code on failure
 */
int ext2_statfs(vfs_superblock_t* sb, struct statfs* buf);

/**
 * @brief Read inode from disk
 * @param sb VFS superblock
 * @param inode VFS inode to populate
 * @return 0 on success, negative error code on failure
 */
int ext2_read_inode(vfs_superblock_t* sb, vfs_inode_t* inode);

/**
 * @brief Write inode to disk
 * @param sb VFS superblock
 * @param inode VFS inode to write
 * @return 0 on success, negative error code on failure
 */
int ext2_write_inode(vfs_superblock_t* sb, vfs_inode_t* inode);

/**
 * @brief Allocate a new EXT2 inode
 * @param sb VFS superblock
 * @param inode VFS inode to populate
 * @return 0 on success, negative error code on failure
 */
int ext2_alloc_inode(vfs_superblock_t* sb, vfs_inode_t* inode);

/**
 * @brief Destroy an EXT2 inode
 * @param inode VFS inode to destroy
 */
void ext2_destroy_inode(vfs_inode_t* inode);

/**
 * @brief Get block number for a file offset
 * @param inode VFS inode
 * @param offset File offset in bytes
 * @param block Output block number
 * @return 0 on success, -1 on error
 */
int ext2_get_block(vfs_inode_t* inode, uint64_t offset, uint64_t* block);

/**
 * @brief Look up directory entry
 * @param dir Directory inode
 * @param name Name to look up
 * @param result Resulting inode
 * @return 0 on success, negative error code on failure
 */
int ext2_lookup(vfs_inode_t* dir, const char* name, vfs_inode_t** result);

/**
 * @brief Read directory entries
 * @param file File object for directory
 * @param dirent Directory entry buffer
 * @param filldir Callback to fill entries
 * @return 0 on success, negative error code on failure
 */
int ext2_readdir(file_t* file, void* dirent, filldir_t filldir);

/* File operations */
extern const file_operations_t ext2_file_ops;
extern const file_operations_t ext2_dir_ops;
extern const inode_operations_t ext2_inode_ops;
extern const inode_operations_t ext2_dir_inode_ops;
extern const super_operations_t ext2_super_ops;
