# CCOS 虚拟内存管理与页错误处理 文档中心

本目录包含 CCOS Stage 16 - 虚拟内存管理与页错误处理开发的完整文档体系。

---

## 阶段概述

**Stage 16: 虚拟内存管理与页错误处理 (VMM & Page Faults)**

本阶段在 Stage 15 内存检测与物理帧管理基础上，实现了完整的虚拟内存管理系统（VMM）和页错误处理机制，为操作系统提供了高级内存管理能力。

### 核心成果

- **页表管理模块** ([`kernel/mm/vmm/page.h`](../../kernel/mm/vmm/page.h))
  - x86_64 四级页表结构 (PML4/PDPT/PD/PT) 管理
  - 页表项的创建、修改、查询
  - 支持标准 4KB 页和巨大页 (2MB/1GB)
  - 直接物理映射 (Direct Physical Map)
  - 虚拟地址到物理地址转换

- **虚拟内存管理器** ([`kernel/mm/vmm/vmm.h`](../../kernel/mm/vmm/vmm.h))
  - 高层虚拟内存分配 API
  - 内核空间与用户空间隔离
  - 内存区域跟踪与管理
  - 统计信息收集

- **页错误处理器** ([`kernel/mm/vmm/fault.h`](../../kernel/mm/vmm/fault.h))
  - 中断向量 14 (#PF) 处理
  - 错误码解析 (Present/Write/User/Reserved/Instr)
  - Copy-on-Write (COW) 支持框架
  - Demand Paging 支持框架
  - 内核态与用户态页错误区分处理

- **VMM 演示程序** ([`kernel/demo/vmm/vmm_demo.h`](../../kernel/demo/vmm/vmm_demo.h))
  - 页表统计信息展示
  - 地址转换测试
  - 页分配与映射演示
  - 用户地址空间创建
  - 巨大页映射测试

- **栈保护支持** ([`kernel/base/stack_check.c`](../../kernel/base/stack_check.c))
  - `__stack_chk_fail` 实现
  - 栈破坏检测与处理
  - VGA 直接错误输出

---

## 目录结构

```
kernel/
├── mm/
│   └── vmm/
│       ├── page.h                  # 页表管理接口
│       ├── page.c                  # 页表管理实现
│       ├── vmm.h                   # 虚拟内存管理接口
│       ├── vmm.c                   # 虚拟内存管理实现
│       ├── vmm_debug_config.h      # 调试配置
│       ├── vmm_debug.c             # 调试功能实现
│       ├── fault.h                 # 页错误处理接口
│       └── fault.c                 # 页错误处理实现
├── demo/
│   └── vmm/
│       ├── vmm_demo.h              # VMM 演示接口
│       └── vmm_demo.c              # VMM 演示实现
└── base/
    └── stack_check.c               # 栈保护支持
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要虚拟内存管理
- x86_64 页表机制详解
- 设计决策（地址空间布局、巨大页支持）
- 架构设计与模块协作
- 实现细节与关键技术
- 页错误处理策略
- 未来改进方向

**适合**:
- 理解设计思路
- 学习虚拟内存机制
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- x86_64 分页机制详解
- 页表项格式与标志位
- 虚拟地址空间布局
- 页错误码详解
- VMM API 完整参考
- 数据结构定义
- 常量定义

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 问题诊断

**内容**:
- 页表初始化失败
- 页映射无效
- 页错误处理异常
- 直接映射问题
- 巨大页分配失败
- 系统稳定性问题

每个问题按"症状-原因-解决方案"格式组织。

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

### 4. [调试工具指南](./调试工具指南.md) 工具使用

**内容**:
- QEMU Monitor 内存调试
- GDB 页表检查命令
- 页转储工具使用
- VMM 统计信息查看
- 性能分析技巧

**适合**:
- 学习调试工具
- 查询具体命令
- 提升调试效率

---

## 快速开始

### 查看代码结构

1. **页表管理接口** → 查看 [`kernel/mm/vmm/page.h`](../../kernel/mm/vmm/page.h)
2. **VMM 接口** → 查看 [`kernel/mm/vmm/vmm.h`](../../kernel/mm/vmm/vmm.h)
3. **页错误处理接口** → 查看 [`kernel/mm/vmm/fault.h`](../../kernel/mm/vmm/fault.h)

### 使用示例

```c
#include "mm/vmm/vmm.h"
#include "mm/vmm/page.h"
#include "mm/vmm/fault.h"

// 内核初始化时调用
void kernel_init(void) {
    // 初始化页表管理
    page_init();

    // 初始化虚拟内存管理器
    vmm_init();

    // 初始化页错误处理器
    pf_init();
}

// 分配虚拟页面
void alloc_example(void) {
    // 分配 4 个 4KB 页面
    virtual_addr_t vaddr = vmm_alloc_pages(4, VMAP_FLAG_WRITE);
    if (vaddr != 0) {
        // 写入数据
        volatile uint64_t* ptr = (volatile uint64_t*)vaddr;
        ptr[0] = 0xDEADBEEF;

        // 释放页面
        vmm_free_pages(vaddr, 4);
    }
}

// 映射物理内存
void map_physical_example(void) {
    // 映射 VGA 内存到虚拟地址空间
    physical_addr_t vga_phys = 0xB8000;
    virtual_addr_t vaddr = vmm_map_physical(vga_phys, VMAP_FLAG_WRITE);

    if (vaddr != 0) {
        // 通过虚拟地址访问 VGA
        volatile uint16_t* vga_ptr = (volatile uint16_t*)vaddr;
        vga_ptr[0] = (uint16_t)'A' | 0x0200;

        // 解除映射
        vmm_unmap_physical(vaddr);
    }
}

// 查询页表映射
void query_example(void) {
    virtual_addr_t vaddr = 0xFFFFFFFF80000000ULL;
    page_query_result_t result;

    if (page_query(page_get_pml4(), vaddr, &result) == PAGE_OK) {
        // 检查映射是否存在
        if (result.present) {
            // 获取物理地址和标志
            physical_addr_t paddr = result.phys_addr;
            uint64_t flags = result.flags;
        }
    }
}
```

### 创建用户地址空间

```c
#include "mm/vmm/vmm.h"
#include "mm/pframe/pframe.h"

// 创建新进程的用户地址空间
void create_user_process(void) {
    physical_addr_t user_pml4;

    // 创建用户地址空间
    if (vmm_create_user_space(&user_pml4) == VMM_OK) {
        // 分配物理页
        physical_addr_t paddr;
        if (pframe_alloc(&paddr) == PFRAME_OK) {
            // 映射到用户空间
            virtual_addr_t user_vaddr = 0x400000;  // 4MB
            vmm_map_to_user(user_pml4, user_vaddr, paddr,
                           1, VMAP_FLAG_WRITE | VMAP_FLAG_USER);
        }
    }
}
```

---

## 与前一阶段对比

| 特性 | Stage 15 (内存检测与物理帧) | Stage 16 (VMM 与页错误) |
|------|----------------------------|-------------------------|
| 内存管理 | 物理帧分配器 | 虚拟内存管理系统 |
| 页表支持 | 使用 Bootloader 创建 | 完整的四级页表管理 |
| 地址映射 | 仅身份映射 | 直接映射 + 动态映射 |
| 页大小 | 4KB | 4KB + 2MB + 1GB 巨大页 |
| 用户空间 | 不支持 | 用户地址空间框架 |
| 页错误 | 不处理 | 完整的 #PF 处理器 |
| 内存统计 | 物理帧统计 | 虚拟内存统计 |
| 新增文件 | - | 10+ 个 |

---

## 技术亮点

### 1. 直接物理映射

实现了内核虚拟地址空间到物理内存的直接映射：

```c
#define KERNEL_VIRT_BASE   0xFFFF800000000000ULL
#define PHYS_MAP_OFFSET    KERNEL_VIRT_BASE

static inline void* phys_to_virt(physical_addr_t phys) {
    return (void*)(phys + PHYS_MAP_OFFSET);
}
```

### 2. 巨大页支持

支持 2MB 和 1GB 巨大页以减少 TLB 压力：

```c
// 分配 2MB 巨大页
virtual_addr_t vaddr = vmm_alloc_pages(2, VMAP_FLAG_WRITE | VMAP_FLAG_HUGE_2MB);
```

### 3. 页错误码解析

详细解析页错误原因：

```c
typedef struct {
    virtual_addr_t fault_addr;
    uint64_t error_code;
    bool present;        // 页面是否存在
    bool write;          // 是否写操作
    bool user;           // 是否用户态
    bool reserved_bit;   // 保留位是否设置
    bool instruction_fetch;  // 是否取指令
} page_fault_info_t;
```

### 4. 地址空间布局

清晰的 x86_64 地址空间布局定义：

```
用户空间:   0x0000000000400000 - 0x00007FFFFFFFFFFF (128TB)
非规范空洞: 0x00007FFFFFFFFFFF - 0xFFFF800000000000
内核空间:   0xFFFF800000000000 - 0xFFFFFFFFFFFFFFFF (128TB)
  - 直接映射:  0xFFFF800000000000 - 0xFFFF800001000000 (256MB)
  - 内核代码:  0xFFFFFFFF80000000 - 0xFFFFFFFF80200000 (2MB)
  - 内核数据:  0xFFFFFFFF80200000 - 0xFFFFFFFF80400000 (2MB)
  - 内核堆:    0xFFFFFFFF81000000 - 0xFFFFFFFF89000000 (128MB)
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

- **阶段**: Stage 16
- **分支**: `stage/16_vmm_pagefaults`
- **日期**: 2026-02-18
- **作者**: Charliechen114514

---

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../15_memdetect_with_pframe/README.md](../15_memdetect_with_pframe/) - 上一阶段文档

### 源码文件
- [`kernel/mm/vmm/page.h`](../../kernel/mm/vmm/page.h) - 页表管理接口
- [`kernel/mm/vmm/vmm.h`](../../kernel/mm/vmm/vmm.h) - VMM 接口
- [`kernel/mm/vmm/fault.h`](../../kernel/mm/vmm/fault.h) - 页错误处理接口
- [`kernel/demo/vmm/vmm_demo.h`](../../kernel/demo/vmm/vmm_demo.h) - VMM 演示

### 外部参考
- [x86_64 Paging](https://wiki.osdev.org/Paging)
- [Page Faults](https://wiki.osdev.org/Exceptions#Page_Fault)
- [Huge Pages](https://wiki.osdev.org/Page_Tables#Huge_Pages)
- [OSDev.org](https://wiki.osdev.org)

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-18
