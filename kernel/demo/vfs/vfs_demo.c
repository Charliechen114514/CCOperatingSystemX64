/**
 * @file vfs_demo.c
 * @brief VFS/EXT2 Filesystem Demo - Demonstrates file and directory operations
 */

#include "vfs_demo.h"
#include "fs/vfs/vfs.h"
#include "klogs/kprintf.h"
#include "klogs/ksnprintf.h"
#include "mm/heap/heap.h"
#include "base/string.h"
#include "base/memory.h"

/* ============================================================================
 * Demo Constants
 * ============================================================================ */

#define DEMO_READ_BUFFER_SIZE 4096
#define DEMO_MAX_PATH_LEN 256
#define DEMO_MAX_ENTRIES 32

/* ============================================================================
 * Forward declarations for internal functions
 * ============================================================================ */

extern int vfs_path_lookup(const char* path, vfs_inode_t** inode_out, vfs_dentry_t** dentry_out);

/* ============================================================================
 * Context for directory traversal
 * ============================================================================ */

typedef struct {
    char name[256];
    unsigned int type;
} demo_entry_t;

typedef struct {
    demo_entry_t entries[DEMO_MAX_ENTRIES];
    int entry_count;
    int max_entries;
} demo_dir_context_t;

/* ============================================================================
 * Demo Helper Functions
 * ============================================================================ */

/**
 * @brief Callback for directory entry filling
 */
static int demo_filldir_callback(void* buffer, const char* name, int namlen,
                                  uint64_t offset, uint64_t ino, unsigned int d_type) {
    (void)buffer;
    (void)namlen;
    (void)offset;
    (void)ino;

    /* Skip "." and ".." entries */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 0;
    }

    /* Print entry name */
    klog_trace("  - %s", name);

    /* Print file type */
    switch (d_type) {
        case V_REG:
            klog_trace(" (file)\n");
            break;
        case V_DIR:
            klog_trace(" (dir)\n");
            break;
        case V_LNK:
            klog_trace(" (symlink)\n");
            break;
        default:
            klog_trace(" (unknown)\n");
            break;
    }

    return 0;
}

/**
 * @brief Callback for collecting directory entries
 */
static int demo_collect_entries(void* buffer, const char* name, int namlen,
                                 uint64_t offset, uint64_t ino, unsigned int d_type) {
    (void)namlen;
    (void)offset;
    (void)ino;

    demo_dir_context_t* ctx = (demo_dir_context_t*)buffer;

    if (ctx->entry_count >= ctx->max_entries) {
        return 0;
    }

    /* Skip "." and ".." entries */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 0;
    }

    strncpy(ctx->entries[ctx->entry_count].name, name, 255);
    ctx->entries[ctx->entry_count].name[255] = '\0';
    ctx->entries[ctx->entry_count].type = d_type;
    ctx->entry_count++;

    return 0;
}

/**
 * @brief Get file type string from mode
 */
static const char* demo_file_type_str(uint32_t mode) {
    switch (mode & V_S_IFMT) {
        case V_REG: return "Regular File";
        case V_DIR: return "Directory";
        case V_LNK: return "Symbolic Link";
        case V_CHR: return "Character Device";
        case V_BLK: return "Block Device";
        case V_FIFO: return "FIFO";
        case V_SOCK: return "Socket";
        default: return "Unknown";
    }
}

/* ============================================================================
 * Demo Functions
 * ============================================================================ */

/**
 * @brief Demo 1: Mount EXT2 filesystem
 */
