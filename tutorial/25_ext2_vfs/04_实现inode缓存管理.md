# 实现 inode 缓存管理 —— 文件系统的记忆中枢

在上一篇文章中，我们定义了 VFS 的核心数据结构。现在让我们来实现第一个核心组件：inode 缓存。

为什么需要 inode 缓存？想想看，如果你频繁地访问同一个文件，每次都要从磁盘读取它的 inode，那效率就太低了。更糟糕的是，判断一个文件是否存在于目录中，需要读取目录的 inode 来查找文件名。没有缓存的话，每次路径解析都要进行多次磁盘读取，性能会非常差。

inode 缓存的作用就是把常用的 inode 保存在内存中，避免重复访问磁盘。它的设计需要解决几个问题：如何快速查找 inode、如何管理引用计数、如何处理脏 inode、何时释放缓存。

## 缓存数据结构设计

inode 缓存使用哈希表来实现快速查找。每个 inode 通过一个哈希值映射到哈希表的一个桶中，同一桶中的 inode 用链表组织。

```c
#define INODE_HASH_BITS    8
#define INODE_HASH_SIZE    (1 << INODE_HASH_BITS)  /* 256 */

static struct {
    list_head hash_table[INODE_HASH_SIZE];  /* 哈希表 */
    list_head inodes;                       /* 所有 inode 的链表 */
    uint32_t count;                         /* 缓存的 inode 数量 */
} inode_cache;
```

哈希表的大小选择很有讲究。256 个桶是一个平衡的选择 —— 足够减少冲突，又不会占用太多内存。如果缓存中的 inode 数量远大于桶数，冲突会增多，查找性能会下降。不过在单用户操作系统的早期实现中，256 个桶足够了。

## 哈希函数

哈希函数是缓存性能的关键。我们需要把 inode 号和超级块组合成一个哈希值：

```c
static uint32_t inode_hash(uint64_t ino, vfs_superblock_t* sb) {
    /* 结合 inode 号和超级块指针 */
    uint64_t sb_ptr = (uint64_t)(uintptr_t)sb;
    uint32_t hash = (uint32_t)(ino ^ (sb_ptr >> 3));
    return hash & (INODE_HASH_SIZE - 1);
}
```

这个哈希函数的设计有几个考虑。首先，我们把 inode 号和超级块指针混合在一起。这是因为不同文件系统可能有相同的 inode 号，只有同时考虑超级块才能唯一确定一个 inode。

其次，我们对超级块指针进行了右移操作。这是因为指针通常有对齐要求，低位几 bit 往往是 0，右移可以避免这部分浪费。

最后，哈希值通过与 `(INODE_HASH_SIZE - 1)` 进行掩码操作来限制在有效范围内。由于 `INODE_HASH_SIZE` 是 2 的幂，这个操作等同于取模，但位运算更快。

## 分配和释放 inode

缓存管理的核心是 inode 的分配和释放。

```c
vfs_inode_t* vfs_alloc_inode(vfs_superblock_t* sb) {
    if (!sb) {
        return NULL;
    }

    vfs_inode_t* inode = kmalloc(sizeof(vfs_inode_t));
    if (!inode) {
        return NULL;
    }

    /* 初始化所有字段为 0 */
    memset(inode, 0, sizeof(vfs_inode_t));
    INIT_LIST_HEAD(&inode->i_hash);
    INIT_LIST_HEAD(&inode->i_list);
    INIT_LIST_HEAD(&inode->i_sb_list);

    /* 设置基本属性 */
    inode->i_sb = sb;
    inode->i_nlink = 1;
    inode->i_count = 1;

    /* 加入全局链表 */
    list_add(&inode->i_list, &inode_cache.inodes);
    inode_cache.count++;

    return inode;
}
```

分配函数首先从堆上分配内存，然后把所有字段初始化为 0。这很重要，因为 C 语言不会自动初始化结构体的字段。如果不显式初始化，字段会包含随机的垃圾值，可能导致难以调试的问题。

`i_nlink` 初始化为 1，因为新分配的 inode 至少被自身引用一次（硬链接计数）。`i_count` 初始化为 1，因为调用者当前正持有这个 inode 的引用。

释放函数需要处理更多的细节：

