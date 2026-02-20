# 实现堆扩展与 VMM 集成

在之前的文章中，我们实现了 `kmalloc` 和 `kfree` 的核心逻辑。但有一个重要的功能还没实现 —— 堆扩展。当堆的空闲内存不够用时，我们需要向 VMM 申请更多的虚拟页，动态扩展堆的大小。这篇文章我们来实现这个功能，完成堆分配器与 VMM 的集成。

---

## 环境说明

这篇文章假设你已经完成了前面的步骤，`kmalloc` 和 `kfree` 的基本逻辑已经实现。我们会在 `heap.c` 中继续添加代码。

```
前置依赖: Stage 16 VMM 已完成并能正常工作
关键常量: KERNEL_HEAP_BASE = 0xFFFFFFFF81000000
          KERNEL_HEAP_MAX = 0xFFFFFFFF89000000 (128MB)
          PAGE_SIZE = 4096
```

---

## 第一步：理解堆扩展的需求

为什么需要堆扩展？想象这样一个场景：你的程序分配了很多小对象，堆的初始 64KB 用完了，但你又不需要那么多大对象。如果堆不能扩展，你就无法继续分配内存了。

堆扩展的原理是：当 `kmalloc` 找不到合适的空闲块时，我们向 VMM 申请更多的虚拟页，在堆的末尾创建一个新的空闲块，然后把这个新块加入空闲链表。下次 `kmalloc` 就能从这个新块中分配内存了。

这里有个关键点：堆扩展是单向的。堆只能向上增长，不能向下收缩。这是因为收缩需要把已分配的块移到更低的地址，这涉及到指针更新，复杂度很高。而且内核场景下，内存通常是只增不减的，所以堆不收缩是可以接受的。

---

## 第二步：实现堆扩展函数

现在我们来实现堆扩展函数。这个函数会被 `kmalloc` 在找不到空闲块时调用。

在 `heap.c` 中添加：

```c
/**
 * expand_heap_locked - Expand heap by allocating more pages
 *
 * IMPORTANT: Caller must hold s_heap_lock when calling this function!
 *
 * @param min_needed Minimum bytes needed
 * @return HEAP_OK on success, error code on failure
 */
static heap_result_t expand_heap_locked(size_t min_needed) {
    /* Calculate pages needed */
    size_t page_count = (min_needed + PAGE_SIZE - 1) / PAGE_SIZE;

    /* Allocate at least 4 pages (16KB) to avoid frequent expansion */
    if (page_count < 4) {
        page_count = 4;
    }

    /* Check if we would exceed the heap maximum */
    virtual_addr_t new_brk = s_heap.heap_brk + page_count * PAGE_SIZE;
    if (s_heap.heap_brk != 0 && new_brk > s_heap.heap_end) {
        klog_error("[HEAP] Cannot expand: would exceed maximum\n");
        return HEAP_ERR_OOM;
    }

    /* Allocate virtual pages at current heap break */
    virtual_addr_t target_vaddr = s_heap.heap_brk != 0 ?
                                  s_heap.heap_brk : KERNEL_HEAP_BASE;

    vmm_result_t vmm_result = vmm_alloc_pages_at(target_vaddr, page_count,
                                                  VMAP_FLAG_WRITE);
    if (vmm_result != VMM_OK) {
        klog_error("[HEAP] VMM allocation failed: %d\n", vmm_result);
        return HEAP_ERR_OOM;
    }

    /* Initialize as a new free block */
    heap_block_t* new_block = (heap_block_t*)target_vaddr;
    new_block->size = page_count * PAGE_SIZE;
    new_block->used = false;
    new_block->magic = HEAP_BLOCK_MAGIC;
    new_block->prev = NULL;
    new_block->next = NULL;

    /* Clear the memory (except header) */
    memset((void*)(target_vaddr + sizeof(heap_block_t)), 0,
           new_block->size - sizeof(heap_block_t));

    /* Insert to free list */
    insert_to_free_list(new_block);

    /* Update heap break */
    s_heap.heap_brk = target_vaddr + new_block->size;

    /* Update stats */
    s_heap.stats.total_bytes += new_block->size;
    s_heap.stats.free_bytes += new_block->size;
    s_heap.stats.total_blocks++;
    s_heap.stats.free_blocks++;
    s_heap.stats.expand_count++;

    klog_info("[HEAP] Expanded by %lu pages to 0x%llX\n",
              page_count, s_heap.heap_brk);

    /* Try to coalesce with the last block if adjacent */
    coalesce_block(new_block);

    return HEAP_OK;
}
```

`expand_heap_locked` 函数的逻辑分为几个步骤。首先计算需要的页数，向上取整到页边界。我们有个最小扩展值：即使只需要 1 页，我们也至少扩展 4 页（16KB）。这是为了避免频繁扩展，每次扩展都有开销，不如一次多扩展一点。

然后检查扩展后是否会超过堆的最大边界。`KERNEL_HEAP_MAX` 是堆的硬性限制，超过这个地址就不能再扩展了。如果会超过，返回内存不足错误。

