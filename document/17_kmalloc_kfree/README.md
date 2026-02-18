# CCOS 堆内存分配器 (kmalloc/kfree) 文档中心

本目录包含 CCOS Stage 17 - 堆内存分配器开发的完整文档体系。

---

## 阶段概述

**Stage 17: 堆内存分配器 (kmalloc/kfree)**

本阶段在 Stage 16 虚拟内存管理与页错误处理基础上，实现了内核堆内存分配器，为操作系统提供了字节级内存管理能力。这标志着从页级内存管理（4KB 粒度）迈向更灵活的动态内存分配，为内核动态数据结构（如链表、树、字符串等）提供了内存分配基础。

### 核心成果

- **堆分配器模块** ([`kernel/mm/heap/heap.h`](../../kernel/mm/heap/heap.h))
  - kmalloc/kfree/krealloc 核心分配 API
  - kmalloc_aligned 对齐分配支持
  - Best-fit 分配算法
  - 自动块合并（coalescing）
  - 动态堆扩展

- **堆管理功能** ([`kernel/mm/heap/heap.c`](../../kernel/mm/heap/heap.c))
  - 基于虚拟内存管理的后备存储
  - 内存块完整性检测（魔数校验）
  - 双重释放检测
  - 详细统计信息收集
  - 调试转储功能

- **堆演示程序** ([`kernel/demo/heap/heap_demo.h`](../../kernel/demo/heap/heap_demo.h))
  - 10 个综合测试用例
  - 基本分配/释放测试
  - 多次分配测试
  - 对齐分配测试
  - 重分配测试
  - 块合并测试
  - 压力测试

- **构建系统集成**
  - CMake 支持 ENABLE_HEAP_DEMO 选项
  - 内核初始化集成 heap_init()
  - 与 VMM 模块的无缝集成

---

## 目录结构

```
kernel/
├── mm/
│   └── heap/
│       ├── heap.h                  # 堆分配器接口
│       ├── heap.c                  # 堆分配器实现
│       ├── heap_config.h           # 配置常量
│       └── CMakeLists.txt          # 构建配置
├── demo/
│   └── heap/
│       ├── heap_demo.h             # 堆演示接口
│       └── heap_demo.c             # 堆演示实现
└── kernel_init.c                   # 添加 heap_init() 调用
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要堆分配器
- 堆分配器设计基础
- 设计决策（算法选择、内存布局）
- 架构设计与模块协作
- 实现细节与关键技术
- 常见陷阱与注意事项
- 未来改进方向

**适合**:
- 理解设计思路
- 学习堆分配算法
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- 堆内存布局详解
- Heap API 完整参考
- 数据结构定义
- 常量定义
- 算法说明（best-fit、合并、扩展）
- 返回码定义

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 问题诊断

**内容**:
- 初始化问题
- 分配问题（返回 NULL、内存不足）
- 释放问题（双重释放、野指针、内存泄漏）
- 堆损坏检测（魔数校验）
- 碎片化问题
- 调试技巧

每个问题按"症状-原因-解决方案"格式组织。

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

### 4. [调试工具指南](./调试工具指南.md) 工具使用

**内容**:
- 内置调试工具（heap_dump、heap_get_stats）
- QEMU 调试方法
- GDB 调试技巧
- Demo 测试说明
- 性能分析方法

**适合**:
- 学习调试工具
- 查询具体命令
- 提升调试效率

---

## 快速开始

### 查看代码结构

1. **堆分配器接口** → 查看 [`kernel/mm/heap/heap.h`](../../kernel/mm/heap/heap.h)
2. **堆分配器实现** → 查看 [`kernel/mm/heap/heap.c`](../../kernel/mm/heap/heap.c)
3. **堆演示程序** → 查看 [`kernel/demo/heap/heap_demo.h`](../../kernel/demo/heap/heap_demo.h)

### 使用示例

```c
#include "mm/heap/heap.h"