```c
void vfs_free_inode(vfs_inode_t* inode) {
    if (!inode) {
        return;
    }

    /* 从哈希表移除 */
    inode_hash_remove(inode);

    /* 从全局链表移除 */
    list_del(&inode->i_list);
    inode_cache.count--;

    /* 调用文件系统特定的销毁函数 */
    if (inode->i_sb && inode->i_sb->s_op &&
        inode->i_sb->s_op->destroy_inode) {
        inode->i_sb->s_op->destroy_inode(inode);
    }

    /* 释放私有数据 */
    if (inode->i_private) {
        kfree(inode->i_private);
    }

    kfree(inode);
}
```

这里有个值得注意的设计：我们调用文件系统的 `destroy_inode` 函数。这是因为 VFS 的 inode 结构只包含通用部分，每个文件系统可能有自己扩展的私有数据。`destroy_inode` 负责释放这些私有数据。

## 获取 inode：vfs_iget

`vfs_iget()` 是 inode 缓存的核心函数。它根据超级块和 inode 号查找或创建 inode：

```c
vfs_inode_t* vfs_iget(vfs_superblock_t* sb, uint64_t ino) {
    if (!sb) {
        return NULL;
    }

    uint32_t hash = inode_hash(ino, sb);

    /* 在缓存中查找 */
    vfs_inode_t* inode;
    list_for_each_entry(inode, &inode_cache.hash_table[hash], i_hash) {
        if (inode->i_sb == sb && inode->i_ino == ino) {
            /* 命中缓存，递增引用计数 */
            inode->i_count++;
            return inode;
        }
    }

    /* 缓存未命中，分配新 inode */
    inode = vfs_alloc_inode(sb);
    if (!inode) {
        return NULL;
    }

    inode->i_ino = ino;

    /* 从文件系统读取 inode */
    if (sb->s_op && sb->s_op->read_inode) {
        int result = sb->s_op->read_inode(sb, inode);
        if (result != 0) {
            vfs_free_inode(inode);
            return NULL;
        }
    }

    /* 加入哈希表 */
    inode_hash_insert(inode);

    return inode;
}
```

这个函数的逻辑很清晰：首先在哈希表中查找，如果找到就递增引用计数后返回；如果没找到，就分配新的 inode 并从磁盘读取。

这里有个重要的设计决策：**查找时需要同时匹配超级块和 inode 号**。这是因为不同文件系统可能有相同的 inode 号，必须同时考虑两者才能唯一确定。

另一个细节是调用文件系统的 `read_inode` 函数。VFS 不知道如何从磁盘读取特定文件系统的 inode，它只是提供一个框架，具体的读取工作由文件系统实现。这种设计让 VFS 可以支持多种文件系统。

## 释放 inode：vfs_iput

当不再需要 inode 时，调用 `vfs_iput()` 释放引用：

```c
void vfs_iput(vfs_inode_t* inode) {
    if (!inode) {
        return;
    }

    /* 递减引用计数 */
    if (inode->i_count > 0) {
        inode->i_count--;
    }

    /* 如果引用计数为 0 且 inode 是脏的，写回磁盘 */
    if (inode->i_count == 0 && (inode->i_state & I_DIRTY)) {
        if (inode->i_sb && inode->i_sb->s_op &&
            inode->i_sb->s_op->write_inode) {
            inode->i_sb->s_op->write_inode(inode->i_sb, inode);
        }
    }

    /* 如果引用计数为 0，释放 inode */
    if (inode->i_count == 0) {
        vfs_free_inode(inode);
    }
}
```

这个函数处理了几个重要的事情。首先是引用计数的递减，这是基础的生命周期管理。

其次是**写回脏 inode**。当 inode 的元数据被修改后（比如文件大小改变），它会被标记为脏（`I_DIRTY`）。在释放 inode 之前，如果它是脏的，需要写回磁盘。这是延迟写策略的一部分，可以减少磁盘 I/O。

最后是**引用计数为 0 时释放 inode**。只有当没有任何地方引用这个 inode 时，才能安全释放。这避免了悬空指针的问题。

## 脏 inode 管理

当 inode 的元数据被修改时，需要标记为脏：

```c
void vfs_mark_inode_dirty(vfs_inode_t* inode) {
    if (inode) {
        inode->i_state |= I_DIRTY;
    }
}
```

