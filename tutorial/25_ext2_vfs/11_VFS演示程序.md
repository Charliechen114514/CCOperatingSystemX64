# VFS 演示程序 —— ls、cat、df 实战

经过这么多努力，我们的文件系统终于可以用了！现在让我们实现一些演示程序，验证文件系统的功能。

这些演示程序虽然简单，但它们展示了文件系统的核心功能：列出目录、读取文件、查看文件系统信息。通过这些程序，我们可以直观地看到文件系统是否正常工作。

## ls 命令 —— 列出目录内容

ls 命令是最常用的文件系统命令之一。它的实现很简单：打开目录，遍历目录项，打印每个文件的信息：

```c
void vfs_demo_ls(const char* path) {
    file_t* dir;
    int ret = vfs_open(path, O_RDONLY, 0, &dir);
    if (ret != 0) {
        klog_error("demo: Failed to open directory '%s'\n", path);
        return;
    }

    klog_info("Directory listing for %s:\n", path);

    /* 遍历目录 */
    char name[256];
    while (vfs_readdir(dir, name) == 0) {
        /* 获取文件信息 */
        file_t* file;
        if (vfs_open(name, O_RDONLY, 0, &file) == 0) {
            vfs_inode_t* inode = file->f_inode;

            /* 打印文件类型 */
            if (vfs_is_dir(inode)) {
                klog_info("  [DIR]  %s\n", name);
            } else if (vfs_is_reg(inode)) {
                klog_info("  %8lu  %s\n", inode->i_size, name);
            } else {
                klog_info("  [???]  %s\n", name);
            }

            vfs_close(file);
        }
    }

    vfs_close(dir);
}
```

这个函数打开目录，然后遍历所有目录项。对于每个文件，它打开文件获取 inode，然后根据文件类型打印不同的信息。目录显示 `[DIR]`，普通文件显示文件大小。

## cat 命令 —— 显示文件内容

cat 命令用于显示文件内容。它的实现也很直接：打开文件，读取数据，打印到控制台：

```c
void vfs_demo_cat(const char* path) {
    file_t* file;
    int ret = vfs_open(path, O_RDONLY, 0, &file);
    if (ret != 0) {
        klog_error("demo: Failed to open file '%s'\n", path);
        return;
    }

    klog_info("Content of %s:\n", path);
    klog_info("----------------------------------------\n");

    char buffer[256];
    ssize_t bytes;
    while ((bytes = vfs_read(file, buffer, sizeof(buffer))) > 0) {
        /* 打印到控制台 */
        for (ssize_t i = 0; i < bytes; i++) {
            klog_putc(buffer[i]);
        }
    }

    klog_info("\n----------------------------------------\n");
    klog_info("Total bytes read: %lu\n", file->f_pos);

    vfs_close(file);
}
```

这个函数使用循环读取文件，每次读取最多 256 字节。读取的数据逐字符打印到控制台。这种分批读取的方式可以处理任意大小的文件，而不会占用太多内存。

## df 命令 —— 显示文件系统信息

df 命令显示文件系统的统计信息：

```c
void vfs_demo_df(const char* path) {
    /* 首先获取文件系统信息 */
    vfs_superblock_t* sb = vfs_get_root_sb();
    if (!sb) {
        klog_error("demo: No filesystem mounted\n");
        return;
    }

    struct statfs buf;
    if (sb->s_op && sb->s_op->statfs) {
        sb->s_op->statfs(sb, &buf);

        klog_info("Filesystem information:\n");
        klog_info("  Type:           0x%x\n", buf.f_type);
        klog_info("  Block size:     %lu bytes\n", buf.f_bsize);
        klog_info("  Total blocks:   %lu\n", buf.f_blocks);
        klog_info("  Free blocks:    %lu\n", buf.f_bfree);
        klog_info("  Total inodes:   %lu\n", buf.f_files);
        klog_info("  Free inodes:    %lu\n", buf.f_ffree);
        klog_info("  Max name len:   %lu\n", buf.f_namelen);

        /* 计算使用百分比 */
        if (buf.f_blocks > 0) {
            uint64_t used = buf.f_blocks - buf.f_bfree;
            uint32_t percent = (used * 100) / buf.f_blocks;
            klog_info("  Used:           %u%%\n", percent);
        }
    }
}
```