// 内核初始化时调用
void kernel_init(void) {
    // 确保在 vmm_init() 之后调用
    heap_init();
}

// 基本分配与释放
void basic_example(void) {
    // 分配 64 字节
    void* ptr = kmalloc(64);
    if (ptr != NULL) {
        // 写入数据
        volatile uint32_t* data = (uint32_t*)ptr;
        data[0] = 0xDEADBEEF;

        // 释放内存
        kfree(ptr);
    }
}

// 分配数组
void array_example(void) {
    // 分配 100 个 int 的数组
    int* array = (int*)kmalloc(100 * sizeof(int));
    if (array != NULL) {
        for (int i = 0; i < 100; i++) {
            array[i] = i;
        }
        kfree(array);
    }
}

// 重分配
void realloc_example(void) {
    char* str = (char*)kmalloc(16);
    if (str != NULL) {
        // 复制数据
        // ...

        // 扩展到 64 字节
        str = (char*)krealloc(str, 64);
        if (str != NULL) {
            // 继续使用扩展后的内存
            // ...
        }
        kfree(str);
    }
}

// 对齐分配（用于 DMA、SIMD 等）
void aligned_example(void) {
    // 分配 256 字节，64 字节对齐
    void* ptr = kmalloc_aligned(256, 64);
    if (ptr != NULL) {
        // ptr 保证 64 字节对齐
        kfree(ptr);  // 使用普通 kfree 释放
    }
}
```

### 查看统计信息

```c
#include "mm/heap/heap.h"
#include "klogs/kprintf.h"

void print_heap_stats(void) {
    heap_stats_t stats;

    if (heap_get_stats(&stats) == HEAP_OK) {
        klog_trace("[Heap] Total: %llu bytes\n", stats.total_bytes);
        klog_trace("[Heap] Used: %llu bytes\n", stats.used_bytes);
        klog_trace("[Heap] Free: %llu bytes\n", stats.free_bytes);
        klog_trace("[Heap] Used blocks: %llu\n", stats.used_blocks);
        klog_trace("[Heap] Free blocks: %llu\n", stats.free_blocks);
        klog_trace("[Heap] Alloc count: %llu\n", stats.alloc_count);
        klog_trace("[Heap] Free count: %llu\n", stats.free_count);
    }
}

// 转储详细堆状态
void dump_heap(void) {
    heap_dump();  // 输出所有块的详细信息
}
```

### 启用堆演示

```bash
# 编译时启用堆演示
cmake -DENABLE_HEAP_DEMO=ON ..
make

