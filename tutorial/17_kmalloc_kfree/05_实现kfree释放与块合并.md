# 实现 kfree 释放与块合并

在上一篇文章中，我们实现了 `kmalloc` 的分配逻辑。现在让我们来实现它的另一半 —— `kfree` 释放逻辑。虽然释放看起来比分配简单，但细节一点也不少，特别是块合并部分需要仔细处理。

---

## 环境说明

这篇文章假设你已经完成了前面的步骤，`kmalloc` 已经能正常工作。我们会在 `heap.c` 中继续添加代码。

---

## 第一步：理解释放流程

在开始写代码之前，我们先理解一下 `kfree` 的完整流程。这有助于我们避免遗漏重要的步骤。

当用户调用 `kfree(ptr)` 时，我们需要做以下几件事：

1. 检查 ptr 是否为 NULL，如果是就直接返回（释放 NULL 是安全的）
2. 把用户数据指针转换成块指针（减去头部大小）
3. 验证块的完整性（魔数、地址范围、对齐、大小）
4. 检查块是否已经是空闲的（双重释放检测）
5. 把块标记为空闲
6. 把块插入空闲链表
7. 尝试与前后邻居合并（如果它们也是空闲的）
8. 更新统计信息

这个流程看起来很直接，但每一步都有细节需要注意。让我们一步步实现。

---

## 第二步：实现块合并函数

块合并是 `kfree` 最关键的部分。当我们释放一个块时，如果它的前后邻居也是空闲的，我们应该把它们合并成一个大块。这能减少外部碎片，保留更大的块给大分配请求。

在 `heap.c` 中添加：

```c
/**
 * coalesce_block - Merge a free block with adjacent free blocks
 *
 * Tries to merge with both the next and previous blocks if they are free.
 * @param block The block to merge (must be free and in free list)
 */
static void coalesce_block(heap_block_t* block) {
    CCOS_ASSERT(!block->used);

    /* Try to merge with next block */
    if (!is_last_block(block)) {
        heap_block_t* next = next_block(block);
        if (!next->used && validate_block(next)) {
            klog_trace("[HEAP] Coalescing with next block: 0x%llX + 0x%llX\n",
                       (virtual_addr_t)block, (virtual_addr_t)next);

            /* Remove next from free list */
            remove_from_free_list(next);

            /* Merge sizes */
            block->size += next->size;

            /* Update stats */
            s_heap.stats.total_blocks--;
            s_heap.stats.free_blocks--;
        }
    }

    /* Try to merge with previous block */
    /* Find previous block in free list that is immediately before us */
    heap_block_t* prev_in_free = NULL;
    heap_block_t* curr = s_heap.free_list;
    while (curr && curr < block) {
        prev_in_free = curr;
        curr = curr->next;
    }

    if (prev_in_free) {
        /* Check if prev_in_free is immediately before block */
        if ((virtual_addr_t)prev_in_free + prev_in_free->size == (virtual_addr_t)block) {
            klog_trace("[HEAP] Coalescing with prev block: 0x%llX + 0x%llX\n",
                       (virtual_addr_t)prev_in_free, (virtual_addr_t)block);

            /* Merge sizes into prev */
            prev_in_free->size += block->size;

            /* Remove current block from free list (it's being merged) */
            remove_from_free_list(block);

            /* Update stats */
            s_heap.stats.total_blocks--;
            s_heap.stats.free_blocks--;
        }
    }
}
```

`coalesce_block` 函数尝试与前后两个邻居合并。我们先检查后邻居，这很简单，通过 `next_block` 函数可以计算出后邻居的地址。如果后邻居是空闲的，我们就合并。合并的逻辑是：把后邻居从空闲链表移除，把后邻居的大小加到当前块上，更新统计信息。

检查前邻居稍微复杂一点。我们需要找到物理内存中紧邻当前块的前一个块。但问题是，前一个块可能不是空闲的（如果是已使用的，我们看不到它的 `prev` 指针）。所以我们只在空闲链表中查找。我们遍历空闲链表，找到地址小于当前块的最大块，然后检查这个块是否真的紧邻当前块（通过地址计算验证）。如果是，就合并。合并时我们把大小加到前一个块上，然后把当前块从空闲链表移除。

注意合并的顺序。我们先合并后邻居，再合并前邻居。这样有个好处：如果前后都是空闲的，最终三个块会合并成一个大块，这个大块的地址是前一个块的地址。如果我们先合并前邻居，合并后当前块就不存在了（被合并到前一个块里），处理起来会更复杂。

