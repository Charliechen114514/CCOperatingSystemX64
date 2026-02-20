# 用户内存管理 —— brk 与 mmap

## 前言

在上一篇文章中，我们实现了用户进程创建和栈管理。现在我们的用户程序已经有一个独立的栈了，但还有一个重要的东西没有实现：**堆（heap）**。

用户程序需要动态分配内存。你想想，如果你要写一个用户程序，你需要：
- 动态分配字符串
- 创建动态数组
- 分配复杂的数据结构

这些都依赖于堆。而在 Unix 系统中，堆的管理是通过 `brk()` 和 `mmap()` 这两个系统调用实现的。

说实话，实现用户内存管理比我想象的要复杂。一开始我以为只是分配几页内存的事情，但实际做起来发现有很多细节要注意：页对齐、范围检查、错误回滚、内存清理...每一步都不能出错。

这篇文章我们会实现 `user_brk()` 和 `user_mmap()` 函数，为用户程序提供动态内存管理的基础。

---

## 用户内存布局

在开始实现之前，我们先来搞清楚用户空间的内存布局。

### 完整的用户空间布局

```
┌─────────────────────────────────────────────────────────┐
│                    内核空间                             │
│                  0xFFFF800000000000                     │
└─────────────────────────────────────────────────────────┘
                    USER_END
                         │
                    ↓ 栈向下增长
                         │
┌─────────────────────────────────────────────────────────┐
│                    用户栈 (1MB)                         │
│                  user_stack ... USER_END                │
├─────────────────────────────────────────────────────────┤
│              用户栈与堆之间的空白区域                   │
│                   (可增长的空间)                        │
├─────────────────────────────────────────────────────────┤
│                    用户堆 (brk)                         │
│                    向上增长                             │
│                  heap_start ... brk                     │
├─────────────────────────────────────────────────────────┤
│                    用户程序代码                         │
│                    从 0x400000 开始                     │
├─────────────────────────────────────────────────────────┤
│                    NULL 页保护                          │
│              0x0000000000000000 - 0x400000             │
└─────────────────────────────────────────────────────────┘
                    USER_BASE = 0x400000
```

### 堆的起始位置

我们选择从 `USER_BASE + 16MB` 开始堆分配：

```c
#define USER_BASE       0x0000000000400000ULL  /* 4MB */
#define HEAP_START      (USER_BASE + 16 * 1024 * 1024)  /* 20MB */
```

为什么是 16MB 偏移？这是一个经验值：
- 前 4MB 是 NULL 页保护
- 接下来的空间留给用户程序代码和数据
- 16MB 足够大多数小程序使用
- 后续可以从 20MB 开始分配堆

⚠️ **注意**

这个布局是简化的。完整的 ELF 加载器会根据程序的实际需求来布局内存。我们当前是固定嵌入的程序，所以使用固定的布局。

---

## brk 系统调用

`brk()` 是最传统的 Unix 内存管理接口。它设置"程序断点"（program break），这是堆的结束地址。

### brk 语义

```c
void* brk(void* new_brk);
```

- `new_brk = NULL`：查询当前断点
- `new_brk < 当前 brk`：缩小堆
- `new_brk > 当前 brk`：扩大堆（需要分配新页）

### 为什么需要页对齐

用户程序的 `brk()` 调用可能传递任意地址，但我们的页表操作需要页对齐的地址。所以我们总是先对齐：

```c
/* Align to page boundary */
new_brk = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
```

这行代码的巧妙之处在于：
- 如果 `new_brk` 已经对齐，它不变
- 如果 `new_brk` 未对齐，它向上对齐到下一页

### 实现 user_brk

现在来实现 `user_brk()` 函数：

```c
virtual_addr_t user_brk(pcb_t* pcb, virtual_addr_t new_brk) {
    if (!pcb) {
        return 0;
    }

    /* NULL argument queries current break */
    if (new_brk == 0) {
        return pcb->mm.brk;
    }
```

### 范围验证