这只是一个简单的位操作，但背后有一套完整的写回机制。脏 inode 会在以下情况被写回：显式调用 `vfs_iput()`、文件系统卸载时、或者周期性的同步操作。

这种延迟写策略可以显著提高性能。想象一下，你创建了一个新文件，这会修改目录的 inode（添加新目录项）和分配新 inode。如果每次修改都立即写磁盘，性能会很差。延迟写让多个修改可以批量写回，减少磁盘 I/O 次数。

## 类型检查辅助函数

inode 的 `i_mode` 字段编码了文件类型，我们提供一些辅助函数来检查：

```c
vtype_t vfs_mode_to_type(uint32_t mode) {
    switch (mode & V_S_IFMT) {
        case V_FIFO: return V_FIFO;
        case V_CHR:  return V_CHR;
        case V_DIR:  return V_DIR;
        case V_BLK:  return V_BLK;
        case V_REG:  return V_REG;
        case V_LNK:  return V_LNK;
        case V_SOCK: return V_SOCK;
        default:     return V_UNKNOWN;
    }
}

bool vfs_is_dir(vfs_inode_t* inode) {
    return inode && vfs_mode_to_type(inode->i_mode) == V_DIR;
}

bool vfs_is_reg(vfs_inode_t* inode) {
    return inode && vfs_mode_to_type(inode->i_mode) == V_REG;
}
```

这些函数通过掩码操作提取文件类型，然后进行比较。虽然代码简单，但它们让上层代码更清晰 —— `if (vfs_is_dir(inode))` 比 `if ((inode->i_mode & V_S_IFMT) == V_DIR)` 更易读。

## 缓存初始化

最后，我们需要在系统启动时初始化 inode 缓存：

```c
void vfs_inode_cache_init(void) {
    /* 初始化哈希表 */
    for (int i = 0; i < INODE_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&inode_cache.hash_table[i]);
    }
    INIT_LIST_HEAD(&inode_cache.inodes);
    inode_cache.count = 0;
}
```

这个函数需要在 VFS 子系统初始化时调用，确保哈希表和链表都处于正确的初始状态。

## 使用示例

让我们看一下 inode 缓存是如何使用的。假设 EXT2 文件系统需要读取一个 inode：

```c
/* EXT2 实现 read_inode 操作 */
static int ext2_read_inode(vfs_superblock_t* sb, vfs_inode_t* inode) {
    /* 从磁盘读取 EXT2 inode */
    ext2_fs_info_t* fs_info = sb->s_fs_info;
    ext2_inode_info_t* ei = kmalloc(sizeof(ext2_inode_info_t));

    /* 计算 inode 在磁盘上的位置 */
    uint32_t group = EXT2_INODE_GROUP(sb, inode->i_ino);
    uint32_t local_ino = EXT2_LOCAL_INODE(sb, inode->i_ino);

    /* 读取 inode 数据 */
    /* ... */

    /* 填充 VFS inode */
    inode->i_mode = ext2_inode.i_mode;
    inode->i_size = ext2_inode.i_size;
    /* ... */

    inode->i_private = ei;
    return 0;
}

/* EXT2 注册操作表 */
static const super_operations_t ext2_super_ops = {
    .read_inode = ext2_read_inode,
    .write_inode = ext2_write_inode,
    /* ... */
};
```

当 VFS 调用 `vfs_iget(sb, ino)` 时，会自动调用 EXT2 的 `read_inode` 函数。EXT2 从磁盘读取数据，填充 VFS inode 结构，并把 EXT2 特定的数据存放在 `i_private` 中。这种设计让 VFS 和文件系统各司其职，互不干扰。

## 小结

inode 缓存是文件系统性能的关键。通过把常用的 inode 保存在内存中，我们避免了大量的磁盘 I/O。哈希表设计让查找快速高效，引用计数确保了正确的生命周期管理，脏标记和延迟写策略进一步优化了性能。

接下来我们需要实现 dentry 缓存，它处理文件名到 inode 的映射，是路径解析的基础。有了 inode 缓存和 dentry 缓存，我们就可以实现完整的路径解析功能了。

---

<div align="center">

## 文档导航

[← VFS 核心结构定义](03_VFS核心结构定义.md)  | [实现目录项缓存 →](05_实现目录项缓存.md)

</div>
