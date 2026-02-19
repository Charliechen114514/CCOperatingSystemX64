/**
 * @file ext2_internal.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief EXT2 Filesystem On-Disk Data Structures
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 *
 * This file contains the on-disk structure definitions for the EXT2 filesystem.
 * These structures match the binary layout on disk.
 */

#pragma once

#include "defines/types.h"

/* Forward declaration */
typedef struct block_device block_device_t;

/* ============================================================================
 * EXT2 Superblock
 * ============================================================================ */

/**
 * @brief EXT2 Superblock
 *
 * Located at byte offset 1024 of the device (block 2 for 1024-byte blocks).
 * The superblock is replicated in each block group.
 */
/**
 * @brief EXT2 Superblock (aligned with Linux kernel fs/ext2/ext2.h)
 *
 * Located at byte offset 1024 of the device (block 2 for 1024-byte blocks).
 * The superblock is replicated in each block group.
 *
 * This structure matches the on-disk format used by mkfs.ext2/e2fsprogs.
 */
typedef struct PACKED ext2_superblock {
    uint32_t s_inodes_count;       /* Inodes count */
    uint32_t s_blocks_count;       /* Blocks count */
    uint32_t s_r_blocks_count;     /* Reserved blocks count */
    uint32_t s_free_blocks_count;  /* Free blocks count */
    uint32_t s_free_inodes_count;  /* Free inodes count */
    uint32_t s_first_data_block;   /* First Data Block */
    uint32_t s_log_block_size;     /* Block size = 1024 << s_log_block_size */
    uint32_t s_log_frag_size;      /* Fragment size */
    uint32_t s_blocks_per_group;   /* # Blocks per group */
    uint32_t s_frags_per_group;    /* # Fragments per group */
    uint32_t s_inodes_per_group;   /* # Inodes per group */
    uint32_t s_mtime;              /* Mount time */
    uint32_t s_wtime;              /* Write time */
    uint16_t s_mnt_count;          /* Mount count */
    uint16_t s_max_mnt_count;      /* Maximal mount count */
    uint16_t s_magic;              /* Magic signature (0xEF53) at offset 56 */
    uint16_t s_state;              /* File system state */
    uint16_t s_errors;             /* Behaviour when detecting errors */
    uint16_t s_minor_rev_level;    /* minor revision level */
    uint32_t s_lastcheck;          /* time of last check */
    uint32_t s_checkinterval;      /* max. time between checks */
    uint32_t s_creator_os;         /* OS */
    uint32_t s_rev_level;          /* Revision level */
    uint16_t s_def_resuid;         /* Default uid for reserved blocks */
    uint16_t s_def_resgid;         /* Default gid for reserved blocks */
    /* EXT2_DYNAMIC_REV superblocks only: */
    uint32_t s_first_ino;          /* First non-reserved inode */
    uint16_t s_inode_size;         /* size of inode structure */
    uint16_t s_block_group_nr;     /* block group # of this superblock */
    uint32_t s_feature_compat;     /* compatible feature set */
    uint32_t s_feature_incompat;   /* incompatible feature set */
    uint32_t s_feature_ro_compat;  /* readonly-compatible feature set */
    uint8_t  s_uuid[16];           /* 128-bit uuid for volume */
    char     s_volume_name[16];    /* volume name */
    char     s_last_mounted[64];   /* directory where last mounted */
    uint32_t s_algorithm_usage_bitmap; /* For compression */
    /* Performance hints: */
    uint8_t  s_prealloc_blocks;    /* Nr of blocks to try to preallocate */
    uint8_t  s_prealloc_dir_blocks;/* Nr to preallocate for dirs */
    uint16_t s_padding1;           /* Padding */
    /* Journaling support valid if EXT3_FEATURE_COMPAT_HAS_JOURNAL set: */
    uint8_t  s_journal_uuid[16];   /* uuid of journal superblock */
    uint32_t s_journal_inum;       /* inode number of journal file */
    uint32_t s_journal_dev;        /* device number of journal file */
    uint32_t s_last_orphan;        /* start of list of inodes to delete */
    uint32_t s_hash_seed[4];       /* HTREE hash seed */
    uint8_t  s_def_hash_version;   /* Default hash version to use */
    uint8_t  s_reserved_char_pad;
    uint16_t s_reserved_word_pad;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;      /* First metablock block group */
    uint32_t s_reserved[190];      /* Padding to the end of the block */
} ext2_superblock_t;

