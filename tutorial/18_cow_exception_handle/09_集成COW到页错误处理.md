# 09 - 集成 COW 到页错误处理

说实话，当 COW 页错误第一次被正确处理时，我盯着屏幕看了好久——这么复杂的一套机制，居然真的能跑起来。

---

## 集成概述

现在我们已经有了：
1. **COW 模块**：管理 COW 页和引用计数
2. **异常处理器**：处理 Double Fault、Stack Fault、GPF
3. **页错误处理器**：处理 #PF 异常

本节我们将把 COW 模块集成到页错误处理器中，让写入 COW 页时能够自动触发复制。

---

## 修改 kernel/mm/vmm/fault.c

在 Stage 16 中，我们实现了基础的页错误处理器。现在需要扩展它以支持 COW。

### 在头文件中添加 COW 支持

```c
/* In fault.h */

#include "mm/vmm/cow.h"

/**
 * @brief Page fault handler with COW support
 */
void pf_handler(interrupt_frame_t* frame, uint64_t error_code);
```

### 在页错误处理器中集成 COW

```c
/* In fault.c */

void pf_handler(interrupt_frame_t* frame, uint64_t error_code) {
    s_stats.pf_count++;

    /* Get fault address from CR2 */
    virtual_addr_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));

    /* Get current PML4 */
    physical_addr_t pml4 = vmm_get_current_pml4();

    /* Check if this is a write fault */
    bool is_write = (error_code & PF_ERROR_WRITE) != 0;

    if (is_write) {
        /* Check if this is a COW page */
        page_query_result_t query;
        if (page_query(pml4, fault_addr, &query) == PAGE_OK) {
            if (cow_is_cow_page(query.flags)) {
                /* This is a COW write fault */
                klog_debug("[PF] COW write fault at 0x%llX\n", fault_addr);

                cow_result_t result = cow_handle_fault(pml4, fault_addr);

                if (result == COW_OK) {
                    /* COW handled successfully, can continue */
                    return;
                } else {
                    klog_error("[PF] COW handling failed: %d\n", result);
                    /* Fall through to normal handling */
                }
            }
        }
    }

    /* ... rest of existing page fault handling ... */
}
```

⚠️ 注意：COW 处理必须在其他检查之前进行，因为 COW 页是故意设置为只读的。

---

## COW 页错误处理流程

让我们详细看看 COW 页错误的完整处理流程：

```
1. 进程尝试写入 COW 页
    ↓
2. CPU 检测到写权限不足
    ↓
3. 触发 #PF (错误码包含 PF_ERROR_WRITE)
    ↓
4. pf_handler 被调用
    ↓
5. 从 CR2 获取故障地址
    ↓
6. 查询页表获取当前映射
    ↓
7. 检查页表项的 COW 标志
    ↓
8. 如果是 COW 页，调用 cow_handle_fault
    ↓
9. cow_handle_fault 查找 COW 块
    ↓
10. 检查引用计数
    ↓
    refcount = 1?         refcount > 1?
    ↓                     ↓
11a. 直接设置写权限      11b. 分配新页 + 复制
    ↓                     ↓
12. 清除 COW 标志         12b. 更新页表指向新页
    ↓                     ↓
13. 刷新 TLB              13b. 减少原页引用计数
    ↓                     ↓
14. 恢复执行              14b. 恢复执行
```

---

## cow_handle_fault 实现

让我们详细看看这个关键函数的实现：

```c
/* In cow.c */

cow_result_t cow_handle_fault(physical_addr_t pml4, virtual_addr_t fault_addr) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    s_cow_state.stats.cow_write_faults++;

    /* Step 1: Query the current mapping */
    page_query_result_t query;
    if (page_query(pml4, fault_addr, &query) != PAGE_OK) {
        klog_error("[COW] Page not mapped: 0x%llX\n", fault_addr);
        return COW_ERR_INVALID;
    }

    /* Step 2: Check if it's a COW page */
    if (!cow_is_cow_page(query.flags)) {
        klog_debug("[COW] Not a COW page\n");
        return COW_ERR_NOT_COW;
    }

    /* Step 3: Look up COW block */
    cow_block_t* block = find_block_by_phys(query.phys_addr);
    if (block == NULL) {
        klog_error("[COW] No COW block for phys: 0x%llX\n", query.phys_addr);
        return COW_ERR_INVALID;
    }

    /* Step 4: Handle based on refcount */
    uint16_t refcount = atomic_load(&block->refcount);

    klog_debug("[COW] Fault at 0x%llX, refcount=%u\n", fault_addr, refcount);

    if (refcount == 1) {
        /* Only we have this page, just make it writable */
        return cow_make_writable(pml4, fault_addr, query.phys_addr, block);
    } else {
        /* Multiple references, need to copy */
        return cow_copy_on_write(pml4, fault_addr, query.phys_addr, block);
    }
}
```

