# EXT2 inode 操作 —— 文件元数据的奥秘

inode 是 EXT2 中最重要的概念之一，它存储文件或目录的元数据（大小、权限、时间戳等）和数据块指针。理解 inode 的结构和使用，是实现文件读写的第一步。

每个文件都有一个 inode，但 inode 本身不包含文件名。文件名是通过目录项与 inode 关联的，这允许同一个文件有多个名字（硬链接）。

## EXT2 inode 结构

EXT2 inode 在磁盘上的结构如下：

```c
typedef struct PACKED ext2_inode {
    uint16_t i_mode;               /* 文件类型和权限 */
    uint16_t i_uid;                /* 所有者 UID */
    uint32_t i_size;               /* 文件大小（低 32 位） */
    uint32_t i_atime;              /* 访问时间 */
    uint32_t i_ctime;              /* 创建时间 */
    uint32_t i_mtime;              /* 修改时间 */
    uint32_t i_dtime;              /* 删除时间 */
    uint16_t i_gid;                /* 组 GID */
    uint16_t i_links_count;        /* 硬链接数 */
    uint32_t i_blocks;             /* 512 字节块数 */
    uint32_t i_flags;              /* 文件标志 */
    uint32_t i_osd1;               /* OS 相关数据 1 */
    uint32_t i_block[15];          /* 块指针数组 */
    uint32_t i_generation;         /* 生成号（用于 NFS） */
    uint32_t i_file_acl;           /* 文件 ACL */
    uint32_t i_dir_acl;            /* 目录 ACL */
    uint32_t i_faddr;              /* 片段地址 */
    uint8_t  i_osd2[12];           /* OS 相关数据 2 */
} ext2_inode_t;
```

最关键的是 `i_block[15]` 数组，它指向文件的数据块。这 15 个指针的设计非常巧妙，支持从小文件到超大文件的高效存储。

## 块指针布局

EXT2 inode 使用三级间接块来支持大文件：

```
i_block[0]    → 直接数据块
i_block[1]    → 直接数据块
...
i_block[11]   → 直接数据块 (12个直接块)

i_block[12]   → 单间接块 → 1024个块指针 → 数据块
i_block[13]   → 双间接块 → 1024个单间接块指针 → 数据块
i_block[14]   → 三间接块 → 1024个双间接块指针 → 数据块
```

这种设计平衡了空间效率和时间效率。小文件（< 48KB）只需要直接块，没有额外开销。中等文件（48KB ~ 4MB）使用单间接块，只需要一次间接访问。超大文件使用双间接和三间接块，虽然访问慢一些，但支持巨大的文件尺寸。

对于 4KB 块大小的文件系统，最大文件大小计算如下：
- 直接块：12 × 4KB = 48KB
- 单间接：1024 × 4KB = 4MB
- 双间接：1024 × 1024 × 4KB = 4GB
- 三间接：1024 × 1024 × 1024 × 4KB = 4TB

## 读取 inode

读取 inode 需要计算它在磁盘上的位置，然后读取对应的块：

```c
int ext2_read_inode(vfs_superblock_t* sb, vfs_inode_t* inode) {
    ext2_fs_info_t* fs_info = sb->s_fs_info;
    uint64_t ino = inode->i_ino;

    /* 计算块组 */
    uint32_t group = EXT2_INODE_GROUP(fs_info->sb, ino);

    /* 获取块组描述符 */
    ext2_block_group_desc_t* bgd = &fs_info->bg_desc[group];
    uint32_t inode_table_block = bgd->bg_inode_table;

    /* 计算组内索引 */
    uint32_t index = EXT2_LOCAL_INODE(fs_info->sb, ino);

    /* 计算 inode 在表中的偏移 */
    uint32_t inode_offset = index * fs_info->inode_size;

    /* 读取包含该 inode 的块 */
    uint32_t block_size = fs_info->block_size;
    uint32_t table_block_offset = inode_offset / block_size;
    uint32_t inode_block = inode_table_block + table_block_offset;
    uint32_t offset_in_block = inode_offset % block_size;

    uint8_t* block_buffer = kmalloc(block_size);
    block_read_sync(fs_info->device, inode_block * (block_size / 512),
                    block_buffer, block_size / 512);

    /* 获取 inode */
    ext2_inode_t* ext2_inode = (ext2_inode_t*)(block_buffer + offset_in_block);

    /* 填充 VFS inode */
    inode->i_mode = ext2_inode->i_mode;
    inode->i_size = ext2_inode->i_size;
    inode->i_blocks = ext2_inode->i_blocks;
    inode->i_nlink = ext2_inode->i_links_count;

    /* 复制块指针到私有数据 */
    ext2_inode_info_t* ei = kmalloc(sizeof(ext2_inode_info_t));
    memcpy(ei->i_data, ext2_inode->i_block, sizeof(ei->i_data));
    inode->i_private = ei;

    kfree(block_buffer);
    return 0;
}
```

