# 实现 kmalloc 核心分配逻辑

在上一篇文章中，我们实现了堆分配器的核心数据结构。现在让我们来实现最核心的部分 —— `kmalloc` 分配逻辑。这部分代码是堆分配器的灵魂，包含了 Best-Fit 搜索算法、块分割、空闲链表管理等关键操作。

---

## 环境说明

这篇文章假设你已经完成了前面的步骤，`heap.h` 和 `heap.c` 的框架已经搭好，编译能通过。我们会在 `heap.c` 中继续添加代码。

---

## 第一步：实现空闲链表操作

在实现 `kmalloc` 之前，我们需要先实现一些操作空闲链表的辅助函数。空闲链表是堆分配器的核心数据结构，所有空闲的块都挂在这条链表上，按地址排序。

在 `heap.c` 中添加：

```c
/**
 * remove_from_free_list - Remove a block from free list
 */
static void remove_from_free_list(heap_block_t* block) {
    CCOS_ASSERT(!block->used);

    heap_block_t* prev = block->prev;
    heap_block_t* next = block->next;

    if (prev) {
        prev->next = next;
    } else {
        /* This was the head of free list */
        s_heap.free_list = next;
    }

    if (next) {
        next->prev = prev;
    }

    block->prev = NULL;
    block->next = NULL;
}

/**
 * insert_to_free_list - Insert a block to free list (sorted by address)
 */
static void insert_to_free_list(heap_block_t* block) {
    CCOS_ASSERT(!block->used);
    CCOS_ASSERT(block->prev == NULL);
    CCOS_ASSERT(block->next == NULL);

    /* Insert sorted by address for better coalescing */
    heap_block_t* curr = s_heap.free_list;
    heap_block_t* prev = NULL;

    while (curr && curr < block) {
        prev = curr;
        curr = curr->next;
    }

    block->next = curr;
    if (curr) {
        curr->prev = block;
    }
    block->prev = prev;
    if (prev) {
        prev->next = block;
    } else {
        s_heap.free_list = block;
    }
}
```

`remove_from_free_list` 从空闲链表中移除一个块。这里有个细节需要注意：空闲链表使用双向链表结构，`prev` 和 `next` 指针只在块空闲时有效。所以移除时需要处理前驱和后继的链接关系。如果被移除的块是链表头，需要更新 `s_heap.free_list` 指针。

`insert_to_free_list` 把一个块插入空闲链表。这里我们按地址排序插入，而不是插在链表头或链表尾。按地址排序的好处是，相邻的空闲块在链表中也相邻，这样在合并时查找邻居更方便。插入时我们遍历链表找到合适的插入位置，保持链表的有序性。

这两个函数都有一个断言检查，确保块是空闲的。对于 `insert_to_free_list`，我们还检查块的 `prev` 和 `next` 指针都是空的，确保块不在任何链表中。这些断言能在开发阶段快速发现 bug。

---

## 第二步：实现 Best-Fit 搜索算法

现在我们来实现核心的 Best-Fit 搜索算法。这个函数在空闲链表中搜索，找到满足大小要求的最小块。

在 `heap.c` 中添加：

```c
/**
 * find_best_fit - Find the best fitting free block (best-fit algorithm)
 *
 * @param size Minimum size required (including header)
 * @return Best fitting block, or NULL if no suitable block found
 */
static heap_block_t* find_best_fit(size_t size) {
    heap_block_t* best = NULL;
    heap_block_t* curr = s_heap.free_list;

    while (curr) {
        if (curr->size >= size) {
            if (best == NULL || curr->size < best->size) {
                best = curr;
                /* Exact match is optimal - no need to search further */
                if (curr->size == size) {
                    break;
                }
            }
        }
        curr = curr->next;
    }

    return best;
}
```

这个函数的逻辑很直接。我们遍历整个空闲链表，对于每个足够大的块，如果它比当前的最佳块更小，就更新最佳块。如果我们找到一个精确匹配（大小正好相等），就立即返回，因为不可能有更好的选择了。