### cow_make_writable 实现

当引用计数为 1 时，只需要设置写权限：

```c
cow_result_t cow_make_writable(physical_addr_t pml4,
                               virtual_addr_t vaddr,
                               physical_addr_t phys,
                               cow_block_t* block) {
    /* Query to get current flags */
    page_query_result_t query;
    if (page_query(pml4, vaddr, &query) != PAGE_OK) {
        return COW_ERR_INVALID;
    }

    /* Clear COW flag and set write permission */
    uint64_t new_flags = cow_clear_cow_flag(query.flags);
    new_flags |= PAGE_FLAG_WRITE;

    /* Update the mapping */
    if (page_update_flags(pml4, vaddr, new_flags) != PAGE_OK) {
        return COW_ERR_INVALID;
    }

    /* Invalidate TLB for this page */
    page_invalidate_tlb_single(vaddr);

    /* Remove from COW tracking since we now own it exclusively */
    if (block) {
        atomic_fetch_sub(&block->refcount, 1);
        if (atomic_load(&block->refcount) == 0) {
            hashmap_remove(s_cow_state.page_map, block);
            kfree(block);
            s_cow_state.stats.cow_current_blocks--;
        }
    }

    s_cow_state.stats.cow_coalesced++;
    s_cow_state.stats.cow_faults_handled++;

    klog_debug("[COW] Made writable without copy (refcount was 1)\n");

    return COW_OK;
}
```

### cow_copy_on_write 实现

当引用计数大于 1 时，需要分配新页并复制：

```c
cow_result_t cow_copy_on_write(physical_addr_t pml4,
                               virtual_addr_t vaddr,
                               physical_addr_t orig_phys,
                               cow_block_t* block) {
    /* Allocate new physical page */
    physical_addr_t new_phys;
    if (pframe_alloc(1, &new_phys) != PF_OK) {
        klog_error("[COW] Failed to allocate physical page\n");
        return COW_ERR_OOM;
    }

    /* Map temporary addresses to copy content */
    virtual_addr_t temp_src = vmm_alloc_temporary(orig_phys, PAGE_SIZE);
    virtual_addr_t temp_dst = vmm_alloc_temporary(new_phys, PAGE_SIZE);

    if (temp_src == 0 || temp_dst == 0) {
        pframe_free(new_phys, 1);
        return COW_ERR_OOM;
    }

    /* Copy the page content */
    const uint64_t* src = (const uint64_t*)temp_src;
    uint64_t* dst = (uint64_t*)temp_dst;

    for (size_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++) {
        dst[i] = src[i];
    }

    /* Unmap temporary addresses */
    vmm_free_temporary(temp_src);
    vmm_free_temporary(temp_dst);

    /* Update page table to point to new page with write permission */
    uint64_t flags = PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE | PAGE_FLAG_USER;
    if (page_map_page(pml4, vaddr, new_phys, flags, false) != PAGE_OK) {
        pframe_free(new_phys, 1);
        return COW_ERR_INVALID;
    }

    /* Decrease refcount for original page */
    if (block) {
        uint16_t old_ref = atomic_fetch_sub(&block->refcount, 1);
        if (old_ref == 1) {
            /* Refcount became 0, remove from tracking */
            hashmap_remove(s_cow_state.page_map, block);
            kfree(block);
            s_cow_state.stats.cow_current_blocks--;
        }
    }

    /* Invalidate TLB for this page */
    page_invalidate_tlb_single(vaddr);

    s_cow_state.stats.cow_faults_handled++;

    klog_debug("[COW] Copied page: 0x%llX -> 0x%llX\n", orig_phys, new_phys);

    return COW_OK;
}
```