/* ============================================================================
 * EXT2 Block Group Descriptor
 * ============================================================================ */

/**
 * @brief EXT2 Block Group Descriptor
 *
 * Describes a block group. Located after the superblock (and backup superblocks).
 */
typedef struct PACKED ext2_block_group_desc {
    uint32_t bg_block_bitmap;      /* Block bitmap block */
    uint32_t bg_inode_bitmap;      /* Inode bitmap block */
    uint32_t bg_inode_table;       /* Inode table block */
    uint16_t bg_free_blocks_count; /* Free blocks */
    uint16_t bg_free_inodes_count; /* Free inodes */
    uint16_t bg_used_dirs_count;   /* Used directories */
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} ext2_block_group_desc_t;

/* ============================================================================
 * EXT2 Inode
 * ============================================================================ */

/**
 * @brief EXT2 Inode (on-disk)
 *
 * Each inode is 128 bytes by default. Contains file metadata and block pointers.
 */
typedef struct PACKED ext2_inode {
    uint16_t i_mode;               /* File mode (type + permissions) */
    uint16_t i_uid;                /* Owner UID */
    uint32_t i_size;               /* Size in bytes (low 32 bits) */
    uint32_t i_atime;              /* Access time */
    uint32_t i_ctime;              /* Change time */
    uint32_t i_mtime;              /* Modification time */
    uint32_t i_dtime;              /* Deletion time */
    uint16_t i_gid;                /* Group GID */
    uint16_t i_links_count;        /* Hard link count */
    uint32_t i_blocks;             /* Number of 512-byte blocks */
    uint32_t i_flags;              /* Flags */
    uint32_t i_osd1;               /* OS dependent 1 */
    uint32_t i_block[15];          /* Block pointers */
    uint32_t i_generation;         /* Generation (for NFS) */
    uint32_t i_file_acl;           /* File ACL */
    uint32_t i_dir_acl;            /* Directory ACL */
    uint32_t i_faddr;              /* Fragment address */
    uint8_t  i_osd2[12];           /* OS dependent 2 */
} ext2_inode_t;

/* ============================================================================
 * EXT2 Directory Entry
 * ============================================================================ */

/**
 * @brief EXT2 Directory Entry
 *
 * Variable length structure. Directory entries are 4-byte aligned.
 */
typedef struct PACKED ext2_dir_entry {
    uint32_t inode;                /* Inode number */
    uint16_t rec_len;              /* Record length (including this entry) */
    uint16_t name_len;             /* Name length */
    uint8_t  file_type;            /* File type (if enabled) */
    char     name[];               /* Name (variable length) */
} ext2_dir_entry_t;

/* ============================================================================
 * EXT2 Constants
 * ============================================================================ */

/* Superblock */
#define EXT2_SUPERBLOCK_OFFSET     1024        /* Byte offset of superblock */
#define EXT2_SUPERBLOCK_MAGIC      0xEF53      /* Magic number */
#define EXT2_GOOD_OLD_INODE_SIZE   128
#define EXT2_GOOD_OLD_REV          0

/* Block sizes */
#define EXT2_MIN_BLOCK_SIZE       1024
#define EXT2_MAX_BLOCK_SIZE       4096

/* Inode numbers */
#define EXT2_ROOT_INO             2           /* Root inode */
#define EXT2_BAD_INO              1           /* Bad blocks inode */
#define EXT2_GOOD_OLD_FIRST_INO   11

/* Block pointers */
#define EXT2_NDIR_BLOCKS           12          /* Direct blocks */
#define EXT2_IND_BLOCK             12          /* Indirect block */
#define EXT2_DIND_BLOCK            13          /* Double indirect block */
#define EXT2_TIND_BLOCK            14          /* Triple indirect block */