这个函数调用文件系统的 `statfs` 操作获取统计信息，然后格式化输出。信息包括块大小、总块数、空闲块数、inode 统计等。

## 演示主程序

在内核初始化时调用这些演示程序：

```c
void vfs_demo_run_all(void) {
    klog_info("\n");
    klog_info("========================================\n");
    klog_info("VFS Filesystem Demo\n");
    klog_info("========================================\n");

    /* 显示文件系统信息 */
    klog_info("\n--- df ---\n");
    vfs_demo_df("/");

    /* 列出根目录 */
    klog_info("\n--- ls / ---\n");
    vfs_demo_ls("/");

    /* 读取某个文件 */
    klog_info("\n--- cat /test.txt ---\n");
    vfs_demo_cat("/test.txt");

    klog_info("\n========================================\n");
    klog_info("Demo completed\n");
    klog_info("========================================\n");
}
```

## 在内核初始化时调用

在内核主函数中，文件系统初始化后调用演示程序：

```c
void kernel_main(void) {
    /* ... 其他初始化 ... */

    /* 初始化块设备 */
    block_init();

    /* 初始化 VFS */
    vfs_init();

    /* 初始化 EXT2 */
    ext2_init();

    /* 挂载根文件系统 */
    vfs_mount("hda", "/", "ext2");

    /* 运行演示程序 */
    vfs_demo_run_all();

    /* ... 其他代码 ... */
}
```

## 测试输出示例

假设我们在磁盘上创建了一个简单的 EXT2 文件系统，包含几个文件。演示程序运行后可能输出：

```
========================================
VFS Filesystem Demo
========================================

--- df ---
Filesystem information:
  Type:           0xEF53
  Block size:     4096 bytes
  Total blocks:   10240
  Free blocks:    8192
  Total inodes:   2560
  Free inodes:    2540
  Max name len:   255
  Used:           20%

--- ls / ---
Directory listing for /:
  [DIR]  .
  [DIR]  ..
     1024  test.txt
  [DIR]  bin
  [DIR]  etc

--- cat /test.txt ---
Content of /test.txt:
----------------------------------------
Hello, CCOS!
This is a test file.
========================================
Demo completed
========================================
```

## 创建测试文件系统

为了测试文件系统，我们需要在主机上创建一个 EXT2 镜像：

```bash
# 创建一个 64MB 的镜像文件
dd if=/dev/zero of=disk.img bs=1M count=64

# 格式化为 EXT2
mkfs.ext2 disk.img

# 挂载到本地目录
sudo mount -o loop disk.img /mnt

# 创建测试文件
echo "Hello, CCOS!" > /mnt/test.txt
mkdir /mnt/bin
mkdir /mnt/etc

# 卸载
sudo umount /mnt
```

然后在虚拟机或真实硬件上测试，确保磁盘镜像的路径正确。

## 常见问题排查

如果演示程序不能正常工作，首先检查串口输出中的错误信息。常见问题包括：

1. **磁盘读取失败**：检查设备号是否正确，块设备是否正常工作
2. **魔数错误**：磁盘镜像可能不是 EXT2 格式，或者读取位置错误
3. **文件未找到**：检查路径是否正确，文件是否真的存在于镜像中

可以在关键位置添加调试输出，比如：

```c
klog_trace("ext2: Reading block %u\n", block_num);
klog_trace("vfs: Looking up '%s'\n", name);
```

## 小结

演示程序验证了文件系统的核心功能。通过 ls、cat、df 命令，我们可以直观地看到文件系统的工作状态。这些程序虽然简单，但它们展示了完整的文件系统调用链：从用户命令到 VFS 接口，到具体文件系统实现，到块设备 I/O，最后到硬件驱动。

到这里，我们已经实现了一个完整的文件系统子系统！虽然还有很多功能可以添加（写文件、创建文件、删除文件等），但核心架构已经建立，后续扩展会相对容易。

下一篇文章将总结整个阶段，讨论测试方法和未来改进方向。

---

<div align="center">

## 文档导航

[← EXT2 文件读写](10_EXT2文件读写.md)  | [完整测试与故障排查 →](12_完整测试与故障排查.md)

</div>