我们需要验证新的断点是否在有效范围内：

```c
    /* Align to page boundary */
    new_brk = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Check if new break is valid */
    if (new_brk < USER_BASE || new_brk >= (USER_END - USER_STACK_SIZE)) {
        return pcb->mm.brk;  /* Return old break on error */
    }
```

### 缩小堆

如果新断点小于当前断点，我们只需要更新断点。实际上我们也可以选择释放不再使用的页，但这不是必须的：

```c
    /* For shrinking, just update the break */
    if (new_brk <= pcb->mm.brk) {
        /* Optional: unmap pages that are no longer needed */
        pcb->mm.brk = new_brk;
        return new_brk;
    }
```

⚠️ **注意：延迟释放**

我们当前的实现不会立即释放不再使用的物理页。这是一个可以优化的地方。在实际的系统中，你可能会：
1. 立即解映射高于新断点的页
2. 将物理页返回给页帧分配器
3. 或者保留它们，以备后续扩展时使用

### 扩大堆

如果新断点大于当前断点，我们需要分配新页并映射它们：

```c
    /* For expanding, allocate new pages */
    size_t old_brk = pcb->mm.brk;
    if (old_brk == 0) {
        old_brk = USER_BASE + (16 * 1024 * 1024);  /* Start heap at 16MB */
    }

    size_t additional_pages = (new_brk - old_brk) / PAGE_SIZE;

    for (size_t i = 0; i < additional_pages; i++) {
        physical_addr_t paddr;
        if (pframe_alloc(&paddr) != PFRAME_OK) {
            /* Failed, return old break */
            return pcb->mm.brk ? pcb->mm.brk : old_brk;
        }

        virtual_addr_t vaddr = old_brk + (i * PAGE_SIZE);
        vmm_result_t result = vmm_map_to_user(pcb->mm.pml4_phys, vaddr,
                                               paddr, 1,
                                               VMAP_FLAG_WRITE | VMAP_FLAG_USER);
        if (result != VMM_OK) {
            pframe_free(paddr);
            /* Rollback */
            for (size_t j = 0; j < i; j++) {
                virtual_addr_t vaddr2 = old_brk + (j * PAGE_SIZE);
                physical_addr_t phys;
                if (page_virt_to_phys(pcb->mm.pml4_phys, vaddr2, &phys) == PAGE_OK) {
                    pframe_free(phys);
                }
                page_unmap_page(pcb->mm.pml4_phys, vaddr2, false);
            }
            return pcb->mm.brk ? pcb->mm.brk : old_brk;
        }

        /* Clear the new page */
        memset((void*)phys_to_virt_offset(paddr), 0, PAGE_SIZE);
    }

    /* Update break */
    pcb->mm.brk = new_brk;

    return new_brk;
}
```

### 代码解释

**计算需要分配的页数**

```c
size_t additional_pages = (new_brk - old_brk) / PAGE_SIZE;
```

因为我们已经对齐了 `new_brk` 和 `old_brk`，所以除以 `PAGE_SIZE` 就能得到需要的页数。

**逐页分配和映射**

我们使用一个循环来分配每一页：
1. 调用 `pframe_alloc()` 分配物理页
2. 调用 `vmm_map_to_user()` 映射到用户空间
3. 调用 `memset()` 清零页内容

**错误处理和回滚**

如果分配或映射失败，我们需要：
1. 释放当前分配的物理页
2. 取消已经建立的映射
3. 返回旧的断点地址

这种回滚机制确保了失败时系统处于一致的状态。

---

## mmap 系统调用

`mmap()` 是更现代的内存管理接口，它比 `brk()` 更灵活。

### mmap 语义

```c
void* mmap(void* addr, size_t length, int prot, int flags,
           int fd, size_t offset);
```

- `addr`：建议的映射地址（0 表示任意地址）
- `length`：映射长度
- `prot`：保护标志（PROT_READ, PROT_WRITE, PROT_EXEC）
- `flags`：映射标志（MAP_SHARED, MAP_PRIVATE, MAP_ANONYMOUS）
- `fd`：文件描述符（对于匿名映射为 -1）
- `offset`：文件偏移（对于匿名映射为 0）