⚠️ 注意：上面的实现使用了临时映射函数 `vmm_alloc_temporary`。如果你的内核还没有这个功能，可以使用直接映射区域。

---

## COW 标志操作

页表项的 COW 标志操作是通过位运算完成的：

```c
/* In vmm_constants.h or similar */

#define COW_FLAG_MASK    (1ULL << 9)   /* Use bit 9 (available bit) */

/* In cow.c */

static inline bool cow_is_cow_page(uint64_t pte_flags) {
    return (pte_flags & COW_FLAG_MASK) != 0;
}

static inline uint64_t cow_set_cow_flag(uint64_t pte_flags) {
    return pte_flags | COW_FLAG_MASK;
}

static inline uint64_t cow_clear_cow_flag(uint64_t pte_flags) {
    return pte_flags & ~COW_FLAG_MASK;
}
```

### 注册 COW 区域

当 fork() 创建子进程时，需要把整个地址空间注册为 COW：

```c
cow_result_t cow_register_region(physical_addr_t pml4,
                                 virtual_addr_t base,
                                 size_t size) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    /* Align to page boundaries */
    base = base & ~(PAGE_SIZE - 1);
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    uint64_t page_count = size / PAGE_SIZE;

    klog_debug("[COW] Registering region: 0x%llX - 0x%llX (%llu pages)\n",
               base, base + size, page_count);

    /* Process each page in the region */
    for (uint64_t i = 0; i < page_count; i++) {
        virtual_addr_t vaddr = base + i * PAGE_SIZE;

        /* Query current mapping */
        page_query_result_t query;
        if (page_query(pml4, vaddr, &query) != PAGE_OK) {
            continue;  /* Page not mapped, skip */
        }

        /* Add to COW tracking */
        cow_result_t result = cow_add_page(query.phys_addr);
        if (result != COW_OK) {
            klog_error("[COW] Failed to add page: 0x%llX\n", query.phys_addr);
            return result;
        }

        /* Increment refcount (for the child process) */
        cow_inc_refcount(query.phys_addr);

        /* Mark page as read-only with COW flag */
        page_query_result_t new_query;
        if (page_query(pml4, vaddr, &new_query) == PAGE_OK) {
            uint64_t new_flags = new_query.flags;
            new_flags &= ~PAGE_FLAG_WRITE;  /* Clear write */
            new_flags = cow_set_cow_flag(new_flags);  /* Set COW */
            page_update_flags(pml4, vaddr, new_flags);
        }
    }

    /* Flush TLB */
    page_invalidate_tlb();

    return COW_OK;
}
```

---

## 常见问题

### 问题一：COW 页错误不被识别

**症状**：写入 COW 页时直接触发 GPF 而不是 #PF

**原因**：页表项可能没有正确设置 COW 标志

**解决**：检查 `cow_register_region` 是否正确调用，确认页表项的 RW 位已清除、COW 标志已设置

### 问题二：引用计数泄漏

**症状**：进程退出后 COW 页没有被释放

**原因**：进程退出时没有调用 `cow_unregister_region`

**解决**：在进程销毁路径中确保调用 `cow_unregister_region`

### 问题三：递归页错误

**症状**：COW 处理过程中再次触发页错误

**原因**：`cow_copy_on_write` 中的内存分配或临时映射可能触发页错误

**解决**：使用预分配的内存池，或确保临时映射不会触发新的页错误

---

## 下一步

现在我们已经把 COW 模块集成到页错误处理器中，COW 功能已经可以工作了。

但要让整个系统运行起来，我们还需要：

1. 在内核初始化时初始化 COW 模块
2. 初始化异常处理器
3. 创建 COW 演示程序来测试功能

下一节我们将完成内核集成，包括修改 `kernel_init.c`、创建 COW 演示程序、编译验证等。

在继续之前，请确保你理解了：
1. COW 页错误的完整处理流程
2. `cow_handle_fault` 的实现
3. 引用计数如何管理
4. COW 标志如何存储和检查