接下来调用 `vmm_alloc_pages_at` 在当前堆顶地址分配虚拟页。这个函数是 VMM 提供的接口，可以在指定虚拟地址分配页，并自动关联物理页。我们传入 `VMAP_FLAG_WRITE` 标志，表示这块内存需要可写权限。

分配成功后，我们把新内存初始化为一个空闲块。设置块的大小为分配的页数乘以页大小，使用状态为空闲，魔数设置为 `HEAP_BLOCK_MAGIC`。然后我们清零用户数据部分，确保新分配的内存是干净的。这点很重要，如果不清零，用户可能会读到之前的垃圾数据。

然后把新块插入空闲链表，更新堆顶指针和统计信息。最后调用 `coalesce_block` 尝试与最后一个块合并。如果扩展前的最后一个块是空闲的，它们会合并成一个大块。

注意这个函数的调用约定：调用者必须持有 `s_heap_lock`。函数名中的 `_locked` 后缀就是提醒这一点。这是因为函数内部会修改堆的全局状态，如果在多核环境下不加锁保护，会有竞态条件。

---

## 第三步：实现 heap_init 函数

有了堆扩展函数，我们就可以实现堆的初始化了。初始化主要是设置堆的边界，然后分配初始的内存。

在 `heap.c` 中添加：

```c
/**
 * heap_init - Initialize the kernel heap allocator
 *
 * Sets up the heap region and allocates initial pages.
 * Must be called after vmm_init().
 *
 * @return HEAP_OK on success, error code on failure
 */
heap_result_t heap_init(void) {
    if (s_heap.initialized) {
        klog_warn("[HEAP] Already initialized\n");
        return HEAP_OK;
    }

    klog_info("[HEAP] Initializing Kernel Heap Allocator...\n");

    /* Clear state */
    memset(&s_heap, 0, sizeof(s_heap));

    /* Set up heap region boundaries */
    s_heap.heap_start = KERNEL_HEAP_BASE;
    s_heap.heap_end = KERNEL_HEAP_MAX;
    s_heap.heap_brk = 0;  /* Will be set by expand_heap_locked */
    s_heap.free_list = NULL;

    /* Allocate initial pages (64KB = 16 pages)
     * Note: No need to acquire lock here since we're in single-threaded init */
    heap_result_t result = expand_heap_locked(HEAP_INIT_PAGES * PAGE_SIZE);
    if (result != HEAP_OK) {
        klog_error("[HEAP] Initial heap expansion failed\n");
        return result;
    }

    /* Update heap_start to match actual allocated address */
    s_heap.heap_start = (virtual_addr_t)s_heap.free_list;

    s_heap.initialized = true;

    klog_info("[HEAP] Heap initialized:\n");
    klog_info("[HEAP]   Region:  0x%llX - 0x%llX\n",
              s_heap.heap_start, s_heap.heap_end);
    klog_info("[HEAP]   Break:   0x%llX\n", s_heap.heap_brk);
    klog_info("[HEAP]   Size:   %lu KB\n",
              (s_heap.heap_brk - s_heap.heap_start) / 1024);

    return HEAP_OK;
}
```

`heap_init` 首先检查是否已经初始化，如果已经初始化就直接返回成功。这是个常见的防御性编程做法，允许多次调用初始化函数不会出错。

然后我们清零堆的状态结构，设置堆的起始地址、结束地址。起始地址是 `KERNEL_HEAP_BASE`（0xFFFFFFFF81000000），结束地址是 `KERNEL_HEAP_MAX`（0xFFFFFFFF89000000），总共 128MB 的虚拟地址空间。堆顶指针初始设为 0，表示还没有分配任何内存。

接下来调用 `expand_heap_locked` 分配初始的 16 页（64KB）。这个调用会创建第一个空闲块，设置堆顶指针，把块加入空闲链表。分配成功后，我们更新堆起始地址为实际分配的地址（虽然理论上应该等于 `KERNEL_HEAP_BASE`，但这样写更健壮）。

最后设置初始化标志，打印一些统计信息，返回成功。

注意在初始化阶段，我们还没有启用多核，所以不需要获取自旋锁。但 `expand_heap_locked` 函数名中有 `_locked` 后缀，可能会让人困惑。在实际的多核初始化代码中，会有一个临界区保护这段代码，确保不会有其他核心同时访问堆。

---

## 第四步：更新 kmalloc 使用堆扩展

现在我们需要更新 `kmalloc` 函数，让它在找不到空闲块时调用堆扩展函数。

之前 `kmalloc` 中有这样的代码：

```c
if (block == NULL) {
    /* Need to expand heap */
    heap_result_t result = expand_heap_locked(total_size);
    /* ... */
}
```

这段代码已经在我们之前实现 `kmalloc` 时加上了，所以现在应该能正常工作。但我们需要确认一下，确保 `kmalloc` 在调用 `expand_heap_locked` 时持有锁。

回顾一下 `kmalloc` 的开头：