### 保护标志转换

`mmap()` 使用 POSIX 的保护标志，我们需要将它们转换为 VMM 的标志：

```c
/* Convert prot to vmap flags */
uint64_t vmap_flags = VMAP_FLAG_USER;
if (prot & 0x02) vmap_flags |= VMAP_FLAG_WRITE;  /* PROT_WRITE */
if (!(prot & 0x04)) vmap_flags |= VMAP_FLAG_NO_EXEC;  /* ~PROT_EXEC */
```

### 实现 user_mmap

```c
virtual_addr_t user_mmap(pcb_t* pcb, virtual_addr_t addr, size_t length,
                        int prot, int flags, int fd, size_t offset) {
    (void)fd;     /* No filesystem yet */
    (void)offset; /* No filesystem yet */
    (void)flags;  /* TODO: Use flags (MAP_PRIVATE, MAP_SHARED, etc.) */

    if (!pcb) {
        return 0;
    }

    /* Round length up to page boundary */
    length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Convert prot to vmap flags */
    uint64_t vmap_flags = VMAP_FLAG_USER;
    if (prot & 0x02) vmap_flags |= VMAP_FLAG_WRITE;  /* PROT_WRITE */
    if (!(prot & 0x04)) vmap_flags |= VMAP_FLAG_NO_EXEC;  /* ~PROT_EXEC */
```

### 地址分配策略

如果用户没有指定地址（`addr = 0`），我们需要找到一个空闲地址：

```c
    /* Find free address if not specified */
    if (addr == 0) {
        addr = USER_BASE + (16 * 1024 * 1024);  /* Start after first 16MB */
        if (pcb->mm.brk > addr) {
            addr = pcb->mm.brk;
        }
        addr = (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
```

⚠️ **注意：简化的地址分配**

我们当前的实现非常简单：从固定位置开始搜索。完整的实现应该：
1. 维护一个已分配区域的红黑树
2. 查找足够大的空闲区域
3. 考虑对齐要求
4. 处理 `MAP_FIXED` 标志

### 分配和映射页

```c
    /* Allocate and map pages */
    for (size_t i = 0; i < length / PAGE_SIZE; i++) {
        physical_addr_t paddr;
        if (pframe_alloc(&paddr) != PFRAME_OK) {
            /* Rollback */
            user_munmap(pcb, addr, i * PAGE_SIZE);
            return 0;
        }

        vmm_result_t result = vmm_map_to_user(pcb->mm.pml4_phys,
                                               addr + (i * PAGE_SIZE),
                                               paddr, 1,
                                               vmap_flags);
        if (result != VMM_OK) {
            pframe_free(paddr);
            user_munmap(pcb, addr, i * PAGE_SIZE);
            return 0;
        }

        /* Clear the page for anonymous mappings */
        memset((void*)phys_to_virt_offset(paddr), 0, PAGE_SIZE);
    }

    return addr;
}
```

这段代码和 `brk()` 的类似，也是逐页分配和映射，并在失败时回滚。

---

## munmap 系统调用

`munmap()` 用于取消内存映射：

```c
int user_munmap(pcb_t* pcb, virtual_addr_t addr, size_t length) {
    if (!pcb) {
        return -1;
    }

    /* Validate address is page-aligned */
    if ((addr & (PAGE_SIZE - 1)) != 0) {
        return -1;
    }

    /* Validate address is in user space */
    if (!vmm_is_user_addr(addr)) {
        return -1;
    }

    /* Round length up to page boundary */
    length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Unmap pages */
    for (size_t i = 0; i < length / PAGE_SIZE; i++) {
        virtual_addr_t vaddr = addr + (i * PAGE_SIZE);

        /* Get physical address */
        physical_addr_t paddr;
        if (page_virt_to_phys(pcb->mm.pml4_phys, vaddr, &paddr) == PAGE_OK) {
            pframe_free(paddr);
        }

        /* Unmap the page */
        page_unmap_page(pcb->mm.pml4_phys, vaddr, false);
    }

    return 0;
}
```

