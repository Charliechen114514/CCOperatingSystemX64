/**
 * @file vfs.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Virtual File System (VFS) - Main public interface
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 *
 * This module provides the Virtual File System layer that abstracts
 * multiple filesystem implementations (EXT2, etc.) behind a common interface.
 */

#pragma once

#include "defines/types.h"
#include "list/list.h"

/* Forward declarations */
typedef struct vfs_superblock vfs_superblock_t;
typedef struct vfs_inode vfs_inode_t;
typedef struct vfs_dentry vfs_dentry_t;
typedef struct vfs_mount vfs_mount_t;
typedef struct file file_t;
typedef struct file_operations file_operations_t;
typedef struct inode_operations inode_operations_t;
typedef struct super_operations super_operations_t;
typedef struct dentry_operations dentry_operations_t;

/* ============================================================================
 * File Types
 * ============================================================================ */

/**
 * @brief VFS file types (matches POSIX mode bits)
 */
typedef enum vtype {
    V_UNKNOWN = 0,
    V_FIFO    = 0010000,  /* Named pipe (FIFO) */
    V_CHR     = 0020000,  /* Character device */
    V_DIR     = 0040000,  /* Directory */
    V_BLK     = 0060000,  /* Block device */
    V_REG     = 0100000,  /* Regular file */
    V_LNK     = 0120000,  /* Symbolic link */
    V_SOCK    = 0140000,  /* Socket */
} vtype_t;

/* File type mask */
#define V_S_IFMT  0170000

/* ============================================================================
 * Open Flags (from fcntl.h)
 * ============================================================================ */

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_NOCTTY    0x0100
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_DIRECTORY 0x100000

/* File mode flags */
#define FMODE_READ  (1 << 0)
#define FMODE_WRITE (1 << 1)

/* ============================================================================
 * Superblock Operations
 * ============================================================================ */

/**
 * @brief Filesystem statistics
 */
struct statfs {
    uint64_t f_type;               /* Filesystem type */
    uint64_t f_bsize;              /* Block size */
    uint64_t f_blocks;             /* Total blocks */
    uint64_t f_bfree;              /* Free blocks */
    uint64_t f_bavail;             /* Available blocks */
    uint64_t f_files;              /* Total inodes */
    uint64_t f_ffree;              /* Free inodes */
    uint64_t f_namelen;            /* Maximum filename length */
};

/**
 * @brief Superblock operations
 */
struct super_operations {
    int (*read_inode)(vfs_superblock_t* sb, vfs_inode_t* inode);
    int (*write_inode)(vfs_superblock_t* sb, vfs_inode_t* inode);
    int (*statfs)(vfs_superblock_t* sb, struct statfs* buf);
    int (*alloc_inode)(vfs_superblock_t* sb, vfs_inode_t* inode);
    void (*destroy_inode)(vfs_inode_t* inode);
    void (*put_super)(vfs_superblock_t* sb);
};

/* ============================================================================
 * Inode Operations
 * ============================================================================ */

/**
 * @brief Inode operations
 */
struct inode_operations {
    int (*lookup)(vfs_inode_t* dir, const char* name, vfs_inode_t** result);
    int (*create)(vfs_inode_t* dir, const char* name, int mode, vfs_inode_t** result);
    int (*mkdir)(vfs_inode_t* dir, const char* name, int mode);
    int (*mknod)(vfs_inode_t* dir, const char* name, int mode, uint32_t dev);
    int (*unlink)(vfs_inode_t* dir, const char* name);
    int (*rmdir)(vfs_inode_t* dir, const char* name);
    int (*rename)(vfs_inode_t* old_dir, const char* old_name,
                  vfs_inode_t* new_dir, const char* new_name);
};

/* ============================================================================
 * File Operations
 * ============================================================================ */

/**
 * @brief Directory entry type for readdir
 */
struct linux_dirent {
    uint64_t d_ino;                /* Inode number */
    int64_t  d_off;                /* Offset to next entry */
    uint16_t d_reclen;             /* Length of this record */
    char     d_name[];             /* Filename (null-terminated) */
};

