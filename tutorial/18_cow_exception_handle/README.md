# CCOS 写时复制与异常处理 - 教程

本阶段将实现写时复制（Copy-on-Write）内存管理机制和关键的异常处理器，为操作系统的进程管理和错误恢复提供基础支持。

---

## 阶段概述

### 你将学到什么

| 技能 | 描述 |
|------|------|
| 写时复制 (COW) | 基于页错误的延迟复制机制 |
| 哈希表实现 | 泛型哈希表数据结构与链式冲突解决 |
| 引用计数管理 | 原子引用计数与共享资源跟踪 |
| x86_64 异常处理 | Double Fault/Stack Fault/GPF 处理器 |
| GDT/TSS 管理 | 内核 GDT 初始化与 TSS 栈配置 |
| IST 栈机制 | 中断栈表与异常隔离 |

### 与前一阶段对比

| 特性 | stage/17 | stage/18 (本阶段) |
|------|----------|------------------|
| 堆管理 | kmalloc/kfree | 堆管理 + COW 支持 |
| 页错误处理 | 基础 #PF 处理 | 集成 COW 处理 |
| 异常处理 | 无 | GPF/SS/DF 处理器 |
| 哈希表 | 无 | 泛型哈希表实现 |
| GDT/TSS | 使用 Bootloader | 内核完整管理 |
| IST 栈 | 无 | Double Fault/Stack Fault IST |
| 统计信息 | 堆统计 | COW + 异常统计 |
| 新增文件 | - | 15+ 个 |

---

## 文档导航

本教程包含 11 篇文档，按开发顺序排列：

### 基础篇：COW 机制（3篇）

### 1. [为什么需要写时复制](./01_为什么需要写时复制.md)
**动机和背景**
- 传统 fork() 的性能问题
- 写时复制的价值
- CCOS 为什么在此时实现 COW

### 2. [写时复制机制详解](./02_写时复制机制详解.md)
**COW 原理与设计**
- COW 的基本原理
- 引用计数管理机制
- 页错误处理流程
- COW 页表标志位设计

### 3. [实现泛型哈希表数据结构](./03_实现泛型哈希表数据结构.md)
**哈希表实现**
- 为什么 COW 需要哈希表
- 哈希表设计决策
- 创建 kernel/base/hashmap.h
- 创建 kernel/base/hashmap.c

### 核心篇：COW 实现（2篇）

### 4. [实现 COW 核心模块](./04_实现COW核心模块.md)
**COW 模块实现**
- 创建 kernel/mm/vmm/cow.h
- 创建 kernel/mm/vmm/cow.c
- COW 块结构设计
- 页管理 API 实现

### 5. [集成 COW 到页错误处理](./05_集成COW到页错误处理.md)
**页错误处理集成**
- 修改 kernel/mm/vmm/fault.c
- COW 页错误检测流程
- cow_handle_fault 完整实现

### 异常篇：异常处理机制（4篇）

### 6. [x86_64 异常处理机制基础](./06_x86_64异常处理机制基础.md)
**异常基础**
- Exception vs Interrupt
- x86_64 异常分类
- 错误码格式解析
- Double Fault/Stack Fault/GPF 概述

### 7. [GDT 与 TSS 详解](./07_GDT与TSS详解.md)
**GDT/TSS 机制**
- GDT 在 x86_64 中的作用
- TSS 结构和用途
- IST (Interrupt Stack Table) 机制
- 为什么需要 IST 栈

### 8. [实现 GDT 和 TSS 管理](./08_实现GDT和TSS管理.md)
**GDT/TSS 实现**
- 创建 kernel/interrupt/gdt.h
- 创建 kernel/interrupt/gdt.c
- 创建 kernel/interrupt/tss.h
- 创建 kernel/interrupt/tss.c
- 创建 kernel/interrupt/gdt.asm

### 9. [实现异常处理器](./09_实现异常处理器.md)
**异常处理器实现**
- 创建 kernel/interrupt/exception.h
- 创建 kernel/interrupt/exception.c
- Double Fault 处理器
- Stack Fault 处理器
- GPF 处理器与错误码解析

### 集成篇：内核集成与测试（2篇）

### 10. [内核集成与 COW 演示程序](./10_内核集成与COW演示程序.md)
**内核集成**
- 修改 kernel/kernel_init.c
- 创建 COW 演示程序
- 编译验证与测试