这里有个优化点：如果找到精确匹配，立即返回。这能节省遍历链表的时间。在实际的内核场景中，很多分配的大小是固定的（比如链表节点、字符串缓冲区），精确匹配的概率不低，这个优化很有价值。

但要注意，Best-Fit 算法的时间复杂度是 O(n)，n 是空闲块的数量。如果空闲块很多，分配会变慢。对于内核场景来说，这通常不是问题，因为内核的内存分配模式相对简单，空闲块数量不会太多。但如果以后发现性能问题，可以考虑用更高效的数据结构，比如平衡二叉搜索树。

---

## 第三步：实现块分割函数

当我们找到一个足够大的空闲块，但它比我们需要的大很多时，我们应该把块分割成两部分。这样可以避免浪费。

在 `heap.c` 中添加：

```c
/**
 * split_block - Split a block if it's large enough
 *
 * @param block The block to split (must be removed from free list first)
 * @param needed_size Size needed for the first part
 */
static void split_block(heap_block_t* block, size_t needed_size) {
    size_t remaining = block->size - needed_size;

    /* Only split if remaining is large enough for a new block */
    if (remaining >= HEAP_MIN_BLOCK) {
        heap_block_t* new_block = (heap_block_t*)((virtual_addr_t)block + needed_size);

        new_block->size = remaining;
        new_block->used = false;
        new_block->magic = HEAP_BLOCK_MAGIC;
        new_block->prev = NULL;
        new_block->next = NULL;

        /* Update original block size */
        block->size = needed_size;

        /* Insert new block to free list */
        insert_to_free_list(new_block);

        /* Update stats */
        s_heap.stats.total_blocks++;
        s_heap.stats.free_blocks++;
        s_heap.stats.free_bytes += remaining;

        klog_trace("[HEAP] Split block: 0x%llX -> %lu + %lu\n",
                   (virtual_addr_t)block, needed_size, remaining);
    }
}
```

分割的逻辑是这样的：首先计算剩余大小。如果剩余大小至少是 `HEAP_MIN_BLOCK`（32 字节），我们就分割。否则，直接把整个块给用户，虽然有点浪费，但避免了产生一个无法使用的小碎片。

分割时，我们在原块的后面创建一个新块。新块的地址是原块地址加上需要的部分大小。我们设置新块的各种字段：大小是剩余大小，使用状态是空闲，魔数设置为 `HEAP_BLOCK_MAGIC`，指针初始化为空。然后我们更新原块的大小为需要的部分大小。最后我们把新块插入空闲链表，更新统计信息。

注意调用 `split_block` 之前，块应该已经从空闲链表中移除了。因为分割后的新块会被插入空闲链表，如果原块还在链表中，会导致链表混乱。

---

## 第四步：实现块验证函数

在操作块之前，我们需要验证块的完整性。这能帮助我们快速发现内存损坏、野指针等问题。

在 `heap.c` 中添加：

```c
/**
 * validate_block - Validate block integrity
 *
 * Checks magic number, address range, alignment, and size.
 * @return true if block is valid, false otherwise
 */
static bool validate_block(heap_block_t* block) {
    if (block == NULL) {
        return false;
    }

    /* Check magic number */
    if (block->magic != HEAP_BLOCK_MAGIC) {
        klog_error("[HEAP] Invalid magic at 0x%llX: 0x%X\n",
                   (virtual_addr_t)block, block->magic);
        return false;
    }

    /* Check address range */
    if ((virtual_addr_t)block < s_heap.heap_start ||
        (virtual_addr_t)block >= s_heap.heap_end) {
        klog_error("[HEAP] Block 0x%llX out of heap range\n",
                   (virtual_addr_t)block);
        return false;
    }

    /* Check alignment */
    if (((virtual_addr_t)block & (HEAP_ALIGN - 1)) != 0) {
        klog_error("[HEAP] Block 0x%llX not aligned\n", (virtual_addr_t)block);
        return false;
    }

    /* Check size */
    if (block->size < sizeof(heap_block_t) ||
        (virtual_addr_t)block + block->size > s_heap.heap_end) {
        klog_error("[HEAP] Invalid block size: %lu\n", block->size);
        return false;
    }

    return true;
}
```