/**
 * @brief Callback type for readdir
 */
typedef int (*filldir_t)(void* buffer, const char* name, int namlen,
                         uint64_t offset, uint64_t ino, unsigned int d_type);

/**
 * @brief File operations
 */
struct file_operations {
    uint64_t (*llseek)(file_t* file, uint64_t offset, int whence);
    ssize_t (*read)(file_t* file, char* buffer, size_t count, uint64_t* pos);
    ssize_t (*write)(file_t* file, const char* buffer, size_t count, uint64_t* pos);
    int (*readdir)(file_t* file, void* dirent, filldir_t filldir);
    int (*ioctl)(file_t* file, unsigned int cmd, unsigned long arg);
    int (*open)(vfs_inode_t* inode, file_t* file);
    int (*release)(vfs_inode_t* inode, file_t* file);
};

/* ============================================================================
 * Dentry Operations
 * ============================================================================ */

/**
 * @brief Dentry operations
 */
struct dentry_operations {
    int (*d_revalidate)(vfs_dentry_t* dentry);
    void (*d_release)(vfs_dentry_t* dentry);
    void (*d_iput)(vfs_dentry_t* dentry, vfs_inode_t* inode);
};

/* ============================================================================
 * VFS Superblock Structure
 * ============================================================================ */

/**
 * @brief VFS Superblock
 *
 * Represents a mounted filesystem.
 */
struct vfs_superblock {
    list_head s_list;              /* List of all superblocks */
    list_head s_mounts;            /* List of mount points */

    uint32_t s_dev;                /* Device identifier */
    uint64_t s_blocksize;          /* Block size in bytes */
    uint64_t s_maxbytes;           /* Maximum file size */

    uint32_t s_magic;              /* Filesystem magic number */
    char s_fsname[32];             /* Filesystem name */

    vfs_inode_t* s_root;           /* Root inode */

    uint64_t s_blocks;             /* Total blocks */
    uint64_t s_bfree;              /* Free blocks */
    uint64_t s_files;              /* Total inodes */
    uint64_t s_ffree;              /* Free inodes */

    void* s_fs_info;               /* Private filesystem info */
    const super_operations_t* s_op; /* Superblock operations */

    uint32_t s_flags;              /* Mount flags */
    uint32_t s_count;              /* Reference count */

    vfs_dentry_t* s_mount;         /* Mount point dentry */
    vfs_mount_t* s_mount_instance; /* Mount instance */
};

/* ============================================================================
 * VFS Inode Structure
 * ============================================================================ */

/**
 * @brief Inode state flags
 */
#define I_DIRTY        (1 << 0)
#define I_FREEING      (1 << 1)
#define I_LOCK         (1 << 2)

/**
 * @brief VFS Inode
 *
 * Generic inode structure representing a file/directory.
 */
struct vfs_inode {
    list_head i_hash;              /* Hash list for lookup */
    list_head i_list;              /* List of all inodes */
    list_head i_sb_list;           /* Per-superblock list */

    vfs_superblock_t* i_sb;        /* Containing superblock */

    uint64_t i_ino;                /* Inode number */
    uint32_t i_nlink;              /* Hard link count */

    uint32_t i_mode;               /* File mode (type + permissions) */

    uint64_t i_size;               /* Size in bytes */
    uint64_t i_blocks;             /* Number of 512-byte blocks */

    uint64_t i_atime;              /* Access time */
    uint64_t i_mtime;              /* Modification time */
    uint64_t i_ctime;              /* Change time */

    uint32_t i_count;              /* Reference count */
    uint32_t i_state;              /* State flags */

    const inode_operations_t* i_op;  /* Inode operations */
    const file_operations_t* i_fop;  /* File operations */

    void* i_private;               /* Filesystem private data */
};

/* ============================================================================
 * VFS Dentry Structure
 * ============================================================================ */

/**
 * @brief Dentry state flags
 */