static void demo_mount_ext2(void) {
    klog_trace("\n=== Demo 1: Mounting EXT2 Filesystem ===\n");

    /* Check if root filesystem is already mounted */
    vfs_superblock_t* sb = vfs_get_root_sb();
    if (sb) {
        klog_info("Root filesystem already mounted: %s\n", sb->s_fsname);
        klog_trace("  Block size: %lu bytes\n", sb->s_blocksize);
        klog_trace("  Total blocks: %lu\n", sb->s_blocks);
        klog_trace("  Free blocks: %lu\n", sb->s_bfree);
        klog_trace("  Total inodes: %lu\n", sb->s_files);
        klog_trace("  Free inodes: %lu\n", sb->s_ffree);
        return;
    }

    /* Mount EXT2 filesystem on hdb (second data disk) */
    klog_trace("Attempting to mount EXT2 filesystem on /dev/hdb...\n");

    int result = vfs_mount("hdb", "/", "ext2");
    if (result != 0) {
        klog_error("Failed to mount EXT2 filesystem\n");
        klog_warn("The data disk may not have an EXT2 filesystem\n");
        klog_info("To format the disk with EXT2, run on host:\n");
        klog_info("  mkfs.ext2 disk/disk.img\n");
        return;
    }

    klog_info("Successfully mounted EXT2 filesystem on /\n");

    /* Get filesystem info */
    sb = vfs_get_root_sb();
    if (sb) {
        klog_trace("Filesystem info:\n");
        klog_trace("  Type: %s\n", sb->s_fsname);
        klog_trace("  Block size: %lu bytes\n", sb->s_blocksize);
        klog_trace("  Total blocks: %lu\n", sb->s_blocks);
        klog_trace("  Free blocks: %lu\n", sb->s_bfree);
        klog_trace("  Total inodes: %lu\n", sb->s_files);
        klog_trace("  Free inodes: %lu\n", sb->s_ffree);
    }
}

/**
 * @brief Demo 2: List root directory contents
 */
static void demo_list_root_directory(void) {
    klog_trace("\n=== Demo 2: Listing Root Directory ===\n");

    /* Open root directory */
    file_t* dir_file = NULL;
    int result = vfs_open("/", O_RDONLY, 0, &dir_file);
    if (result != 0) {
        klog_error("Failed to open root directory: %d\n", result);
        return;
    }

    if (!dir_file || !dir_file->f_inode) {
        klog_error("Invalid file handle\n");
        return;
    }

    /* Check if it's a directory */
    if (!vfs_is_dir(dir_file->f_inode)) {
        klog_error("Root is not a directory!\n");
        vfs_close(dir_file);
        return;
    }

    klog_info("Root directory contents:\n");

    /* Read directory entries */
    if (dir_file->f_op && dir_file->f_op->readdir) {
        result = dir_file->f_op->readdir(dir_file, NULL, demo_filldir_callback);
        if (result != 0) {
            klog_error("Failed to read directory: %d\n", result);
        }
    } else {
        klog_warn("readdir operation not available\n");
    }

    vfs_close(dir_file);
    klog_info("Directory listing complete\n");
}

/**
 * @brief Demo 3: Read file contents
 */