```c
void* kmalloc(size_t size) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&s_heap_lock, &flags);

    /* ... */

    if (block == NULL) {
        heap_result_t result = expand_heap_locked(total_size);
        /* ... */
    }

    /* ... */

    spin_unlock_irqrestore(&s_heap_lock, flags);
}
```

很好，`kmalloc` 在整个执行期间都持有锁，所以调用 `expand_heap_locked` 时锁已经被持有，符合函数的调用约定。

---

## 第五步：测试堆扩展

让我们写一个测试来验证堆扩展是否正常工作。这个测试会分配大量内存，迫使堆进行扩展。

```c
void test_heap_expansion(void) {
    klog_info("=== 堆扩展测试 ===\n");

    /* 获取初始统计信息 */
    heap_stats_t stats_before, stats_after;
    heap_get_stats(&stats_before);
    klog_info("Before: %llu total bytes, %llu expand count\n",
              stats_before.total_bytes, stats_before.expand_count);

    /* 分配超过初始堆大小的内存，迫使堆扩展 */
    void* ptrs[20];
    for (int i = 0; i < 20; i++) {
        /* 每次分配 4KB，总共 80KB，超过初始的 64KB */
        ptrs[i] = kmalloc(4096);
        if (ptrs[i] == NULL) {
            klog_error("Failed to allocate ptrs[%d]\n", i);
            break;
        }
        klog_trace("Allocated ptrs[%d] = 0x%llX\n", i,
                   (virtual_addr_t)ptrs[i]);
    }

    /* 检查统计信息 */
    heap_get_stats(&stats_after);
    klog_info("After:  %llu total bytes, %llu expand count\n",
              stats_after.total_bytes, stats_after.expand_count);

    if (stats_after.expand_count > stats_before.expand_count) {
        klog_info("Heap expanded successfully!\n");
    }

    /* 释放所有内存 */
    for (int i = 0; i < 20; i++) {
        if (ptrs[i] != NULL) {
            kfree(ptrs[i]);
        }
    }

    klog_info("堆扩展测试完成\n");
}
```

这个测试分配 20 个 4KB 的块，总共 80KB，超过初始堆的 64KB。这样至少会触发一次堆扩展。我们检查扩展次数是否增加，如果增加了，说明堆扩展正常工作。

在 `kernel_main` 函数中调用这个测试：

```c
void kernel_main(void) {
    /* ... 其他初始化代码 ... */

    heap_init();

    /* 测试堆扩展 */
    test_heap_expansion();

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
[INFO ] === 堆扩展测试 ===
[INFO ] Before: 65536 total bytes, 1 expand count
[TRACE] No free block found, expanding heap
[HEAP] Expanded by 4 pages to 0xFFFFFFFF81020000
...
[INFO ] After:  131072 total bytes, 2 expand count
[INFO ] Heap expanded successfully!
[INFO ] 堆扩展测试完成
```

注意扩展次数从 1 增加到 2，堆大小从 64KB 增加到 128KB。这说明堆扩展正常工作了。

---

## 第六步：内核初始化顺序

到这里，堆分配器的核心功能已经完成了。让我们来确认一下内核的初始化顺序，确保堆分配器在正确的时机被初始化。

在 `kernel_init.c` 中，初始化顺序应该是这样的：

```c
void kernel_init(void) {
    /* 1. 物理帧分配器 */
    pframe_init();

    /* 2. 页表管理 */
    page_init();

    /* 3. 虚拟内存管理器 */
    vmm_init();

    /* 4. 堆分配器 */
    heap_init();

    /* ... 其他初始化 ... */
}
```

这个顺序很重要。堆分配器依赖于 VMM，所以必须在 VMM 之后初始化。VMM 依赖于页表和物理帧分配器，所以它们要在 VMM 之前初始化。如果顺序错了，系统可能无法正常启动。

---

## 到这里我们完成了什么

这篇文章我们实现了堆扩展机制和 VMM 集成。我们实现了 `expand_heap_locked` 函数，它可以在堆的末尾分配新的虚拟页，创建新的空闲块。我们实现了 `heap_init` 函数，设置堆的边界，分配初始内存。我们还确认了 `kmalloc` 在找不到空闲块时会调用堆扩展函数，更新了内核的初始化顺序。

到这一步，堆分配器的核心功能已经全部实现了。我们可以分配任意大小的内存，可以在没有空闲块时自动扩展堆，可以释放内存并合并相邻块。这是一个完整的、可用的堆分配器。

---

## 接下来

在下一篇文章中，我们会实现一些高级功能，比如 `krealloc`（重新分配内存大小）和 `kmalloc_aligned`（对齐分配）。这些功能虽然不是必需的，但在实际使用中非常方便。特别是对齐分配，对于 DMA 操作和 SIMD 指令是必须的。

准备好了吗？让我们继续。

---

<div align="center">

## 文档导航

[实现kfree释放与块合并](./05_实现kfree释放与块合并.md) | [实现高级功能 →](./07_实现高级功能.md)

</div>