---

## 第三步：实现 kfree 函数

现在我们可以实现 `kfree` 函数了。

在 `heap.c` 中添加：

```c
/**
 * kfree - Free memory allocated from kernel heap
 *
 * @param ptr Pointer to memory to free (NULL is safe)
 */
void kfree(void* ptr) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&s_heap_lock, &flags);

    if (!s_heap.initialized) {
        klog_error("[HEAP] Not initialized\n");
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return;
    }

    /* NULL is safe to free */
    if (ptr == NULL) {
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return;
    }

    /* Convert user pointer to block pointer */
    heap_block_t* block = ptr_to_block(ptr);

    /* Validate block integrity */
    if (!validate_block(block)) {
        klog_error("[HEAP] Invalid pointer or corrupted block: 0x%llX\n",
                   (virtual_addr_t)ptr);
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return;
    }

    /* Check for double free */
    if (!block->used) {
        klog_warn("[HEAP] Double free detected at 0x%llX\n",
                  (virtual_addr_t)ptr);
        spin_unlock_irqrestore(&s_heap_lock, flags);
        return;
    }

    /* Mark as free */
    block->used = false;

    /* Update stats before coalescing (coalescing will modify them) */
    s_heap.stats.used_bytes -= block->size;
    s_heap.stats.free_bytes += block->size;
    s_heap.stats.used_blocks--;
    s_heap.stats.free_blocks++;
    s_heap.stats.free_count++;

    /* Insert to free list */
    insert_to_free_list(block);

    /* Try to coalesce with adjacent blocks */
    coalesce_block(block);

    spin_unlock_irqrestore(&s_heap_lock, flags);

    klog_trace("[HEAP] Freed %lu bytes at 0x%llX\n",
               block->size - sizeof(heap_block_t), (virtual_addr_t)ptr);
}
```

`kfree` 的逻辑很直接。首先获取自旋锁，检查堆是否已初始化，然后检查 ptr 是否为 NULL。如果是 NULL，直接返回，不做任何操作。这是标准 C 库的约定，`free(NULL)` 是安全的。

然后我们把用户数据指针转换成块指针，调用 `validate_block` 验证块的完整性。如果验证失败，说明这是个非法指针或者块被破坏了，我们打印错误信息然后返回。

接下来检查双重释放。如果块的 `used` 字段已经是 false，说明这个块已经被释放过了，可能是双重释放。我们打印警告信息然后返回。

检查通过后，我们把块标记为空闲，更新统计信息，然后把块插入空闲链表。最后调用 `coalesce_block` 尝试与前后邻居合并。

这里有个细节：统计信息的更新在合并之前。这是因为合并会修改块数量统计，如果在合并之后更新，逻辑会更复杂。我们在合并前更新，合并函数会相应地调整统计。

---

## 第四步：测试 kfree

现在我们来测试一下 `kfree` 是否能正常工作。在 `kernel_main.c` 中添加测试代码：

```c
void test_kfree_basic(void) {
    klog_info("=== kfree 基础测试 ===\n");

    /* 测试基本的分配和释放 */
    void* ptr1 = kmalloc(100);
    if (ptr1 != NULL) {
        klog_info("Allocated 100 bytes at 0x%llX\n", (virtual_addr_t)ptr1);
        kfree(ptr1);
        klog_info("Freed ptr1\n");
    }

    /* 测试多次分配和释放 */
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = kmalloc(64);
        if (ptrs[i] != NULL) {
            klog_trace("Allocated ptrs[%d] = 0x%llX\n", i,
                       (virtual_addr_t)ptrs[i]);
        }
    }

    for (int i = 0; i < 10; i++) {
        if (ptrs[i] != NULL) {
            kfree(ptrs[i]);
            klog_trace("Freed ptrs[%d]\n", i);
        }
    }

    /* 测试 NULL 释放 */
    kfree(NULL);
    klog_info("NULL free is safe\n");

    /* 测试双重释放检测 */
    void* ptr2 = kmalloc(50);
    if (ptr2 != NULL) {
        klog_info("Allocated 50 bytes at 0x%llX\n", (virtual_addr_t)ptr2);
        kfree(ptr2);
        klog_info("Freed ptr2 once\n");
        kfree(ptr2);  /* 应该检测到双重释放 */
    }

    klog_info("kfree 基础测试完成\n");
}
```