static void demo_read_file(const char* path) {
    klog_trace("\n=== Demo 3: Reading File ===\n");
    klog_trace("File: %s\n", path);

    /* Open file */
    file_t* file = NULL;
    int result = vfs_open(path, O_RDONLY, 0, &file);
    if (result != 0) {
        klog_error("Failed to open file '%s': %d\n", path, result);
        klog_warn("File may not exist. Try listing directory first.\n");
        return;
    }

    if (!file || !file->f_inode) {
        klog_error("Invalid file handle\n");
        return;
    }

    /* Print file info */
    klog_trace("File information:\n");
    klog_trace("  Inode: %lu\n", file->f_inode->i_ino);
    klog_trace("  Size: %lu bytes\n", file->f_inode->i_size);
    klog_trace("  Type: %s\n", demo_file_type_str(file->f_inode->i_mode));
    klog_trace("  Links: %u\n", file->f_inode->i_nlink);

    if (!vfs_is_reg(file->f_inode)) {
        klog_warn("Not a regular file, skipping content read\n");
        vfs_close(file);
        return;
    }

    /* Allocate read buffer */
    char* buffer = (char*)kmalloc(DEMO_READ_BUFFER_SIZE);
    if (!buffer) {
        klog_error("Failed to allocate read buffer\n");
        vfs_close(file);
        return;
    }

    klog_info("File contents:\n");

    /* Read file content */
    uint64_t total_read = 0;
    ssize_t bytes_read;
    int line_count = 0;

    do {
        bytes_read = vfs_read(file, buffer, DEMO_READ_BUFFER_SIZE - 1);

        if (bytes_read < 0) {
            klog_error("Error reading file: %ld\n", (long)bytes_read);
            break;
        }

        if (bytes_read == 0) {
            break;  /* EOF */
        }

        total_read += (uint64_t)bytes_read;

        /* Null-terminate for printing */
        buffer[bytes_read] = '\0';

        /* Print content (limit lines for readability) */
        for (ssize_t i = 0; i < bytes_read && line_count < 20; i++) {
            if (buffer[i] == '\n') {
                klog_trace("\n");
                line_count++;
            } else if (buffer[i] >= 32 && buffer[i] < 127) {
                klog_trace("%c", buffer[i]);
            } else if (buffer[i] == '\t') {
                klog_trace("    ");
            }
        }

    } while (bytes_read > 0 && total_read < file->f_inode->i_size);

    if (line_count >= 20) {
        klog_trace("...\n(output truncated)\n");
    }

    kfree(buffer);
    vfs_close(file);

    klog_info("\nRead complete: %lu bytes\n", total_read);
}

/**
 * @brief Demo 4: List subdirectory
 */
static void demo_list_directory(const char* path) {
    klog_trace("\n=== Demo 4: Listing Directory ===\n");
    klog_trace("Path: %s\n", path);

    /* Look up directory inode */
    vfs_inode_t* inode = NULL;
    int result = vfs_path_lookup(path, &inode, NULL);
    if (result != 0 || !inode) {
        klog_error("Failed to find directory '%s'\n", path);
        return;
    }

    /* Check if it's a directory */
    if (!vfs_is_dir(inode)) {
        klog_warn("'%s' is not a directory\n", path);
        vfs_iput(inode);
        return;
    }

    klog_info("Directory '%s' contents:\n", path);

    /* Open directory */
    file_t* dir_file = NULL;
    result = vfs_open_inode(inode, O_RDONLY, &dir_file);
    if (result != 0) {
        klog_error("Failed to open directory: %d\n", result);
        vfs_iput(inode);
        return;
    }

    /* Read directory entries */
    if (dir_file->f_op && dir_file->f_op->readdir) {
        result = dir_file->f_op->readdir(dir_file, NULL, demo_filldir_callback);
        if (result != 0) {
            klog_error("Failed to read directory: %d\n", result);
        }
    } else {
        klog_warn("readdir operation not available\n");
    }

    vfs_close(dir_file);
    vfs_iput(inode);

    klog_info("Directory listing complete\n");
}

/**
 * @brief Demo 5: Get file statistics
 */
static void demo_file_stats(const char* path) {
    klog_trace("\n=== Demo 5: File Statistics ===\n");
    klog_trace("Path: %s\n", path);

    /* Look up inode */
    vfs_inode_t* inode = NULL;
    int result = vfs_path_lookup(path, &inode, NULL);
    if (result != 0 || !inode) {
        klog_error("Failed to find '%s'\n", path);
        return;
    }

    klog_info("File statistics:\n");
    klog_trace("  Inode number: %lu\n", inode->i_ino);
    klog_trace("  Type: %s\n", demo_file_type_str(inode->i_mode));
    klog_trace("  Size: %lu bytes\n", inode->i_size);
    klog_trace("  Blocks: %lu (512-byte blocks)\n", inode->i_blocks);
    klog_trace("  Links: %u\n", inode->i_nlink);
    klog_trace("  Mode: 0x%08X\n", inode->i_mode);

    /* Calculate size in KB/MB for display */
    if (inode->i_size < 1024) {
        klog_trace("  Size: %lu B\n", inode->i_size);
    } else if (inode->i_size < (uint64_t)1024 * 1024) {
        klog_trace("  Size: %lu KB\n", inode->i_size / 1024);
    } else {
        klog_trace("  Size: %lu MB\n", inode->i_size / ((uint64_t)1024 * 1024));
    }

    vfs_iput(inode);
}

