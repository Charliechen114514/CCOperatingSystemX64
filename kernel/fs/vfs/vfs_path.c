/**
 * @file vfs_path.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VFS Path Resolution
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 */

#include "vfs.h"
#include "klogs/kprintf.h"
#include "base/string.h"
#include "mm/heap/heap.h"

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Skip leading slashes
 */
static const char* skip_slash(const char* path) {
    while (*path == '/') {
        path++;
    }
    return path;
}

/**
 * @brief Find next path component
 */
static char* next_component(const char* path) {
    static char component[256];
    size_t i = 0;

    while (*path && *path != '/' && i < sizeof(component) - 1) {
        component[i++] = *path++;
    }
    component[i] = '\0';

    return component;
}

/* ============================================================================
 * Path Resolution
 * ============================================================================ */

/**
 * @brief Look up a path and return the inode
 *
 * @param path Path to resolve
 * @param inode_out Output inode pointer
 * @param dentry_out Output dentry pointer (can be NULL)
 * @return 0 on success, negative error code on failure
 */
int vfs_path_lookup(const char* path, vfs_inode_t** inode_out, vfs_dentry_t** dentry_out) {
    if (!path || !inode_out) {
        return -1;
    }

    /* Get root filesystem */
    vfs_superblock_t* sb = vfs_get_root_sb();
    if (!sb) {
        klog_error("vfs: No root filesystem mounted\n");
        return -1;
    }

    if (!sb->s_root) {
        klog_error("vfs: Root inode not set\n");
        return -1;
    }

    /* Start from root */
    vfs_inode_t* current = sb->s_root;
    vfs_dentry_t* dentry = NULL;  /* TODO: Track dentry */

    current->i_count++;

    /* Handle absolute path */
    if (*path == '/') {
        path = skip_slash(path);
    }

    /* Traverse path components */
    while (*path) {
        /* Get next component */
        char* component = next_component(path);

        /* Skip empty component (e.g., "//") */
        if (component[0] == '\0') {
            path = skip_slash(path);
            continue;
        }

        /* Handle "." */
        if (strcmp(component, ".") == 0) {
            path += strlen(component);
            path = skip_slash(path);
            continue;
        }

        /* Handle ".." */
        if (strcmp(component, "..") == 0) {
            /* TODO: Implement parent directory navigation */
            path += strlen(component);
            path = skip_slash(path);
            continue;
        }

        /* Check if current is a directory */
        if (!vfs_is_dir(current)) {
            vfs_iput(current);
            return -1;  /* ENOTDIR */
        }

        /* Look up component in directory */
        if (!current->i_op || !current->i_op->lookup) {
            vfs_iput(current);
            return -1;  /* ENOSYS */
        }

        vfs_inode_t* next = NULL;
        int result = current->i_op->lookup(current, component, &next);
        if (result != 0 || !next) {
            vfs_iput(current);
            return -1;  /* ENOENT */
        }

        /* Move to next inode */
        vfs_iput(current);
        current = next;

        /* Advance path */
        path += strlen(component);
        path = skip_slash(path);
    }

    *inode_out = current;

    if (dentry_out) {
        *dentry_out = dentry;  /* TODO: Return actual dentry */
    }

    return 0;
}
