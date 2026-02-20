# CCOS EXT2 文件系统和 VFS 文档中心

本目录包含 CCOS Stage 25 - EXT2 文件系统和虚拟文件系统 (VFS) 开发的完整文档体系。

---

## 阶段概述

**Stage 25: EXT2 文件系统和 VFS**

本阶段在 Stage 24 ATA 驱动基础上，实现了完整的文件系统子系统。包括虚拟文件系统 (VFS) 抽象层、EXT2 文件系统实现和块设备层，标志着操作系统从简单的块设备访问迈向了完整的文件管理能力。

### 核心成果

- **虚拟文件系统 (VFS)** ([`kernel/fs/vfs/vfs.h`](../../kernel/fs/vfs/vfs.h))
  - 统一的文件系统抽象接口
  - 超级块管理 (vfs_superblock)
  - inode 缓存机制
  - 目录项缓存 (dentry)
  - 文件操作抽象
  - 挂载管理
  - 路径解析

- **EXT2 文件系统** ([`kernel/fs/ext2/`](../../kernel/fs/ext2/))
  - 超级块解析和验证
  - inode 读取和解析
  - 目录项操作 (lookup/readdir)
  - 文件读写操作
  - 块映射 (直接/间接块)
  - 块组描述符管理

- **块设备层** ([`kernel/fs/block/block.h`](../../kernel/fs/block/block.h))
  - 块设备抽象接口
  - 同步 I/O 操作
  - 设备注册管理
  - ATA 驱动集成

- **VFS 演示程序** ([`kernel/demo/vfs/vfs_demo.h`](../../kernel/demo/vfs/vfs_demo.h))
  - 目录遍历 (ls)
  - 文件读取 (cat)
  - 文件系统信息 (df)

---

## 目录结构

```
kernel/
├── fs/
│   ├── vfs/
│   │   ├── vfs.h                  # VFS 核心接口和数据结构
│   │   ├── vfs_superblock.c       # 超级块管理
│   │   ├── vfs_inode.c            # inode 缓存和管理
│   │   ├── vfs_dentry.c           # 目录项缓存
│   │   ├── vfs_file.c             # 文件操作
│   │   ├── vfs_mount.c            # 挂载管理
│   │   ├── vfs_open.c             # 文件打开
│   │   └── vfs_path.c             # 路径解析
│   ├── ext2/
│   │   ├── ext2.h                 # EXT2 公共接口
│   │   ├── ext2_internal.h        # EXT2 内部数据结构
│   │   ├── ext2_superblock.c      # 超级块解析
│   │   ├── ext2_inode.c           # inode 操作
│   │   ├── ext2_dir.c             # 目录操作
│   │   └── ext2_file.c            # 文件操作
│   ├── block/
│   │   ├── block.h                # 块设备接口
│   │   └── block.c                # 块设备 I/O
│   ├── fs.c                       # 文件系统注册
│   └── fs.h                       # 文件系统类型定义
├── demo/
│   └── vfs/
│       ├── vfs_demo.h             # VFS 演示接口
│       └── vfs_demo.c             # VFS 演示实现
└── CMakeLists.txt                 # 构建配置
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要文件系统
- VFS 设计基础
- 设计决策（为什么选择 EXT2、VFS 抽象层设计）
- 架构设计（分层架构、模块关系）
- 实现细节（挂载流程、文件打开、读写操作）
- 常见陷阱（缓存一致性、错误处理、边界检查）
- 未来改进方向（异步 I/O、更多文件系统支持）

**适合**:
- 理解设计思路
- 学习文件系统原理
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- VFS API 完整参考
- EXT2 API 参考
- 块设备 API 参考
- 数据结构定义（vfs_superblock_t, vfs_inode_t, vfs_dentry_t, file_t）
- 常量定义
- 系统调用映射

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

## 快速开始

### 查看代码结构

1. **VFS 核心接口** → 查看 [`kernel/fs/vfs/vfs.h`](../../kernel/fs/vfs/vfs.h)
2. **EXT2 文件系统** → 查看 [`kernel/fs/ext2/ext2.h`](../../kernel/fs/ext2/ext2.h)
3. **块设备层** → 查看 [`kernel/fs/block/block.h`](../../kernel/fs/block/block.h)
4. **VFS 演示程序** → 查看 [`kernel/demo/vfs/vfs_demo.h`](../../kernel/demo/vfs/vfs_demo.h)

### 使用示例

```c
#include "fs/vfs/vfs.h"
#include "fs/block/block.h"

