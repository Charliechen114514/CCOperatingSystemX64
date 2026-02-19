/**
 * @file vfs_open.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VFS Open/Create Operations
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 */

#include "vfs.h"
#include "klogs/kprintf.h"
#include "base/string.h"
#include "mm/heap/heap.h"

/* Forward declarations */
int vfs_path_lookup(const char* path, vfs_inode_t** inode_out, vfs_dentry_t** dentry_out);

/* ============================================================================
 * Open Operations
 * ============================================================================ */

/**
 * @brief Open a file
 */
int vfs_open(const char* path, int flags, uint32_t mode, file_t** file_out) {
    (void)mode;  /* Not used yet */
    if (!path || !file_out) {
        return -1;  /* EINVAL */
    }

    /* Look up path */
    vfs_inode_t* inode = NULL;
    vfs_dentry_t* dentry = NULL;
    int result = vfs_path_lookup(path, &inode, &dentry);

    if (result != 0) {
        /* File not found */
        if (flags & O_CREAT) {
            /* TODO: Create file */
            return -1;  /* ENOSYS */
        }
        return -1;  /* ENOENT */
    }

    if (!inode) {
        vfs_dput(dentry);
        return -1;  /* ENOENT */
    }

    /* Check if file exists and O_EXCL is set */
    if ((flags & O_EXCL) && (flags & O_CREAT)) {
        vfs_iput(inode);
        vfs_dput(dentry);
        return -1;  /* EEXIST */
    }

    /* Truncate file if O_TRUNC is set */
    if ((flags & O_TRUNC) && (flags & (O_WRONLY | O_RDWR))) {
        /* TODO: Implement truncate */
    }

    /* Open file object */
    result = vfs_open_inode(inode, flags, file_out);
    if (result != 0) {
        vfs_iput(inode);
        vfs_dput(dentry);
        return result;
    }

    /* Associate dentry with file */
    (*file_out)->f_dentry = dentry;

    /* Keep dentry reference */
    /* (inode reference already incremented in vfs_open_inode) */

    return 0;  /* Success */
}

/**
 * @brief Create a new file
 */
int vfs_create(const char* path, uint32_t mode, file_t** file_out) {
    return vfs_open(path, O_CREAT | O_WRONLY | O_TRUNC, mode, file_out);
}