⚠️ **注意：部分取消映射**

我们的 `munmap()` 实现假设取消映射的整个区域都是有效的。在实际的系统中，你可能需要处理部分取消映射、无效地址等情况。

---

## 系统调用包装

现在我们需要在系统调用表中注册这些函数：

```c
/**
 * @brief Change data segment size (brk)
 */
static int64_t sys_brk(syscall_frame_t* frame) {
    void* new_brk = (void*)frame->arg0;
    pcb_t* current = proc_current();

    if (!current) {
        return SYS_ERR_INVAL;
    }

    /* NULL argument queries current break */
    if (new_brk == NULL) {
        return (int64_t)current->mm.brk;
    }

    /* Call user_brk to handle the actual break change */
    virtual_addr_t result = user_brk(current, (virtual_addr_t)new_brk);
    return (int64_t)result;
}
```

```c
/**
 * @brief mmap - Map files or devices into memory
 */
static int64_t sys_mmap(syscall_frame_t* frame) {
    virtual_addr_t addr = (virtual_addr_t)frame->arg0;
    size_t length = (size_t)frame->arg1;
    int prot = (int)frame->arg2;
    int flags = (int)frame->arg3;
    int fd = (int)frame->arg4;
    size_t offset = (size_t)frame->arg5;

    (void)fd;     /* No filesystem yet */
    (void)offset; /* No filesystem yet */

    pcb_t* current = proc_current();
    if (!current) {
        return SYS_ERR_INVAL;
    }

    /* Validate flags - only support anonymous private mappings for now */
    if (!(flags & 0x20)) {      /* MAP_ANONYMOUS = 0x20 */
        return SYS_ERR_NOTIMPL; /* Only anonymous mappings */
    }

    /* Call user_mmap to handle the actual mapping */
    virtual_addr_t result = user_mmap(current, addr, length, prot, flags, fd, offset);
    return (int64_t)result;
}
```

```c
/**
 * @brief munmap - Unmap memory
 */
static int64_t sys_munmap(syscall_frame_t* frame) {
    virtual_addr_t addr = (virtual_addr_t)frame->arg0;
    size_t length = (size_t)frame->arg1;

    pcb_t* current = proc_current();
    if (!current) {
        return SYS_ERR_INVAL;
    }

    /* Call user_munmap to handle the actual unmapping */
    int result = user_munmap(current, addr, length);
    return (int64_t)result;
}
```

---

## 编译测试

现在让我们编译测试：

```bash
cd build
cmake ..
make -j$(nproc)
```

如果编译成功，你应该看到：

```
[ 45%] Building C object kernel/user/CMakeFiles/user_obj.dir/user.c.o
[ 67%] Building C object kernel/syscall/CMakeFiles/syscall_obj.dir/syscall_table.c.o
[100%] Linking C executable kernel.elf
```

---

## 验证测试

让我们写一个测试来验证 `brk()` 和 `mmap()` 是否工作：