// 内核初始化时调用
void kernel_init(void) {
    // 1. 初始化块设备层
    block_init();

    // 2. 注册文件系统类型
    vfs_register_filesystem("ext2", &ext2_fs_type);

    // 3. 挂载根文件系统
    vfs_mount("hda", "/", "ext2", 0, NULL);
}

// 打开文件
file_t* file = vfs_open("/path/to/file", O_RDONLY);
if (file) {
    char buffer[1024];
    ssize_t bytes = vfs_read(file, buffer, sizeof(buffer));
    // ...
    vfs_close(file);
}

// 读取目录
file_t* dir = vfs_open("/path/to/dir", O_RDONLY);
if (dir) {
    struct dirent entry;
    while (vfs_readdir(dir, &entry) > 0) {
        klog_info("Found: %s\n", entry.d_name);
    }
    vfs_close(dir);
}
```

### EXT2 块映射结构

```
EXT2 Inode 块指针布局:
┌────────────────────────────────────────────────┐
│ i_block[0-11]   │ 直接块 (12 × 4KB = 48KB)     │
├────────────────────────────────────────────────┤
│ i_block[12]     │ 单间接块 (4MB)               │
│                 │ → 杗指针数组 → 数据块         │
├────────────────────────────────────────────────┤
│ i_block[13]     │ 双间接块 (4GB)               │
│                 │ → 单间接块指针数组 → ...      │
├────────────────────────────────────────────────┤
│ i_block[14]     │ 三间接块 (4TB)               │
│                 │ → 双间接块指针数组 → ...      │
└────────────────────────────────────────────────┘
```

---

## 与前一阶段对比

| 特性 | Stage 24 (ATA 驱动) | Stage 25 (EXT2 + VFS) |
|------|---------------------|----------------------|
| 文件系统 | 无 | 完整 VFS + EXT2 |
| 抽象层次 | 块设备 I/O | 三层架构 (VFS/FS/Block) |
| inode 管理 | 无 | 完整缓存机制 |
| 目录操作 | 无 | lookup/readdir |
| 路径解析 | 无 | 完整路径解析 |
| 挂载管理 | 无 | 动态挂载支持 |
| 文件操作 | 无 | open/read/write/close |
| 新增文件 | - | 30+ 个 |

---

## 技术亮点

### 1. VFS 面向对象设计

```c
// 超级块操作 - 函数指针表
typedef struct super_operations {
    int (*read_inode)(struct vfs_inode* inode);
    int (*write_inode)(struct vfs_inode* inode);
    int (*put_super)(struct vfs_superblock* sb);
    // ...
} super_operations_t;

// inode 操作 - 函数指针表
typedef struct inode_operations {
    int (*lookup)(struct vfs_inode* dir, const char* name,
                  struct vfs_dentry* dentry);
    int (*create)(struct vfs_inode* dir, const char* name,
                  int mode, struct vfs_dentry* dentry);
    // ...
} inode_operations_t;

// 文件操作 - 函数指针表
typedef struct file_operations {
    ssize_t (*read)(struct file* file, char* buf, size_t count);
    ssize_t (*write)(struct file* file, const char* buf, size_t count);
    int (*close)(struct file* file);
    // ...
} file_operations_t;
```

### 2. Inode 缓存机制

```c
// 哈希表缓存
#define INODE_HASH_SIZE  256

static struct hlist_head inode_cache_hash[INODE_HASH_SIZE];

// 基于 (superblock, ino) 的哈希查找
static uint32_t inode_hash(struct vfs_superblock* sb, uint64_t ino) {
    return (uint32_t)((ino ^ (uint64_t)sb) % INODE_HASH_SIZE);
}

