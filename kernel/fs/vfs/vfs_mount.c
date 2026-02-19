/**
 * @file vfs_mount.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VFS Mount Point Management
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 */

#include "vfs.h"
#include "klogs/kprintf.h"
#include "mm/heap/heap.h"
#include "base/string.h"

/* Forward declarations for filesystem type registration */
typedef int (*fs_mount_fn)(vfs_superblock_t* sb, const char* data);

typedef struct fs_type {
    char name[32];
    fs_mount_fn mount_fn;
    list_head list;
} fs_type_t;

/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief Mount table
 */
static struct {
    list_head mounts;              /* List of all mount points */
    uint32_t count;                /* Number of mount points */
    list_head fs_types;            /* Registered filesystem types */
} mount_table;

/* ============================================================================
 * Filesystem Type Registration
 * ============================================================================ */

/**
 * @brief Register a filesystem type
 */
int vfs_register_fs_type(const char* name, fs_mount_fn mount_fn) {
    fs_type_t* fst = (fs_type_t*)kmalloc(sizeof(fs_type_t));
    if (!fst) {
        return -1;
    }

    strncpy(fst->name, name, sizeof(fst->name) - 1);
    fst->mount_fn = mount_fn;
    INIT_LIST_HEAD(&fst->list);

    list_add_tail(&fst->list, &mount_table.fs_types);

    klog_info("vfs: Registered filesystem type '%s'\n", name);

    return 0;
}

/**
 * @brief Find filesystem type by name
 */
static fs_type_t* vfs_find_fs_type(const char* name) {
    fs_type_t* fst;

    list_for_each_entry(fst, &mount_table.fs_types, list) {
        if (strcmp(fst->name, name) == 0) {
            return fst;
        }
    }

    return NULL;
}

/* ============================================================================
 * Mount Operations
 * ============================================================================ */

/**
 * @brief Create a new mount structure
 */
static vfs_mount_t* vfs_alloc_mount(vfs_superblock_t* sb,
                                     vfs_dentry_t* mountpoint,
                                     const char* path) {
    vfs_mount_t* mnt = (vfs_mount_t*)kmalloc(sizeof(vfs_mount_t));
    if (!mnt) {
        return NULL;
    }

    INIT_LIST_HEAD(&mnt->m_list);
    mnt->m_sb = sb;
    mnt->m_mountpoint = mountpoint;
    mnt->m_root = NULL;  /* TODO: Create root dentry from root inode */
    mnt->m_flags = 0;

    if (path) {
        strncpy(mnt->m_path, path, sizeof(mnt->m_path) - 1);
    } else {
        mnt->m_path[0] = '\0';
    }

    return mnt;
}

/**
 * @brief Mount a filesystem
 */
int vfs_mount(const char* device, const char* path, const char* fstype) {
    if (!path || !fstype) {
        return -1;
    }

    /* Find filesystem type */
    fs_type_t* fst = vfs_find_fs_type(fstype);
    if (!fst) {
        klog_error("vfs: Filesystem type '%s' not registered\n", fstype);
        return -1;
    }

    /* Parse device number (e.g., "hda" -> 0) */
    int dev_id = -1;
    if (device) {
        if (strcmp(device, "hda") == 0) dev_id = 0;
        else if (strcmp(device, "hdb") == 0) dev_id = 1;
        else if (strcmp(device, "hdc") == 0) dev_id = 2;
        else if (strcmp(device, "hdd") == 0) dev_id = 3;
        else {
            /* Try to parse as number */
            dev_id = 0;  /* Default to first device */
        }
    }

    /* Allocate superblock */
    vfs_superblock_t* sb = vfs_alloc_super();
    if (!sb) {
        return -1;
    }

    sb->s_dev = dev_id;

    /* Call filesystem-specific mount function */
    int result = fst->mount_fn(sb, NULL);
    if (result != 0) {
        vfs_free_super(sb);
        klog_error("vfs: Failed to mount %s filesystem\n", fstype);
        return -1;
    }

    /* Create mount structure */
    vfs_mount_t* mnt = vfs_alloc_mount(sb, NULL, path);
    if (!mnt) {
        vfs_free_super(sb);
        return -1;
    }

    /* Add to mount table */
    list_add_tail(&mnt->m_list, &mount_table.mounts);
    mount_table.count++;

    /* Add superblock to VFS */
    vfs_add_super(sb);

    /* Set as root if path is "/" */
    if (strcmp(path, "/") == 0) {
        vfs_set_root_sb(sb);
    }

    klog_info("vfs: Mounted %s on %s (device=%s)\n", fstype, path,
              device ? device : "none");

    return 0;
}

/**
 * @brief Unmount a filesystem
 */
int vfs_umount(const char* path) {
    if (!path) {
        return -1;
    }

    vfs_mount_t* mnt, *tmp;
    list_for_each_entry_safe(mnt, tmp, &mount_table.mounts, m_list) {
        if (strcmp(mnt->m_path, path) == 0) {
            /* Check if this is the root filesystem */
            if (mnt->m_sb == vfs_get_root_sb()) {
                klog_warn("vfs: Cannot unmount root filesystem\n");
                return -1;
            }

            /* Remove from mount table */
            list_del(&mnt->m_list);
            mount_table.count--;

            /* Remove superblock */
            vfs_remove_super(mnt->m_sb);

            /* Free superblock */
            vfs_free_super(mnt->m_sb);

            /* Free mount structure */
            kfree(mnt);

            klog_info("vfs: Unmounted %s\n", path);

            return 0;
        }
    }

    klog_error("vfs: Mount point %s not found\n", path);
    return -1;
}

/**
 * @brief Get mount for a given path
 */
vfs_mount_t* vfs_get_mount(const char* path) {
    vfs_mount_t* mnt;
    vfs_mount_t* best_match = NULL;
    size_t best_len = 0;

    list_for_each_entry(mnt, &mount_table.mounts, m_list) {
        size_t len = strlen(mnt->m_path);
        if (len > best_len && strncmp(path, mnt->m_path, len) == 0) {
            best_match = mnt;
            best_len = len;
        }
    }

    return best_match;
}

/* ============================================================================
 * Mount Table Initialization
 * ============================================================================ */

/**
 * @brief Initialize mount table
 */
void vfs_mount_table_init(void) {
    INIT_LIST_HEAD(&mount_table.mounts);
    INIT_LIST_HEAD(&mount_table.fs_types);
    mount_table.count = 0;
}
