# VFS 核心结构定义 —— 用 C 实现面向对象

有了块设备层作为基础，现在我们要进入 VFS 层的核心设计了。这一层是整个文件系统的关键，它定义了文件、目录、超级块等核心概念，并且要用 C 语言实现面向对象的设计模式。

说实话，用 C 语言实现面向对象有点像戴着镣铐跳舞，但当你看到函数指针表的妙用时，会发现这种设计其实非常优雅。Linux 内核就是用这种方式实现了高度可扩展的架构，我们这里也是借鉴了同样的思路。

## 面向对象在 C 中的实现

在 C++ 或 Java 中，我们这样定义一个类：

```cpp
class File {
    virtual int read(char* buffer, size_t count);
    virtual int write(const char* buffer, size_t count);
    virtual int close();
};
```

编译器会在背后创建一个虚函数表（vtable），每个对象都有指向这个表的指针。当我们调用 `file->read()` 时，实际上是查表找到正确的函数来执行。

在 C 语言中，我们需要显式地做这件事：

```c
struct file_operations {
    ssize_t (*read)(struct file* file, char* buffer, size_t count, uint64_t* pos);
    ssize_t (*write)(struct file* file, const char* buffer, size_t count, uint64_t* pos);
    int (*release)(struct vfs_inode* inode, struct file* file);
};

struct file {
    const struct file_operations* f_op;  /* 指向操作表 */
    /* ... 其他字段 */
};
```

当我们调用 `file->f_op->read(file, buffer, count, &pos)` 时，效果和 C++ 的虚函数调用是一样的。关键在于 `f_op` 指针可以在运行时指向不同的操作表，从而实现多态。

## VFS 的四个核心概念

VFS 定义了四个核心数据结构，它们分别对应文件系统的不同抽象层面：

**超级块 (superblock)**：代表一个已挂载的文件系统。每个文件系统被挂载时都会创建一个超级块，存储文件系统的全局信息，比如块大小、总块数、根 inode 等。

**Inode**：代表一个文件或目录的元数据。它存储文件的大小、权限、时间戳等信息，但不存储文件名。文件名是通过 dentry 与 inode 关联的。

**Dentry**：目录项，记录文件名到 inode 的映射。它是路径解析的关键，通过缓存 dentry 可以加速路径查找。

**File**：代表一个打开的文件实例。当进程打开文件时创建，存储文件位置、访问模式等与具体打开相关的状态。

这四个结构层层递进：superblock 包含根 inode，inode 通过 dentry 与文件名关联，打开文件时创建 file 结构。理解了它们之间的关系，就理解了 VFS 的核心架构。

## 超级块结构

超级块是文件系统在内存中的代表，存储文件系统的全局信息：

```c
struct vfs_superblock {
    list_head s_list;              /* 所有超级块的链表 */
    list_head s_mounts;            /* 挂载点链表 */

    uint32_t s_dev;                /* 设备标识 */
    uint64_t s_blocksize;          /* 块大小（字节） */
    uint64_t s_maxbytes;           /* 最大文件大小 */

    uint32_t s_magic;              /* 文件系统魔数 */
    char s_fsname[32];             /* 文件系统名称 */

    vfs_inode_t* s_root;           /* 根 inode */

    uint64_t s_blocks;             /* 总块数 */
    uint64_t s_bfree;              /* 空闲块数 */
    uint64_t s_files;              /* 总 inode 数 */
    uint64_t s_ffree;              /* 空闲 inode 数 */

    void* s_fs_info;               /* 文件系统私有数据 */
    const super_operations_t* s_op; /* 超级块操作 */

    uint32_t s_flags;              /* 挂载标志 */
    uint32_t s_count;              /* 引用计数 */

    vfs_dentry_t* s_mount;         /* 挂载点 dentry */
    vfs_mount_t* s_mount_instance; /* 挂载实例 */
};
```

这个结构里有几个关键点值得讨论。`s_fs_info` 是一个 `void*` 指针，用于存储文件系统特定的信息。比如 EXT2 会把 `ext2_fs_info_t` 结构挂在这里，而 FAT32 可以挂完全不同的结构。这种设计让 VFS 可以管理不同类型的文件系统而不需要知道具体细节。

`s_op` 是超级块操作表，定义了超级块可以执行的操作：

