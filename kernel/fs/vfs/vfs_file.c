/**
 * @file vfs_file.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VFS File Object Management
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 */

#include "vfs.h"
#include "klogs/kprintf.h"
#include "mm/heap/heap.h"
#include "base/memory.h"

/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief File object cache
 */
static struct {
    list_head files;               /* List of all file objects */
    uint32_t count;                /* Number of file objects */
} file_cache;

/* ============================================================================
 * File Object Management
 * ============================================================================ */

/**
 * @brief Allocate a new file object
 */
file_t* vfs_alloc_file(void) {
    file_t* file = (file_t*)kmalloc(sizeof(file_t));
    if (!file) {
        klog_error("vfs: Failed to allocate file object\n");
        return NULL;
    }

    /* Initialize file object */
    memset(file, 0, sizeof(file_t));
    INIT_LIST_HEAD(&file->f_list);

    file->f_inode = NULL;
    file->f_dentry = NULL;
    file->f_pos = 0;
    file->f_flags = 0;
    file->f_mode = 0;
    file->f_count = 1;
    file->f_op = NULL;
    file->f_private_data = NULL;
    file->f_vfsmnt = NULL;

    /* Add to global list */
    list_add(&file->f_list, &file_cache.files);
    file_cache.count++;

    return file;
}

/**
 * @brief Free a file object
 */
void vfs_free_file(file_t* file) {
    if (!file) {
        return;
    }

    /* Remove from global list */
    list_del(&file->f_list);
    file_cache.count--;

    /* Release inode reference */
    if (file->f_inode) {
        vfs_iput(file->f_inode);
    }

    /* Free private data */
    if (file->f_private_data) {
        kfree(file->f_private_data);
    }

    kfree(file);
}

/**
 * @brief Get file object (increment reference)
 */
file_t* vfs_file_get(file_t* file) {
    if (file) {
        file->f_count++;
    }
    return file;
}

/**
 * @brief Put file object (decrement reference)
 */
void vfs_file_put(file_t* file) {
    if (!file) {
        return;
    }

    if (file->f_count > 0) {
        file->f_count--;
    }

    /* Free if no more references */
    if (file->f_count == 0) {
        vfs_free_file(file);
    }
}

/* ============================================================================
 * File Operations
 * ============================================================================ */

/**
 * @brief Read from file
 */
ssize_t vfs_read(file_t* file, char* buffer, size_t count) {
    if (!file || !buffer) {
        return -1;  /* EINVAL */
    }

    if (!file->f_inode) {
        return -1;  /* EBADF */
    }

    /* Check if file is opened for reading */
    if (!(file->f_mode & FMODE_READ)) {
        return -1;  /* EBADF */
    }

    /* Call file-specific read operation */
    if (file->f_op && file->f_op->read) {
        ssize_t result = file->f_op->read(file, buffer, count, &file->f_pos);
        if (result > 0) {
            /* Update access time */
            file->f_inode->i_atime = 0;  /* TODO: Get actual time */
        }
        return result;
    }

    return -1;  /* ENOSYS */
}

/**
 * @brief Write to file
 */
ssize_t vfs_write(file_t* file, const char* buffer, size_t count) {
    if (!file || !buffer) {
        return -1;  /* EINVAL */
    }

    if (!file->f_inode) {
        return -1;  /* EBADF */
    }

    /* Check if file is opened for writing */
    if (!(file->f_mode & FMODE_WRITE)) {
        return -1;  /* EBADF */
    }

    /* Call file-specific write operation */
    if (file->f_op && file->f_op->write) {
        ssize_t result = file->f_op->write(file, buffer, count, &file->f_pos);
        if (result > 0) {
            /* Update modification time and mark dirty */
            file->f_inode->i_mtime = 0;  /* TODO: Get actual time */
            vfs_mark_inode_dirty(file->f_inode);
        }
        return result;
    }

    return -1;  /* ENOSYS */
}

/**
 * @brief Seek in file
 */
int64_t vfs_lseek(file_t* file, int64_t offset, int whence) {
    if (!file) {
        return -1;  /* EBADF */
    }

    if (!file->f_inode) {
        return -1;  /* EBADF */
    }

    uint64_t new_pos;

    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = file->f_pos + offset;
            break;
        case SEEK_END:
            new_pos = file->f_inode->i_size + offset;
            break;
        default:
            return -1;  /* EINVAL */
    }

    /* Check for overflow */
    if (offset < 0 && new_pos > file->f_pos) {
        return -1;  /* EOVERFLOW */
    }

    file->f_pos = new_pos;

    return (int64_t)new_pos;
}

/**
 * @brief Close a file
 */
int vfs_close(file_t* file) {
    if (!file) {
        return -1;  /* EBADF */
    }

    /* Call file-specific release operation */
    if (file->f_op && file->f_op->release) {
        file->f_op->release(file->f_inode, file);
    }

    /* Put file reference (may free it) */
    vfs_file_put(file);

    return 0;
}

/**
 * @brief Open a file (internal)
 */
int vfs_open_inode(vfs_inode_t* inode, int flags, file_t** file_out) {
    if (!inode || !file_out) {
        return -1;
    }

    /* Allocate file object */
    file_t* file = vfs_alloc_file();
    if (!file) {
        return -1;  /* ENOMEM */
    }

    file->f_inode = inode;
    file->f_flags = flags;
    file->f_pos = 0;

    /* Set file mode based on flags */
    switch (flags & O_RDWR) {
        case O_RDONLY:
            file->f_mode = FMODE_READ;
            break;
        case O_WRONLY:
            file->f_mode = FMODE_WRITE;
            break;
        case O_RDWR:
            file->f_mode = FMODE_READ | FMODE_WRITE;
            break;
    }

    /* Set file operations */
    file->f_op = inode->i_fop;

    /* Call file-specific open operation */
    if (file->f_op && file->f_op->open) {
        int result = file->f_op->open(inode, file);
        if (result != 0) {
            vfs_file_put(file);
            return result;
        }
    }

    /* Increment inode reference */
    inode->i_count++;

    *file_out = file;

    return 0;
}

/* ============================================================================
 * File Cache Initialization
 * ============================================================================ */

/**
 * @brief Initialize file cache
 */
void vfs_file_cache_init(void) {
    INIT_LIST_HEAD(&file_cache.files);
    file_cache.count = 0;
}