`validate_block` 做了四项检查。第一，检查魔数是否正确。如果魔数不对，说明块头部被破坏了，可能是缓冲区溢出或者野指针写操作导致的。第二，检查块的地址是否在堆的合法范围内。如果不在，说明这是个非法指针。第三，检查块是否对齐到 HEAP_ALIGN 边界。如果不对齐，说明指针计算有问题。第四，检查块的大小是否合理。如果太小，连头部都装不下，或者加上大小后超出堆区域，说明数据被破坏了。

这些检查能帮我们在问题发生时就检测到，而不是等到程序崩溃时一头雾水。虽然这些检查有一些性能开销，但在内核环境中，正确性比性能更重要。

---

## 第五步：实现 kmalloc 函数

现在我们可以实现 `kmalloc` 函数了。这是堆分配器的核心函数，负责分配内存。

在 `heap.c` 中添加：

```c
/**
 * kmalloc - Allocate memory from kernel heap
 *
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void* kmalloc(size_t size) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&s_heap_lock, &flags);

    if (!s_heap.initialized) {
        klog_error("[HEAP] Not initialized\n");
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return NULL;
    }

    if (size == 0) {
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return NULL;
    }

    /* Calculate total size needed (including header and alignment) */
    size_t total_size = total_block_size(size);

    /* Find best fit block */
    heap_block_t* block = find_best_fit(total_size);

    if (block == NULL) {
        /* Need to expand heap */
        klog_trace("[HEAP] No free block found, expanding heap\n");

        heap_result_t result = expand_heap_locked(total_size);
        if (result != HEAP_OK) {
            klog_error("[HEAP] Out of memory\n");
            spin_unlock_irqrestore(&s_heap_lock, flags);
            return NULL;
        }

        /* Try again after expansion */
        block = find_best_fit(total_size);
        if (block == NULL) {
            klog_error("[HEAP] Still no free block after expansion\n");
            spin_unlock_irqrestore(&s_heap_lock, flags);
            return NULL;
        }
    }

    /* Remove from free list */
    remove_from_free_list(block);

    /* Split if necessary */
    split_block(block, total_size);

    /* Mark as used */
    block->used = true;

    /* Update stats */
    s_heap.stats.used_bytes += block->size;
    s_heap.stats.free_bytes -= block->size;
    s_heap.stats.used_blocks++;
    s_heap.stats.free_blocks--;
    s_heap.stats.alloc_count++;

    void* result = block_to_ptr(block);

    spin_unlock_irqrestore(&s_heap_lock, flags);

    klog_trace("[HEAP] Allocated %lu bytes at 0x%llX (block size: %lu)\n",
               size, (virtual_addr_t)result, block->size);

    return result;
}
```

`kmalloc` 的逻辑分为几个步骤。首先，我们获取自旋锁，保护堆的全局状态。然后检查堆是否已初始化，如果没初始化就报错返回。接着检查请求的大小是否为 0，如果是就直接返回 NULL。

然后我们计算需要的总大小，包括头部和对齐。接下来调用 `find_best_fit` 搜索合适的空闲块。如果没找到，说明堆的空闲内存不够，我们需要扩展堆。`expand_heap_locked` 函数会向 VMM 申请更多的虚拟页，然后在堆的末尾创建新的空闲块。扩展后我们再次搜索，这次应该能找到了。

找到块后，我们从空闲链表移除它，然后调用 `split_block` 检查是否需要分割。分割后把块标记为已使用，更新统计信息，最后把用户数据指针返回给调用者。

这里有个重要的细节：我们在整个函数执行期间都持有自旋锁。这意味着在多核环境下，其他核心想要分配或释放内存必须等待。自旋锁在短时间等待时效率很高，但如果等待时间长会浪费 CPU 时间。对于内核的内存分配场景，分配操作通常很快（搜索空闲链表、更新几个字段），所以自旋锁是合适的选择。

---

