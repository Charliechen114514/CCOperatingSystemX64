# Stage 17: 堆内存分配器 (kmalloc/kfree) 教程

本教程将带你从零开始实现一个完整的内核堆内存分配器，为操作系统提供字节级动态内存管理能力。

---

## 阶段概述

**Stage 17: 堆内存分配器 (kmalloc/kfree)**

本阶段在 Stage 16 虚拟内存管理与页错误处理基础上，实现了内核堆内存分配器，为操作系统提供了字节级内存管理能力。这标志着从页级内存管理（4KB 粒度）迈向更灵活的动态内存分配，为内核动态数据结构（如链表、树、字符串等）提供了内存分配基础。

### 核心成果

- **堆分配器模块** (`kernel/mm/heap/`)
  - kmalloc/kfree/krealloc 核心分配 API
  - kmalloc_aligned 对齐分配支持
  - Best-fit 分配算法
  - 自动块合并（coalescing）
  - 动态堆扩展

- **堆管理功能**
  - 基于虚拟内存管理的后备存储
  - 内存块完整性检测（魔数校验）
  - 双重释放检测
  - 详细统计信息收集
  - 调试转储功能

- **演示程序** (`kernel/demo/heap/`)
  - 多个综合测试用例
  - 基本分配/释放测试
  - 多次分配测试
  - 对齐分配测试
  - 重分配测试
  - 压力测试

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

---

## 文档导航

本教程按开发顺序组织，建议依次阅读：

### 1. [为什么需要堆内存分配器](./01_为什么需要堆内存分配器.md)

从 VMM 页级管理的局限性说起，讨论为什么需要字节级的内存分配。

### 2. [堆分配器设计基础](./02_堆分配器设计基础.md)

深入探讨堆分配器的设计，包括堆与栈的区别、内存布局、分配算法选择、数据结构设计。

### 3. [实现核心数据结构](./03_实现核心数据结构.md)

创建目录结构，定义 heap_block_t 结构体、返回码、统计结构，配置 CMake 构建系统。

### 4. [实现 kmalloc 核心分配逻辑](./04_实现kmalloc核心分配逻辑.md)

实现 Best-Fit 搜索算法、块分割、空闲链表操作，完成核心的 kmalloc 函数。

### 5. [实现 kfree 释放与块合并](./05_实现kfree释放与块合并.md)

实现块合并算法、双重释放检测、魔数验证，完成 kfree 函数。

### 6. [实现堆扩展与 VMM 集成](./06_实现堆扩展与VMM集成.md)

实现堆扩展机制，向 VMM 申请更多虚拟页，实现 heap_init 初始化流程。

### 7. [实现高级功能](./07_实现高级功能.md)

实现 krealloc 重分配和 kmalloc_aligned 对齐分配，支持更多使用场景。

### 8. [统计调试与演示程序](./08_统计调试与演示程序.md)

实现 heap_get_stats 和 heap_dump 调试功能，创建完整的演示程序验证正确性。

---

## 快速开始

### 环境要求

```
操作系统: Ubuntu 22.04 LTS / WSL2 / Arch WSL
编译器:   GCC x86_64-elf-gcc 或类似交叉编译器
构建工具: CMake 3.25 或更高版本
模拟器:   QEMU 7.0 或更高版本
前置阶段: Stage 16 (VMM 与页错误处理) 已完成
```

### 构建与运行

```bash
cd /home/charliechen/CCOperatingSystemX64

# 配置构建
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 构建
cmake --build build

# 运行
qemu-system-x86_64 -kernel build/kernel.bin -serial stdio
```

### 启用演示程序

```bash
# 配置构建，启用堆演示
cmake -B build -DENABLE_HEAP_DEMO=ON

# 构建
cmake --build build

# 运行（演示会自动执行）
qemu-system-x86_64 -kernel build/kernel.bin -serial stdio
```

---

## 源码文件

- [`kernel/mm/heap/heap.h`](../../kernel/mm/heap/heap.h) - 堆分配器接口
- [`kernel/mm/heap/heap.c`](../../kernel/mm/heap/heap.c) - 堆分配器实现
- [`kernel/mm/heap/heap_config.h`](../../kernel/mm/heap/heap_config.h) - 配置常量
- [`kernel/demo/heap/heap_demo.h`](../../kernel/demo/heap/heap_demo.h) - 演示接口
- [`kernel/demo/heap/heap_demo.c`](../../kernel/demo/heap/heap_demo.c) - 演示实现

---

## 相关资源

### 项目文档
- [`../../PROGRESS.md`](../../PROGRESS.md) - 项目进度
- [`../../document/17_kmalloc_kfree/`](../../document/17_kmalloc_kfree/) - 详细技术文档

### 外部参考
- [Memory Allocation](https://wiki.osdev.org/Memory_Allocation)
- [Malloc Implementation](https://wiki.osdev.org/Implementing_Malloc)
- [Kmalloc](https://kernel.org/doc/html/latest/mm/kmalloc.html)

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-20