```c
struct super_operations {
    int (*read_inode)(vfs_superblock_t* sb, vfs_inode_t* inode);
    int (*write_inode)(vfs_superblock_t* sb, vfs_inode_t* inode);
    int (*statfs)(vfs_superblock_t* sb, struct statfs* buf);
    int (*alloc_inode)(vfs_superblock_t* sb, vfs_inode_t* inode);
    void (*destroy_inode)(vfs_inode_t* inode);
    void (*put_super)(vfs_superblock_t* sb);
};
```

每个文件系统实现自己的操作函数，然后把这些函数的指针填入操作表。当 VFS 需要读取 inode 时，只需调用 `sb->s_op->read_inode(sb, inode)`，具体执行的是 EXT2 还是其他文件系统的代码，由 `s_op` 指针决定。

## Inode 结构

Inode 是 VFS 中最重要的结构之一，代表文件或目录的元数据：

```c
struct vfs_inode {
    list_head i_hash;              /* 哈希表链表 */
    list_head i_list;              /* 所有 inode 的链表 */
    list_head i_sb_list;           /* 超级块的 inode 链表 */

    vfs_superblock_t* i_sb;        /* 所属超级块 */

    uint64_t i_ino;                /* inode 号 */
    uint32_t i_nlink;              /* 硬链接数 */

    uint32_t i_mode;               /* 文件模式（类型 + 权限） */

    uint64_t i_size;               /* 文件大小（字节） */
    uint64_t i_blocks;             /* 512字节块数 */

    uint64_t i_atime;              /* 访问时间 */
    uint64_t i_mtime;              /* 修改时间 */
    uint64_t i_ctime;              /* 状态改变时间 */

    uint32_t i_count;              /* 引用计数 */
    uint32_t i_state;              /* 状态标志 */

    const inode_operations_t* i_op;  /* inode 操作 */
    const file_operations_t* i_fop;  /* 文件操作 */

    void* i_private;               /* 文件系统私有数据 */
};
```

这里有个重要的概念需要理解：**inode 没有文件名**。inode 只是存储文件的元数据，文件名是通过 dentry 与 inode 关联的。这个设计让硬链接成为可能 —— 多个文件名（多个 dentry）可以指向同一个 inode。

`i_op` 和 `i_fop` 分别是 inode 操作表和文件操作表。inode 操作用于目录相关操作，比如查找文件、创建文件：

```c
struct inode_operations {
    int (*lookup)(vfs_inode_t* dir, const char* name, vfs_inode_t** result);
    int (*create)(vfs_inode_t* dir, const char* name, int mode, vfs_inode_t** result);
    int (*mkdir)(vfs_inode_t* dir, const char* name, int mode);
    int (*unlink)(vfs_inode_t* dir, const char* name);
    /* ... */
};
```

文件操作用于打开后的文件操作，比如读写：

```c
struct file_operations {
    uint64_t (*llseek)(file_t* file, uint64_t offset, int whence);
    ssize_t (*read)(file_t* file, char* buffer, size_t count, uint64_t* pos);
    ssize_t (*write)(file_t* file, const char* buffer, size_t count, uint64_t* pos);
    int (*readdir)(file_t* file, void* dirent, filldir_t filldir);
    int (*open)(vfs_inode_t* inode, file_t* file);
    int (*release)(vfs_inode_t* inode, file_t* file);
};
```

你可能注意到 inode 有两个操作表：`i_op` 和 `i_fop`。这是因为 inode 可以以不同方式被打开。比如一个普通文件被打开时使用文件操作，但如果是设备文件，可能使用不同的操作。不过在我们的实现中，大多数情况下一个 inode 类型对应固定的 `i_fop`。

## Dentry 结构

Dentry（目录项）是路径解析的关键，它记录文件名到 inode 的映射：

```c
struct vfs_dentry {
    list_head d_hash;              /* 哈希表链表 */
    list_head d_lru;               /* LRU 链表 */
    list_head d_subdirs;           /* 子目录链表 */

    vfs_dentry_t* d_parent;        /* 父目录 */
    vfs_inode_t* d_inode;          /* 关联的 inode */

    char d_name[256];              /* 文件名 */

    uint32_t d_flags;              /* 状态标志 */
    uint32_t d_count;              /* 引用计数 */

    uint64_t d_time;               /* 重新验证时间 */

    const dentry_operations_t* d_op; /* dentry 操作 */
    vfs_superblock_t* d_sb;        /* 超级块 */
};
```

Dentry 有几个有趣的特性。首先是父子关系，`d_parent` 指向父目录的 dentry，这构成了一个目录树。每个 dentry 都在父目录的 `d_subdirs` 链表中，可以遍历一个目录的所有子项。