/**
 * @brief Demo 6: File seek test
 */
static void demo_file_seek(const char* path) {
    klog_trace("\n=== Demo 6: File Seek Test ===\n");
    klog_trace("Path: %s\n", path);

    /* Open file */
    file_t* file = NULL;
    int result = vfs_open(path, O_RDONLY, 0, &file);
    if (result != 0) {
        klog_error("Failed to open file '%s'\n", path);
        return;
    }

    if (!file || !vfs_is_reg(file->f_inode)) {
        klog_warn("Not a regular file\n");
        vfs_close(file);
        return;
    }

    uint64_t file_size = file->f_inode->i_size;
    klog_trace("File size: %lu bytes\n", file_size);

    /* Test SEEK_SET */
    int64_t pos = vfs_lseek(file, 0, SEEK_SET);
    klog_trace("SEEK_SET(0): position = %lld\n", pos);

    /* Test SEEK_END */
    pos = vfs_lseek(file, 0, SEEK_END);
    klog_trace("SEEK_END(0): position = %lld\n", pos);

    /* Test SEEK_CUR with negative offset */
    pos = vfs_lseek(file, -100, SEEK_CUR);
    klog_trace("SEEK_CUR(-100): position = %lld\n", pos);

    /* Test SEEK_SET to middle */
    pos = vfs_lseek(file, (int64_t)(file_size / 2), SEEK_SET);
    klog_trace("SEEK_SET(mid): position = %lld\n", pos);

    /* Read 16 bytes from middle */
    char buffer[17];
    ssize_t bytes_read = vfs_read(file, buffer, 16);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        klog_trace("Read at mid: \"%s\"\n", buffer);
    }

    vfs_close(file);
    klog_info("Seek test complete\n");
}

/**
 * @brief Demo 7: Traverse directory tree (internal recursive function)
 */
static void demo_traverse_tree(const char* path, int depth) {
    if (depth > 3) {
        return;  /* Limit depth */
    }

    /* Open directory */
    file_t* dir_file = NULL;
    int result = vfs_open(path, O_RDONLY, 0, &dir_file);
    if (result != 0) {
        return;
    }

    if (!dir_file || !vfs_is_dir(dir_file->f_inode)) {
        vfs_close(dir_file);
        return;
    }

    /* Collect entries */
    demo_dir_context_t ctx;
    ctx.entry_count = 0;
    ctx.max_entries = DEMO_MAX_ENTRIES;
    memset(ctx.entries, 0, sizeof(ctx.entries));

    if (dir_file->f_op && dir_file->f_op->readdir) {
        dir_file->f_op->readdir(dir_file, &ctx, demo_collect_entries);
    }

    vfs_close(dir_file);

    /* Print and recurse */
    for (int i = 0; i < ctx.entry_count; i++) {
        /* Print indentation */
        for (int d = 0; d < depth; d++) {
            klog_trace("  ");
        }

        if (ctx.entries[i].type == V_DIR) {
            klog_trace("[DIR] %s/\n", ctx.entries[i].name);

            /* Build subdirectory path */
            char sub_path[DEMO_MAX_PATH_LEN];
            ksnprintf(sub_path, sizeof(sub_path), "%s%s/", path, ctx.entries[i].name);

            /* Recurse */
            demo_traverse_tree(sub_path, depth + 1);
        } else {
            klog_trace("[FILE] %s\n", ctx.entries[i].name);
        }
    }
}