这个函数的关键是正确计算 inode 的位置。首先确定它属于哪个块组，然后在该组的 inode 表中找到具体位置。由于多个 inode 可能存储在同一个块中，我们读取整个块，然后从正确偏移处提取数据。

## 块映射算法

文件读取时需要把文件偏移转换成块号，`ext2_get_block()` 实现这个功能：

```c
int ext2_get_block(vfs_inode_t* inode, uint64_t offset, uint64_t* block) {
    ext2_inode_info_t* ei = inode->i_private;
    ext2_fs_info_t* fs_info = inode->i_sb->s_fs_info;
    uint32_t block_size = fs_info->block_size;
    uint32_t block_in_file = offset / block_size;

    /* 直接块 (0-11) */
    if (block_in_file < EXT2_NDIR_BLOCKS) {
        *block = ei->i_data[block_in_file];
        return (*block == 0) ? 0 : 1;  /* 0 表示稀疏文件的空洞 */
    }

    /* 单间接块 */
    block_in_file -= EXT2_NDIR_BLOCKS;
    uint32_t ptrs_per_block = block_size / 4;

    if (block_in_file < ptrs_per_block) {
        uint32_t* indirect = kmalloc(block_size);

        /* 读取间接块 */
        uint64_t ind_block = ei->i_data[EXT2_IND_BLOCK];
        block_read_sync(fs_info->device, ind_block * (block_size / 512),
                        indirect, block_size / 512);

        *block = indirect[block_in_file];
        kfree(indirect);
        return (*block == 0) ? 0 : 1;
    }

    /* 双间接块和三间接块：类似但多一层间接 */
    klog_warn("ext2: Large file support not fully implemented\n");
    return -1;
}
```

这个函数处理直接块和单间接块。对于直接块，直接从 `i_data` 数组获取块号。对于间接块，需要先读取间接块本身，然后从其中获取目标块号。

## 稀疏文件支持

EXT2 支持稀疏文件（sparse file），即文件中有"空洞"的部分。如果 `i_block` 条目为 0，表示对应的数据块未分配，读取时应返回零。

```c
if (*block == 0) {
    /* 稀疏文件的空洞 */
    memset(buffer, 0, count);
    return count;
}
```

这种设计让大文件可以只占用实际使用的空间，而不是分配全部空间。比如创建一个 1GB 的文件但只写入了 1KB 数据，磁盘上只占用 1KB 加上一些间接块。

## 文件类型判断

inode 的 `i_mode` 字段编码了文件类型和权限：

```c
/* 文件类型掩码 */
#define EXT2_S_IFMT  0xF000

/* 文件类型 */
#define EXT2_S_IFREG 0x8000  /* 普通文件 */
#define EXT2_S_IFDIR 0x4000  /* 目录 */
#define EXT2_S_IFLNK 0xA000  /* 符号链接 */

/* 判断文件类型 */
uint16_t mode = ext2_inode->i_mode;
switch (mode & EXT2_S_IFMT) {
    case EXT2_S_IFREG:
        /* 普通文件 */
        inode->i_fop = &ext2_file_ops;
        break;
    case EXT2_S_IFDIR:
        /* 目录 */
        inode->i_fop = &ext2_dir_ops;
        inode->i_op = &ext2_dir_inode_ops;
        break;
}
```

## 小结

EXT2 inode 是文件系统的核心数据结构。它存储文件的元数据和块指针，使用三级间接块支持大文件。理解 inode 的结构和使用，是实现文件读写的基础。

稀疏文件支持让 EXT2 可以高效处理大文件。文件类型编码让同一个结构可以表示不同类型的文件系统对象。

下一步我们将学习 EXT2 的目录操作，看看如何通过目录项把文件名和 inode 关联起来。

---

<div align="center">

## 文档导航

[← EXT2 磁盘结构解析](07_EXT2磁盘结构解析.md)  | [EXT2 目录操作 →](09_EXT2目录操作.md)

</div>