/* File types in directory entry */
#define EXT2_FT_UNKNOWN            0
#define EXT2_FT_REG_FILE           1
#define EXT2_FT_DIR                2
#define EXT2_FT_CHRDEV             3
#define EXT2_FT_BLKDEV             4
#define EXT2_FT_FIFO               5
#define EXT2_FT_SOCK               6
#define EXT2_FT_SYMLINK            7

/* File type masks */
#define EXT2_S_IFMT               0xF000      /* File type mask */

/* Feature flags */
#define EXT2_FEATURE_COMPAT_DIR_PREALLOC  0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES 0x0002
#define EXT2_FEATURE_COMPAT_HAS_JOURNAL   0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR      0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INODE  0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX     0x0020

#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER  0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE    0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR     0x0004

#define EXT2_FEATURE_INCOMPAT_COMPRESSION   0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE      0x0002
#define EXT2_FEATURE_INCOMPAT_RECOVER       0x0004
#define EXT2_FEATURE_INCOMPAT_JOURNAL_DEV   0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG       0x0010

/* ============================================================================
 * EXT2 Filesystem-Specific Data
 * ============================================================================ */

/**
 * @brief EXT2 filesystem private data
 *
 * Stored in vfs_superblock.s_fs_info
 */
typedef struct ext2_fs_info {
    ext2_superblock_t* sb;        /* Copy of superblock (in memory) */
    ext2_block_group_desc_t* bg_desc;  /* Block group descriptors (in memory) */
    uint32_t ngroups;             /* Number of block groups */
    uint32_t block_size;          /* Block size in bytes */
    uint32_t inode_size;          /* Inode size in bytes */
    uint32_t inodes_per_group;    /* Inodes per group */
    uint32_t blocks_per_group;    /* Blocks per group */
    uint32_t inode_table_start;   /* Starting block of inode table */
    block_device_t* device;       /* Block device */
    uint32_t first_data_block;    /* First data block number */
} ext2_fs_info_t;

/**
 * @brief EXT2 inode private data
 *
 * Stored in vfs_inode.i_private
 */
typedef struct ext2_inode_info {
    uint32_t i_data[15];          /* Block pointers from disk */
    uint32_t i_flags;             /* Flags from disk */
    uint32_t i_file_acl;           /* File ACL */
    uint32_t i_dir_acl;            /* Directory ACL */
    uint32_t i_dtime;             /* Deletion time */
    uint32_t i_block_alloc;       /* Block allocation hint */
} ext2_inode_info_t;

/* ============================================================================
 * Helper Macros
 * ============================================================================ */

/**
 * @brief Get block size from superblock
 */
#define EXT2_BLOCK_SIZE(sb) (1024 << (sb)->s_log_block_size)

/**
 * @brief Calculate block group containing a given block
 */
#define EXT2_BLOCK_GROUP(sb, block) ((block) / (sb)->s_blocks_per_group)

/**
 * @brief Calculate block group containing a given inode
 */
#define EXT2_INODE_GROUP(sb, inode) (((inode) - 1) / (sb)->s_inodes_per_group)

/**
 * @brief Calculate local block index within group
 */
#define EXT2_LOCAL_BLOCK(sb, block) ((block) % (sb)->s_blocks_per_group)

/**
 * @brief Calculate local inode index within group
 */
#define EXT2_LOCAL_INODE(sb, inode) (((inode) - 1) % (sb)->s_inodes_per_group)

/**
 * @brief Calculate number of block group descriptor blocks needed
 *
 * This calculates how many blocks are needed to store all block group descriptors.
 * Each descriptor is sizeof(ext2_block_group_desc_t) bytes.
 *
 * Note: This macro requires fs_info->ngroups to be set first, or pass ngroups explicitly.
 */
#define EXT2_BG_DESC_BLOCKS(sb, ngroups) ((((ngroups) * sizeof(ext2_block_group_desc_t)) + \
                                           EXT2_BLOCK_SIZE(sb) - 1) / EXT2_BLOCK_SIZE(sb))

/**
 * @brief Calculate size of inode table in blocks
 */
#define EXT2_INODE_TABLE_BLOCKS(sb) (((sb)->s_inodes_per_group * sizeof(ext2_inode_t) + \
                                  EXT2_BLOCK_SIZE(sb) - 1) / EXT2_BLOCK_SIZE(sb))