在 `kernel_main` 函数中调用这个测试：

```c
void kernel_main(void) {
    /* ... 其他初始化代码 ... */

    heap_init();

    /* 测试 kmalloc 和 kfree */
    test_kmalloc_basic();
    test_kfree_basic();

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
[INFO ] === kfree 基础测试 ===
[INFO ] Allocated 100 bytes at 0xFFFFFFFF81000020
[TRACE] Freed 112 bytes at 0xFFFFFFFF81000020
[INFO ] Freed ptr1
[TRACE] Allocated ptrs[0] = 0xFFFFFFFF81000020
[TRACE] Allocated ptrs[1] = 0xFFFFFFFF81000070
...
[TRACE] Freed ptrs[0]
[TRACE] Coalescing with next block: ...
[INFO ] NULL free is safe
[INFO ] Allocated 50 bytes at 0xFFFFFFFF81000020
[TRACE] Freed 64 bytes at 0xFFFFFFFF81000020
[WARN ] Double free detected at 0xFFFFFFFF81000020
[INFO ] kfree 基础测试完成
```

注意这里有几处关键的输出。首先是双重释放检测，当你第二次释放同一个指针时，会看到警告信息。其次是块合并，当你释放多个相邻的块时，应该能看到合并的日志信息。

---

## 第五步：测试块合并

让我们写一个更明确的测试来验证块合并是否正常工作：

```c
void test_coalescing(void) {
    klog_info("=== 块合并测试 ===\n");

    /* 获取初始统计信息 */
    heap_stats_t stats_before, stats_after;
    heap_get_stats(&stats_before);
    klog_info("Before: %llu free blocks, %llu free bytes\n",
              stats_before.free_blocks, stats_before.free_bytes);

    /* 分配三个块 */
    void* ptr1 = kmalloc(100);
    void* ptr2 = kmalloc(100);
    void* ptr3 = kmalloc(100);

    klog_info("Allocated three blocks\n");

    /* 释放中间的块 */
    kfree(ptr2);
    heap_get_stats(&stats_after);
    klog_info("After freeing middle: %llu free blocks\n", stats_after.free_blocks);

    /* 释放第一个块（应该与中间块合并） */
    kfree(ptr1);
    heap_get_stats(&stats_after);
    klog_info("After freeing first: %llu free blocks\n", stats_after.free_blocks);

    /* 释放第三个块（应该与合并后的大块合并） */
    kfree(ptr3);
    heap_get_stats(&stats_after);
    klog_info("After freeing third: %llu free blocks, %llu free bytes\n",
              stats_after.free_blocks, stats_after.free_bytes);

    klog_info("块合并测试完成\n");
}
```

这个测试分配三个相邻的块，然后按中间、第一个、第三个的顺序释放。如果块合并正常工作，释放第一个块时应该与中间块合并，释放第三个块时应该与合并后的大块再次合并，最终三个块合并成一个。

运行这个测试，你应该能看到空闲块的数量变化。理想情况下，释放三个块后，空闲块数量应该只增加了 1（因为三个块合并成了一个），而不是 3。

---

## 到这里我们完成了什么

这篇文章我们实现了 `kfree` 的释放逻辑。我们实现了块合并函数，它可以与前后邻居的空闲块合并。我们实现了完整的 `kfree` 函数，包括参数检查、指针转换、块验证、双重释放检测、标记空闲、插入链表、块合并等步骤。我们还编写了测试代码，验证了 `kfree` 能正确释放内存，并且块合并能正常工作。

`kfree` 虽然逻辑比 `kmalloc` 简单，但细节一点也不少。块合并是减少外部碎片的关键，理解了块合并，你就理解了堆分配器如何保持内存的高效利用。

---

## 接下来

在下一篇文章中，我们会实现堆扩展机制和 VMM 集成。当堆的空闲内存不够用时，我们需要向 VMM 申请更多的虚拟页，动态扩展堆的大小。我们还会实现 `heap_init` 初始化流程，设置堆的初始状态。这会让堆分配器成为一个完整的、可以动态增长的内存管理系统。

准备好了吗？让我们继续。

---

<div align="center">

## 文档导航

[实现kmalloc核心分配逻辑](./04_实现kmalloc核心分配逻辑.md) | [实现堆扩展与VMM集成 →](./06_实现堆扩展与VMM集成.md)

</div>