static void demo_directory_tree(void) {
    klog_trace("\n=== Demo 7: Directory Tree ===\n");

    vfs_superblock_t* sb = vfs_get_root_sb();
    if (!sb || !sb->s_root) {
        klog_error("No root filesystem\n");
        return;
    }

    klog_info("Root directory tree (depth limited to 3):\n");
    demo_traverse_tree("/", 0);

    klog_info("Tree traversal complete\n");
}

/* ============================================================================
 * Main Demo Entry Point
 * ============================================================================ */

/**
 * @brief Run all VFS/EXT2 filesystem demos
 *
 * This function demonstrates the VFS layer with EXT2 filesystem:
 * 1. Mounting EXT2 filesystem
 * 2. Listing root directory contents
 * 3. Reading a file
 * 4. Directory traversal
 * 5. File statistics
 * 6. File seek operations
 * 7. Directory tree display
 *
 * Call this function after filesystem initialization to see the demos.
 */
void vfs_run_demo(void) {
    klog_trace("\n");
    klog_trace("========================================\n");
    klog_trace("   VFS/EXT2 Filesystem Demo\n");
    klog_trace("========================================\n");

    /* Check if filesystem is initialized */
    vfs_superblock_t* sb = vfs_get_root_sb();
    if (!sb) {
        klog_warn("[VFS Demo] No filesystem mounted yet\n");
        klog_info("Will attempt to mount EXT2 filesystem...\n");
    }

    /* Demo 1: Mount filesystem */
    demo_mount_ext2();

    /* Verify filesystem is mounted */
    sb = vfs_get_root_sb();
    if (!sb) {
        klog_error("[VFS Demo] Failed to mount filesystem, demos cannot continue\n");
        return;
    }

    /* Demo 2: List root directory */
    demo_list_root_directory();

    /* Demo 3: Try to read some common files */
    klog_trace("\n--- Attempting to read common files ---\n");

    /* Try common test files */
    const char* test_files[] = {
        "/test.txt",
        "/README",
        "/README.txt",
        "/hello.txt",
        "/boot/grub/grub.cfg",
        "/etc/fstab",
        NULL
    };

    bool file_found = false;
    for (int i = 0; test_files[i] != NULL; i++) {
        vfs_inode_t* inode = NULL;
        if (vfs_path_lookup(test_files[i], &inode, NULL) == 0 && inode) {
            if (vfs_is_reg(inode)) {
                demo_read_file(test_files[i]);
                file_found = true;
                vfs_iput(inode);
                break;  /* Only read one file */
            }
            vfs_iput(inode);
        }
    }

    if (!file_found) {
        klog_info("No readable text files found in common locations\n");
        klog_info("Create a file like /test.txt to see file reading demo\n");
    }

    /* Demo 4: List a subdirectory (try common ones) */
    const char* test_dirs[] = {
        "/etc",
        "/bin",
        "/usr",
        "/home",
        "/boot",
        NULL
    };

    for (int i = 0; test_dirs[i] != NULL; i++) {
        vfs_inode_t* inode = NULL;
        if (vfs_path_lookup(test_dirs[i], &inode, NULL) == 0 && inode) {
            if (vfs_is_dir(inode)) {
                demo_list_directory(test_dirs[i]);
                vfs_iput(inode);
                break;
            }
            vfs_iput(inode);
        }
    }

    /* Demo 5: File stats for root */
    demo_file_stats("/");

    /* Demo 6: Seek test (use the first file we found) */
    if (file_found) {
        for (int i = 0; test_files[i] != NULL; i++) {
            vfs_inode_t* inode = NULL;
            if (vfs_path_lookup(test_files[i], &inode, NULL) == 0 && inode) {
                if (vfs_is_reg(inode)) {
                    demo_file_seek(test_files[i]);
                    vfs_iput(inode);
                    break;
                }
                vfs_iput(inode);
            }
        }
    }

    /* Demo 7: Directory tree */
    demo_directory_tree();

    klog_trace("\n=== VFS Demo Complete ===\n");
    klog_info("Filesystem operations demonstrated successfully\n");
}