# 运行系统，演示将自动执行
qemu-system-x86_64 -kernel build/kernel.bin
```

---

## 与前一阶段对比

| 特性 | Stage 16 (VMM 与页错误) | Stage 17 (kmalloc/kfree) |
|------|------------------------|-------------------------|
| 内存粒度 | 4KB 页 | 字节级 |
| 分配 API | vmm_alloc_pages() | kmalloc() |
| 适用场景 | 大块内存、页映射 | 小对象、动态结构 |
| 后备存储 | 物理帧分配器 | VMM 虚拟内存 |
| 初始化需求 | pframe_init() | vmm_init() + heap_init() |
| 对齐支持 | 页对齐 (4KB/2MB/1GB) | 任意 2 的幂对齐 |
| 统计信息 | 页级统计 | 字节级统计 |
| 新增文件 | - | 6+ 个 |

---

## 技术亮点

### 1. Best-Fit 分配算法

```c
// 搜索最适合的空闲块
static heap_block_t* find_best_fit(size_t size) {
    heap_block_t* best = NULL;
    heap_block_t* current = s_heap.free_list;

    while (current != NULL) {
        if (!current->used && current->size >= size) {
            if (best == NULL || current->size < best->size) {
                best = current;
            }
        }
        current = current->next;
    }

    return best;
}
```

### 2. 自动块合并

```c
// 合并相邻的空闲块
static void coalesce_blocks(heap_block_t* block) {
    // 尝试与下一个块合并
    if (block->next != NULL && !block->next->used) {
        block->size += block->next->size;
        block->next = block->next->next;
        if (block->next != NULL) {
            block->next->prev = block;
        }
    }

    // 尝试与前一个块合并
    if (block->prev != NULL && !block->prev->used) {
        block->prev->size += block->size;
        block->prev->next = block->next;
        if (block->next != NULL) {
            block->next->prev = block->prev;
        }
    }
}
```

### 3. 块头结构与魔数检测

```c
typedef struct heap_block {
    uint64_t size;           // 包含头部的总大小
    bool used;               // 是否已使用
    struct heap_block* prev; // 前一个块
    struct heap_block* next; // 下一个块（空闲链表）
    uint32_t magic;          // 魔数：0x00114514
    uint32_t _padding;       // 16 字节对齐
} __attribute__((aligned(16))) heap_block_t;
```

### 4. 对齐分配实现

```c
void* kmalloc_aligned(size_t size, size_t alignment) {
    // 分配额外空间存储原始指针
    size_t total_size = total_block_size(size) + alignment + sizeof(virtual_addr_t*);

    void* raw_ptr = kmalloc(total_size);
    if (raw_ptr == NULL) {
        return NULL;
    }

    // 计算对齐地址
    virtual_addr_t aligned_addr = align_up(raw_start + sizeof(virtual_addr_t*), alignment);

    // 在对齐地址前存储原始指针
    virtual_addr_t* orig_ptr_loc = (virtual_addr_t*)(aligned_addr - sizeof(virtual_addr_t*));
    *orig_ptr_loc = raw_start;

    return (void*)aligned_addr;
}
```

### 5. 与 VMM 的集成

```c
// 堆扩展时使用 VMM 分配虚拟页
static heap_result_t expand_heap(size_t min_expansion) {
    // 计算需要的页数
    uint64_t page_count = (min_expansion + PAGE_SIZE - 1) / PAGE_SIZE;

    // 使用 VMM 分配虚拟页
    virtual_addr_t new_vaddr = s_heap.heap_brk;
    vmm_result_t result = vmm_alloc_pages_at(new_vaddr, page_count, VMAP_FLAG_WRITE);

    if (result == VMM_OK) {
        s_heap.heap_brk = new_vaddr + page_count * PAGE_SIZE;
        s_heap.stats.expand_count++;
        return HEAP_OK;
    }

    return HEAP_ERR_OOM;
}
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
┌───────▼──────┐   ┌─────▼──────┐   ┌─────▼──────┐
│ 开发笔记     │   │ 技术参考   │   │ 故障排查   │
└──────────────┘   └─────────────┘   └─────────────┘
        │                    │
        └──────────────────┬─────────┘
                           │
                    ┌──────▼──────┐
                    │ 调试工具指南 │
                    └─────────────┘
```

---

## 版本信息

- **阶段**: Stage 17
- **分支**: `stage/17_kmalloc_kfree`
- **日期**: 2026-02-18
- **作者**: Charliechen114514

---

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../16_vmm_pagefaults/README.md](../16_vmm_pagefaults/) - 上一阶段文档

### 源码文件
- [`kernel/mm/heap/heap.h`](../../kernel/mm/heap/heap.h) - 堆分配器接口
- [`kernel/mm/heap/heap.c`](../../kernel/mm/heap/heap.c) - 堆分配器实现
- [`kernel/demo/heap/heap_demo.h`](../../kernel/demo/heap/heap_demo.h) - 堆演示程序
- [`kernel/mm/vmm/vmm.h`](../../kernel/mm/vmm/vmm.h) - VMM 接口（堆的后备存储）

### 外部参考
- [Memory Allocation](https://wiki.osdev.org/Memory_Allocation)
- [Malloc Implementation](https://wiki.osdev.org/Implementing_Malloc)
- [Kmalloc](https://kernel.org/doc/html/latest/mm/kmalloc.html)
- [OSDev.org](https://wiki.osdev.org)

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-18