其次是"负 dentry"的概念。当一个文件不存在时，我们会创建一个没有关联 inode 的 dentry，并标记为"负"。这样下次查找同一个文件时，可以立即知道它不存在，而不需要去磁盘查找。

最后是 LRU 链表。当内存紧张时，可以从 LRU 链表尾部开始释放 dentry，因为尾部是最久未使用的。

## File 结构

File 代表一个打开的文件，存储与具体打开相关的状态：

```c
struct file {
    list_head f_list;              /* 打开文件链表 */

    vfs_inode_t* f_inode;          /* 关联的 inode */
    vfs_dentry_t* f_dentry;        /* 关联的 dentry */

    uint64_t f_pos;                /* 当前文件位置 */
    uint32_t f_flags;              /* 打开标志 (O_*) */
    uint32_t f_mode;               /* 文件模式 (FMODE_*) */

    uint32_t f_count;              /* 引用计数 */

    const file_operations_t* f_op; /* 文件操作 */

    void* f_private_data;          /* 私有数据 */

    vfs_mount_t* f_vfsmnt;         /* VFS 挂载 */
};
```

File 和 inode 的区别很重要。inode 代表文件系统中的文件本身，而 file 代表进程对文件的一次打开。同一个文件可以被多次打开，每次打开都有独立的 file 结构和文件位置。这就是为什么多个进程可以同时读取同一个文件而互不干扰。

`f_pos` 存储当前读写位置，每次 `read()` 或 `write()` 后会更新。`f_flags` 存储打开时的标志，比如是只读还是读写。这些状态是与具体打开相关的，不应该存在 inode 中。

## 文件类型和打开标志

VFS 定义了标准的文件类型常量，与 POSIX 兼容：

```c
typedef enum vtype {
    V_UNKNOWN = 0,
    V_FIFO    = 0010000,  /* 命名管道 */
    V_CHR     = 0020000,  /* 字符设备 */
    V_DIR     = 0040000,  /* 目录 */
    V_BLK     = 0060000,  /* 块设备 */
    V_REG     = 0100000,  /* 普通文件 */
    V_LNK     = 0120000,  /* 符号链接 */
    V_SOCK    = 0140000,  /* 套接字 */
} vtype_t;
```

这些类型存储在 inode 的 `i_mode` 字段中，通过掩码操作可以提取出来：

```c
#define V_S_IFMT  0170000  /* 文件类型掩码 */
```

打开标志定义了文件被打开的方式：

```c
#define O_RDONLY    0x0000  /* 只读 */
#define O_WRONLY    0x0001  /* 只写 */
#define O_RDWR      0x0002  /* 读写 */
#define O_CREAT     0x0040  /* 创建文件 */
#define O_TRUNC     0x0200  /* 截断文件 */
#define O_APPEND    0x0400  /* 追加写入 */
```

## 设计的妙处

到这里你可能已经发现，VFS 的设计有几个很巧妙的地方。

首先是**数据结构与操作的分离**。每个对象都存储指向操作表的指针，而不是直接嵌入函数。这意味着同一类型的对象可以共享操作表，节省内存。更重要的是，这种设计让多态成为可能 —— 不同的文件系统可以提供不同的操作实现。

其次是**三层抽象**。superblock 在最上层，代表整个文件系统；inode 在中间层，代表文件或目录的元数据；dentry 在下层，处理路径解析和文件名映射。每一层都有明确的职责，不会互相干扰。

最后是**引用计数的广泛应用**。superblock、inode、dentry、file 都有引用计数。这确保了对象在使用期间不会被释放，也支持多用户共享同一资源。当引用计数降为 0 时，对象才能被安全释放。

## 下一步

现在我们已经定义了 VFS 的核心数据结构。但定义结构只是第一步，接下来我们需要实现这些结构的管理机制。

首先需要实现的是 inode 缓存。因为频繁访问磁盘太慢了，我们需要把常用的 inode 缓存在内存中。这个缓存机制需要支持哈希查找、引用计数、脏标记等功能。

然后是 dentry 缓存，用于加速路径解析。当我们查找 `/usr/bin/ls` 时，如果每个组件都在缓存中，就不需要每次都访问磁盘。

接下来我们会一一实现这些机制，让 VFS 从"定义"变成"可用"。

---

<div align="center">

## 文档导航

[← 从零搭建块设备层](02_从零搭建块设备层.md)  | [实现 inode 缓存管理 →](04_实现inode缓存管理.md)

</div>