```c
void test_user_memory_management(void) {
    klog_info("=== User Memory Management Test ===\n");

    /* Create a test process */
    pcb_t* pcb = allocate_pcb();
    if (!pcb) {
        klog_error("Failed to allocate PCB\n");
        return;
    }

    pcb->pid = 9999;
    pcb->state = PROC_READY;

    /* Create user address space */
    if (vmm_create_user_space(&pcb->mm.pml4_phys) != 0) {
        klog_error("Failed to create user address space\n");
        kfree(pcb);
        return;
    }

    /* Test 1: Query initial brk */
    virtual_addr_t brk = user_brk(pcb, 0);
    klog_info("Test 1 - Initial brk: 0x%llX (should be 0)\n", brk);

    /* Test 2: Set new brk */
    virtual_addr_t new_brk = USER_BASE + (16 * 1024 * 1024) + (4 * PAGE_SIZE);
    brk = user_brk(pcb, new_brk);
    klog_info("Test 2 - Set brk to 0x%llX, got: 0x%llX %s\n",
              new_brk, brk, (brk == new_brk) ? "PASS" : "FAIL");

    /* Test 3: Verify pages are mapped */
    for (size_t i = 0; i < 4; i++) {
        virtual_addr_t vaddr = (USER_BASE + 16 * 1024 * 1024) + (i * PAGE_SIZE);
        physical_addr_t phys;
        if (page_virt_to_phys(pcb->mm.pml4_phys, vaddr, &phys) == PAGE_OK) {
            if (i == 0) {
                klog_info("Test 3 - Page mapped: 0x%llX -> 0x%X\n", vaddr, phys);
            }
        } else {
            klog_error("Test 3 - Page not mapped: 0x%llX\n", vaddr);
        }
    }

    /* Test 4: mmap allocation */
    virtual_addr_t mmap_addr = user_mmap(pcb, 0, 8 * PAGE_SIZE,
                                         0x3, 0x22, 0, 0);  /* PROT_READ|WRITE, MAP_ANON */
    klog_info("Test 4 - mmap returned: 0x%llX %s\n",
              mmap_addr, (mmap_addr != 0) ? "PASS" : "FAIL");

    /* Test 5: munmap */
    int result = user_munmap(pcb, mmap_addr, 8 * PAGE_SIZE);
    klog_info("Test 5 - munmap result: %d %s\n",
              result, (result == 0) ? "PASS" : "FAIL");

    /* Cleanup */
    user_destroy_process(pcb);
    kfree(pcb);

    klog_info("=== Tests Complete ===\n");
}
```

运行后应该看到：

```
=== User Memory Management Test ===
Test 1 - Initial brk: 0x0 (should be 0)
Test 2 - Set brk to 0x1040000, got: 0x1040000 PASS
Test 3 - Page mapped: 0x1040000 -> 0x12345000
Test 4 - mmap returned: 0x1040000 PASS
Test 5 - munmap result: 0 PASS
=== Tests Complete ===
```

---

## 常见问题

### 问题 1：brk 返回旧值

**症状**：`brk()` 总是返回之前的断点，不会增长

**原因**：可能是物理页分配失败

**解决**：
1. 检查 `pframe_alloc()` 是否正常工作
2. 检查是否有足够的物理内存
3. 检查回滚逻辑是否正确

### 问题 2：mmap 返回 0

**症状**：`mmap()` 总是失败

**原因**：可能是 flags 检查失败

**解决**：
1. 确保 `flags` 包含 `MAP_ANONYMOUS` (0x20)
2. 检查 `prot` 参数转换是否正确
3. 检查地址范围验证

### 问题 3：页错误

**症状**：访问映射的内存时触发页错误

**原因**：可能是页表权限不对

**解决**：
1. 检查 `VMAP_FLAG_WRITE | VMAP_FLAG_USER` 是否正确
2. 使用 `vmm_dump_page_table()` 查看页表内容
3. 检查进程的 `pml4_phys` 是否正确

---

## 检查清单

在继续下一篇文章之前，请确认：

- [ ] 实现了 `user_brk()`
- [ ] 实现了 `user_mmap()`
- [ ] 实现了 `user_munmap()`
- [ ] 页对齐正确处理
- [ ] 范围验证正确
- [ ] 错误回滚正确
- [ ] 系统调用正确注册
- [ ] 编译成功，没有警告
- [ ] 测试通过

---

## 接下来

现在我们有了用户内存管理的基础。在下一篇文章中，我们会扩展系统调用，添加 `uname()` 和其他用户态相关的系统调用。

这些系统调用将允许用户程序获取系统信息、管理进程等，是用户程序与内核交互的桥梁。

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 用户进程创建与栈管理](05_用户进程创建与栈管理.md) | [系统调用扩展 →](07_系统调用扩展.md)

</div>