### 11. [测试验证与调试技巧](./11_测试验证与调试技巧.md)
**调试与验证**
- COW 功能测试方法
- 异常处理测试方法
- GDB 调试技巧
- 常见问题排查

---

## 快速开始

### 环境要求

```bash
# 检查工具
nasm -v          # NASM version 2.x.x
gcc --version    # gcc (Ubuntu xx.x.x.x) xx.x.x
cmake --version  # cmake version x.x.x
qemu-system-x86_64 --version  # QEMU emulator version x.x.x
```

### 切换到正确分支

```bash
cd /path/to/CCOperatingSystemX64

# 切换到 stage/18 分支
git checkout stage/18_cow_exception_handle

# 或者从 stage/17 开始
git checkout stage/17_kmalloc_kfree
```

### 预期输出

启动后应该能看到：

```
[COW] Initializing Copy-on-Write subsystem...
[COW] Initialized with 64 buckets
[EXC] Registering exception handlers...
[EXC] Exception handlers registered
[GDT] Setting up kernel GDT...
[GDT] GDT loaded at 0xXXXXX
[TSS] TSS initialized with IST stacks
```

---

## 关键代码文件

```
kernel/base/
├── hashmap.h              # 泛型哈希表接口
└── hashmap.c              # 泛型哈希表实现

kernel/mm/vmm/
├── cow.h                  # COW 模块接口
├── cow.c                  # COW 模块实现
├── fault.h                # 页错误处理接口 (修改)
└── fault.c                # 页错误处理实现 (修改)

kernel/interrupt/
├── gdt.h                  # GDT 管理接口
├── gdt.c                  # GDT 管理实现
├── gdt.asm                # GDT 汇编辅助函数
├── tss.h                  # TSS 管理接口
├── tss.c                  # TSS 管理实现
├── exception.h            # 异常处理接口
└── exception.c            # 异常处理实现

kernel/demo/cow/
├── cow_demo.h             # COW 演示接口
└── cow_demo.c             # COW 演示实现
```

---

## 技术要点

### 写时复制流程

```
fork() → 共享物理页 → 标记只读+COW
    ↓
写入操作 → 触发页错误
    ↓
检查 COW 标志 → 查找引用计数
    ↓
refcount = 1?     refcount > 1?
    ↓               ↓
设置写权限        分配新页+复制
    ↓               ↓
    恢复执行
```

### COW 页表标志

```c
#define COW_FLAG_MASK    (1ULL << 9)   // 使用第9位 (可用位)
```

### IST 栈配置

```c
#define IST_DF       1    // Double Fault 使用 IST1
#define IST_SS       4    // Stack Fault 使用 IST4
#define IST_STACK_SIZE  (16 * 1024)  // 16KB
```

---

## 常见问题

### Q: 为什么需要写时复制？

A: 传统的 fork() 会复制父进程的全部内存，这在大多数情况下是浪费的——因为 fork() 后通常紧接着 exec()，复制的内容会被丢弃。COW 让父子进程共享物理页，只有在写入时才真正复制，大大提高了 fork() 的效率。

### Q: 为什么使用哈希表而不是链表？

A: 哈希表的查找是 O(1)，而链表是 O(n)。当系统中有大量 COW 页时，哈希表能显著提高页错误处理的性能。我们使用链式法解决冲突，实现简单且效率高。

### Q: 什么是 Double Fault？

A: Double Fault（#DF）是当 CPU 在处理一个异常时又发生了另一个异常而触发的。这通常意味着栈已经损坏，是内核中的严重错误。我们为 Double Fault 配置了独立的 IST 栈，避免栈损坏导致 Triple Fault（系统重启）。

### Q: 为什么需要 GDT 和 TSS？

A: 虽然 x86_64 中分段功能被大大削弱，但 GDT 仍然是必需的——它用于代码段选择器和 TSS 加载。TSS 在 x86_64 中主要用于存储 IST 栈地址，为关键异常提供独立的栈空间。

---

## 下一步

完成本阶段后，你将掌握：

- ✅ 写时复制机制的设计与实现
- ✅ 泛型哈希表数据结构
- ✅ x86_64 异常处理机制
- ✅ GDT/TSS 管理
- ✅ IST 栈配置

下一阶段我们将进一步优化代码结构，为进程管理做准备。

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-20