## 第六步：测试 kmalloc

现在我们来测试一下 `kmalloc` 是否能正常工作。在 `kernel_main.c` 中添加测试代码：

```c
#include "mm/heap/heap.h"
#include "klogs/kprintf.h"

void test_kmalloc_basic(void) {
    klog_info("=== kmalloc 基础测试 ===\n");

    /* 测试小分配 */
    void* ptr1 = kmalloc(16);
    if (ptr1 != NULL) {
        klog_info("Allocated 16 bytes at 0x%llX\n", (virtual_addr_t)ptr1);
    } else {
        klog_error("Failed to allocate 16 bytes\n");
    }

    /* 测试中等分配 */
    void* ptr2 = kmalloc(256);
    if (ptr2 != NULL) {
        klog_info("Allocated 256 bytes at 0x%llX\n", (virtual_addr_t)ptr2);
    } else {
        klog_error("Failed to allocate 256 bytes\n");
    }

    /* 测试大分配 */
    void* ptr3 = kmalloc(4096);
    if (ptr3 != NULL) {
        klog_info("Allocated 4096 bytes at 0x%llX\n", (virtual_addr_t)ptr3);
    } else {
        klog_error("Failed to allocate 4096 bytes\n");
    }

    /* 测试零分配 */
    void* ptr4 = kmalloc(0);
    if (ptr4 == NULL) {
        klog_info("Zero allocation correctly returned NULL\n");
    }

    klog_info("kmalloc 基础测试完成\n");
}
```

在 `kernel_main` 函数中调用这个测试（在 `heap_init` 之后）：

```c
void kernel_main(void) {
    /* ... 其他初始化代码 ... */

    /* 初始化堆 */
    heap_init();

    /* 测试 kmalloc */
    test_kmalloc_basic();

    /* ... 其他代码 ... */
}
```

编译并运行：

```bash
cd /home/charliechen/CCOperatingSystemX64
cmake --build build
qemu-system-x86_64 -kernel build/kernel.bin -serial stdio
```

你应该能看到类似这样的输出：

```
[HEAP] Heap initialized:
[HEAP]   Region:  0xFFFFFFFF81000000 - 0xFFFFFFFF89000000
[HEAP]   Break:   0xFFFFFFFF81010000
[HEAP]   Size:   64 KB
[INFO ] === kmalloc 基础测试 ===
[TRACE] Allocated 16 bytes at 0xFFFFFFFF81000020 (block size: 48)
[INFO ] Allocated 16 bytes at 0xFFFFFFFF81000020
[TRACE] Allocated 256 bytes at 0xFFFFFFFF81000050 (block size: 288)
[INFO ] Allocated 256 bytes at 0xFFFFFFFF81000050
...
```

如果看到这样的输出，说明 `kmalloc` 工作正常！分配的地址是在堆区域内的（以 `0xFFFFFFFF8100` 开头），块大小是请求大小向上对齐后加上头部大小。

---

## 到这里我们完成了什么

这篇文章我们实现了 `kmalloc` 的核心分配逻辑。我们实现了空闲链表的插入和移除操作，实现了 Best-Fit 搜索算法，实现了块分割函数，实现了块验证函数，最后把这些组合起来实现了完整的 `kmalloc` 函数。我们还编写了测试代码，验证了 `kmalloc` 能正确分配内存。

`kmalloc` 是堆分配器最复杂的部分。它涉及空闲链表管理、搜索算法、块分割、统计更新等多个方面。理解了 `kmalloc`，`kfree` 就相对简单了，因为释放的逻辑更直观。

---

## 接下来

在下一篇文章中，我们会实现 `kfree` 释放逻辑。这包括验证魔数、检测双重释放、标记块为空闲、插入空闲链表、块合并等操作。虽然逻辑相对简单，但细节不少，特别是块合并部分需要仔细处理。

准备好了吗？让我们继续。

---

<div align="center">

## 文档导航

[实现核心数据结构](./03_实现核心数据结构.md) | [实现kfree释放与块合并 →](./05_实现kfree释放与块合并.md)

</div>