#define DENTRY_FLAG_CACHED    (1 << 0)
#define DENTRY_FLAG_NEGATIVE  (1 << 1)

/**
 * @brief VFS Dentry
 *
 * Directory entry cache for path lookup.
 */
struct vfs_dentry {
    list_head d_hash;              /* Hash list */
    list_head d_lru;               /* LRU list */
    list_head d_subdirs;           /* Children list */

    vfs_dentry_t* d_parent;        /* Parent directory */
    vfs_inode_t* d_inode;          /* Associated inode */

    char d_name[256];              /* Filename */

    uint32_t d_flags;              /* State flags */
    uint32_t d_count;              /* Reference count */

    uint64_t d_time;               /* Revalidation time */

    const dentry_operations_t* d_op; /* Dentry operations */
    vfs_superblock_t* d_sb;        /* Superblock */
};

/* ============================================================================
 * VFS Mount Structure
 * ============================================================================ */

/**
 * @brief Mount point structure
 */
struct vfs_mount {
    list_head m_list;              /* List of all mounts */
    vfs_superblock_t* m_sb;        /* Mounted superblock */
    vfs_dentry_t* m_mountpoint;    /* Where this is mounted */
    vfs_dentry_t* m_root;          /* Root of mounted fs */
    char m_path[256];              /* Mount path */
    uint32_t m_flags;              /* Mount flags */
};

/* ============================================================================
 * File Structure
 * ============================================================================ */

/**
 * @brief VFS File Structure
 *
 * Represents an open file with per-open state.
 */
struct file {
    list_head f_list;              /* List of open files */

    vfs_inode_t* f_inode;          /* Associated inode */
    vfs_dentry_t* f_dentry;        /* Associated dentry */

    uint64_t f_pos;                /* Current file position */
    uint32_t f_flags;              /* Open flags (O_*) */
    uint32_t f_mode;               /* File mode (FMODE_*) */

    uint32_t f_count;              /* Reference count */

    const file_operations_t* f_op; /* File operations */

    void* f_private_data;          /* Private data */

    vfs_mount_t* f_vfsmnt;         /* VFS mount */
};

/* ============================================================================
 * VFS Public API
 * ============================================================================ */

/**
 * @brief Initialize VFS subsystem
 * @return 0 on success, negative error code on failure
 */
int vfs_init(void);

/**
 * @brief Initialize inode cache
 */
void vfs_inode_cache_init(void);

/**
 * @brief Initialize dentry cache
 */
void vfs_dentry_cache_init(void);

/**
 * @brief Initialize file cache
 */
void vfs_file_cache_init(void);

/**
 * @brief Initialize mount table
 */
void vfs_mount_table_init(void);

/**
 * @brief Get root superblock
 * @return Root superblock, or NULL if not mounted
 */
vfs_superblock_t* vfs_get_root_sb(void);

/**
 * @brief Set root superblock
 * @param sb Superblock to set as root
 */
void vfs_set_root_sb(vfs_superblock_t* sb);

/**
 * @brief Allocate a new superblock
 * @return New superblock, or NULL on failure
 */
vfs_superblock_t* vfs_alloc_super(void);

/**
 * @brief Free a superblock
 * @param sb Superblock to free
 */
void vfs_free_super(vfs_superblock_t* sb);

/**
 * @brief Add superblock to global list
 * @param sb Superblock to add
 */
void vfs_add_super(vfs_superblock_t* sb);

/**
 * @brief Remove superblock from global list
 * @param sb Superblock to remove
 */
void vfs_remove_super(vfs_superblock_t* sb);

/**
 * @brief Find superblock by device
 * @param dev Device number
 * @return Superblock, or NULL if not found
 */
vfs_superblock_t* vfs_find_super(uint32_t dev);

/**
 * @brief Get inode by number
 * @param sb Superblock
 * @param ino Inode number
 * @return Inode, or NULL if not found
 */
vfs_inode_t* vfs_iget(vfs_superblock_t* sb, uint64_t ino);