struct vfs_inode* vfs_iget(struct vfs_superblock* sb, uint64_t ino) {
    uint32_t hash = inode_hash(sb, ino);
    // 1. 查找缓存
    // 2. 如果未命中，从磁盘读取
    // 3. 加入缓存
}
```

### 3. EXT2 超级块解析

```c
typedef struct ext2_superblock {
    uint32_t s_inodes_count;       // inode 总数
    uint32_t s_blocks_count;       // 块总数
    uint32_t s_r_blocks_count;     // 保留块数
    uint32_t s_free_blocks_count;  // 空闲块数
    uint32_t s_free_inodes_count;  // 空闲 inode 数
    uint32_t s_first_data_block;   // 第一个数据块
    uint32_t s_log_block_size;     // 块大小 (2^s)
    uint32_t s_blocks_per_group;   // 每组块数
    uint32_t s_inodes_per_group;   // 每组 inode 数
    uint32_t s_magic;              // 魔数 0xEF53
    // ...
} ext2_superblock_t;
```

### 4. 块设备抽象

```c
typedef struct block_device {
    int dev_id;                    // 设备 ID (0-3)
    uint32_t block_size;           // 块大小
    uint64_t nblocks;              // 块数量
    struct block_device_ops* ops;  // 操作函数指针
    void* private_data;            // 私有数据
} block_device_t;

typedef struct block_device_ops {
    int (*read)(struct block_device* dev, uint64_t block,
                void* buffer, size_t nblocks);
    int (*write)(struct block_device* dev, uint64_t block,
                 const void* buffer, size_t nblocks);
    int (*flush)(struct block_device* dev);
} block_device_ops_t;
```

### 5. VFS 分层架构

```
┌─────────────────────────────────────────────────────────┐
│                     用户空间程序                         │
│                  (open, read, write)                    │
└────────────────────────┬────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                      VFS 层                              │
│  统一接口: vfs_open, vfs_read, vfs_write, vfs_close     │
│  - 路径解析     - 权限检查     - 缓存管理                │
└────────────────────────┬────────────────────────────────┘
                         │
         ┌───────────────┼───────────────┐
         ▼               ▼               ▼
┌─────────────┐  ┌─────────────┐  ┌─────────────┐
│   EXT2      │  │   FAT32     │  │   ...       │
│  文件系统    │  │  文件系统    │  │             │
└──────┬──────┘  └──────┬──────┘  └─────────────┘
       │                │
       └────────┬───────┘
                ▼
┌─────────────────────────────────────────────────────────┐
│                     块设备层                             │
│  - 块 I/O 调度   - 缓冲区管理    - 设备管理              │
└────────────────────────┬────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                    ATA 驱动                             │
│  - PIO/DMA     - 中断处理     - 设备控制                 │
└─────────────────────────────────────────────────────────┘
```

---

## 文档关系图

```
                    ┌──────────────────┐
                    │   项目根目录    │
                    │  (PROGRESS.md)   │
                    └────────┬─────────┘
                             │
                    ┌────────▼─────────┐
                    │  document/       │
                    └────────┬─────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
┌───────▼──────┐   ┌─────────▼──────┐   ┌─────────▼──────┐
│ 开发笔记     │   │ 技术参考        │   │ README.md      │
│ (设计思路)    │   │ (API手册)       │   │ (快速开始)      │
└──────────────┘   └────────────────┘   └────────────────┘
        │                    │
        └──────────────────┬─────────┘
                           │
                   ┌──────▼──────┐
                   │ VFS/EXT2     │
                   │ 源代码       │
                   └─────────────┘
```

---

## 版本信息

- **阶段**: Stage 25
- **分支**: `stage/25_ext2_vfs`
- **日期**: 2026-02-18
- **作者**: CharlieChen

---

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../24_ata_driver/README.md](../24_ata_driver/) - 上一阶段文档

### 源码文件
- [`kernel/fs/vfs/vfs.h`](../../kernel/fs/vfs/vfs.h) - VFS 核心接口
- [`kernel/fs/ext2/ext2.h`](../../kernel/fs/ext2/ext2.h) - EXT2 接口
- [`kernel/fs/block/block.h`](../../kernel/fs/block/block.h) - 块设备接口
- [`kernel/demo/vfs/vfs_demo.h`](../../kernel/demo/vfs/vfs_demo.h) - 演示程序

### 外部参考
- [EXT2 文件系统](https://wiki.osdev.org/Ext2)
- [VFS 设计](https://www.kernel.org/doc/html/latest/filesystems/vfs.html)
- [Second Extended Filesystem](https://www.kernel.org/doc/Documentation/filesystems/ext2.txt)
- [OSDev.org](https://wiki.osdev.org)

---

**作者**: CharlieChen
**最后更新**: 2026-02-18