/**
 * @brief Release inode reference
 * @param inode Inode to release
 */
void vfs_iput(vfs_inode_t* inode);

/**
 * @brief Mark inode as dirty (needs writeback)
 * @param inode Inode to mark
 */
void vfs_mark_inode_dirty(vfs_inode_t* inode);

/**
 * @brief Check if inode is a directory
 * @param inode Inode to check
 * @return true if directory, false otherwise
 */
bool vfs_is_dir(vfs_inode_t* inode);

/**
 * @brief Check if inode is a regular file
 * @param inode Inode to check
 * @return true if regular file, false otherwise
 */
bool vfs_is_reg(vfs_inode_t* inode);

/**
 * @brief Check if inode is a symbolic link
 * @param inode Inode to check
 * @return true if symlink, false otherwise
 */
bool vfs_is_symlink(vfs_inode_t* inode);

/**
 * @brief Get file type from inode mode
 * @param mode File mode
 * @return File type
 */
vtype_t vfs_mode_to_type(uint32_t mode);

/**
 * @brief Get dentry reference
 * @param dentry Dentry to get
 * @return Dentry with incremented reference
 */
vfs_dentry_t* vfs_dget(vfs_dentry_t* dentry);

/**
 * @brief Put dentry reference
 * @param dentry Dentry to put
 */
void vfs_dput(vfs_dentry_t* dentry);

/**
 * @brief Open inode into file object
 * @param inode Inode to open
 * @param flags Open flags
 * @param file_out Output file pointer
 * @return 0 on success, negative error code on failure
 */
int vfs_open_inode(vfs_inode_t* inode, int flags, file_t** file_out);

/**
 * @brief Open a file
 * @param path File path
 * @param flags Open flags (O_*)
 * @param mode File mode (for creation)
 * @param file_out Output file pointer
 * @return File descriptor number on success, negative error code on failure
 */
int vfs_open(const char* path, int flags, uint32_t mode, file_t** file_out);

/**
 * @brief Close a file
 * @param file File to close
 * @return 0 on success, negative error code on failure
 */
int vfs_close(file_t* file);

/**
 * @brief Read from file
 * @param file File to read from
 * @param buffer Buffer to store data
 * @param count Number of bytes to read
 * @return Number of bytes read, negative error code on failure
 */
ssize_t vfs_read(file_t* file, char* buffer, size_t count);

/**
 * @brief Write to file
 * @param file File to write to
 * @param buffer Data to write
 * @param count Number of bytes to write
 * @return Number of bytes written, negative error code on failure
 */
ssize_t vfs_write(file_t* file, const char* buffer, size_t count);

/**
 * @brief Seek in file
 * @param file File to seek in
 * @param offset Offset
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
 * @return New file position, negative error code on failure
 */
int64_t vfs_lseek(file_t* file, int64_t offset, int whence);

/* ============================================================================
 * Mount Operations
 * ============================================================================ */

/**
 * @brief Filesystem mount function type
 * @param sb Superblock to populate
 * @param data Mount options/data
 * @return 0 on success, negative error code on failure
 */
typedef int (*fs_mount_fn)(vfs_superblock_t* sb, const char* data);

/**
 * @brief Register a filesystem type
 * @param name Filesystem type name (e.g., "ext2")
 * @param mount_fn Mount function for this filesystem type
 * @return 0 on success, negative error code on failure
 */
int vfs_register_fs_type(const char* name, fs_mount_fn mount_fn);

/**
 * @brief Mount a filesystem
 * @param device Device number (e.g., 0 for hda)
 * @param path Mount point path
 * @param fstype Filesystem type (e.g., "ext2")
 * @return 0 on success, negative error code on failure
 */
int vfs_mount(const char* device, const char* path, const char* fstype);

/**
 * @brief Unmount a filesystem
 * @param path Mount point path
 * @return 0 on success, negative error code on failure
 */
int vfs_umount(const char* path);

/* ============================================================================
 * Seek Constants
 * ============================================================================ */

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2
