# CCOS 写时复制与异常处理 文档中心

本目录包含 CCOS Stage 18 - 写时复制(COW)与异常处理机制开发的完整文档体系。

---

## 阶段概述

**Stage 18: 写时复制与异常处理 (COW & Exception Handling)**

本阶段在 Stage 17 堆管理器基础上，实现了完整的写时复制(Copy-on-Write)内存管理机制和关键的异常处理器（GPF/Stack Fault/Double Fault），为操作系统的进程管理和错误恢复提供基础支持。

### 核心成果

- **写时复制模块** ([`kernel/mm/vmm/cow.h`](../../kernel/mm/vmm/cow.h))
  - 基于哈希表的 COW 页跟踪
  - 引用计数管理
  - COW 区域注册与注销
  - 页错误时自动处理 COW 写入
  - COW 统计信息收集

- **通用哈希表** ([`kernel/base/hashmap.h`](../../kernel/base/hashmap.h))
  - 泛型哈希表数据结构
  - 支持自定义哈希函数和比较函数
  - 链式冲突解决
  - 迭代器支持

- **异常处理模块** ([`kernel/interrupt/exception.h`](../../kernel/interrupt/exception.h))
  - Double Fault (#DF) 处理器
  - Stack Fault (#SS) 处理器
  - General Protection Fault (#GP) 处理器
  - 错误码解析与诊断
  - 异常统计信息

- **GDT/TSS 管理** ([`kernel/interrupt/gdt.h`](../../kernel/interrupt/gdt.h))
  - 内核 GDT 初始化与管理
  - TSS 结构管理
  - IST (Interrupt Stack Table) 栈配置
  - 用户态到内核态栈切换

- **增强的页错误处理** ([`kernel/mm/vmm/fault.h`](../../kernel/mm/vmm/fault.h))
  - COW 页错误处理集成
  - 写入故障自动检测
  - 按需分页支持框架

- **COW 演示程序** ([`kernel/demo/cow/cow_demo.h`](../../kernel/demo/cow/cow_demo.h))
  - COW 基本功能测试
  - COW 区域注册演示
  - COW 统计信息展示

---

## 目录结构

```
kernel/
├── base/
│   ├── hashmap.h              # 泛型哈希表接口
│   └── hashmap.c              # 泛型哈希表实现
├── mm/
│   └── vmm/
│       ├── cow.h              # COW 模块接口
│       ├── cow.c              # COW 模块实现
│       ├── fault.h            # 页错误处理接口 (增强)
│       └── fault.c            # 页错误处理实现 (增强)
├── interrupt/
│   ├── gdt.h                  # GDT 管理接口
│   ├── gdt.c                  # GDT 管理实现
│   ├── gdt.asm                # GDT 汇编辅助函数
│   ├── tss.h                  # TSS 管理接口
│   ├── tss.c                  # TSS 管理实现
│   ├── exception.h            # 异常处理接口
│   ├── exception.c            # 异常处理实现
│   ├── idt.h                  # IDT 接口 (修改)
│   └── idt.c                  # IDT 实现 (修改)
└── demo/
    └── cow/
        ├── cow_demo.h         # COW 演示接口
        └── cow_demo.c         # COW 演示实现
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要写时复制
- COW 设计决策（哈希表选择、引用计数管理）
- 异常处理机制设计
- GDT/TSS 在 x86_64 中的作用
- IST 栈配置原理
- 架构设计与模块协作
- 实现细节与关键技术
- 未来改进方向

**适合**:
- 理解设计思路
- 学习 COW 机制
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- 写时复制算法详解
- COW 页表标志位定义
- 哈希表 API 完整参考
- 异常错误码详解
- GDT/TSS 结构定义
- IST 配置与使用
- 数据结构定义
- 常量定义

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 问题诊断

**内容**:
- COW 页错误处理失败
- 异常处理器崩溃
- GDT/TSS 初始化问题
- IST 栈溢出
- 哈希表冲突异常
- 引用计数泄漏

每个问题按"症状-原因-解决方案"格式组织。

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

### 4. [调试工具指南](./调试工具指南.md) 工具使用

**内容**:
- COW 状态查看工具
- 异常统计信息查看
- GDT/TSS 检查命令
- 页表 COW 标志检查
- 哈希表状态调试
- 性能分析技巧

**适合**:
- 学习调试工具
- 查询具体命令
- 提升调试效率

---

## 快速开始

### 查看代码结构

1. **COW 模块接口** → 查看 [`kernel/mm/vmm/cow.h`](../../kernel/mm/vmm/cow.h)
2. **异常处理接口** → 查看 [`kernel/interrupt/exception.h`](../../kernel/interrupt/exception.h)
3. **GDT/TSS 接口** → 查看 [`kernel/interrupt/gdt.h`](../../kernel/interrupt/gdt.h)
4. **哈希表接口** → 查看 [`kernel/base/hashmap.h`](../../kernel/base/hashmap.h)

### 使用示例

```c
#include "mm/vmm/cow.h"
#include "mm/vmm/vmm.h"
#include "interrupt/exception.h"

// 内核初始化时调用
void kernel_init(void) {
    // 初始化哈希表系统
    // (COW 模块会自动初始化其内部哈希表)

    // 初始化 COW 子系统
    cow_init();

    // 初始化异常处理器
    exception_init();

    // GDT/TSS 在 IDT 初始化时自动初始化
}

// 注册 COW 区域
void cow_example(void) {
    physical_addr_t pml4 = vmm_get_current_pml4();
    virtual_addr_t base = 0x1000000;
    size_t size = 0x10000;  // 64KB

    // 注册为 COW 区域
    if (cow_register_region(pml4, base, size) == COW_OK) {
        // 现在写入该区域会触发 COW
        volatile uint32_t* ptr = (volatile uint32_t*)base;
        *ptr = 0xDEADBEEF;  // 触发页错误，自动处理 COW
    }

    // 注销 COW 区域
    cow_unregister_region(pml4, base);
}

// 手动 COW 页管理
void manual_cow_example(void) {
    physical_addr_t phys = 0x100000;

    // 添加到 COW 跟踪
    if (cow_add_page(phys) == COW_OK) {
        // 增加引用计数 (模拟多个映射)
        cow_inc_refcount(phys);
        cow_inc_refcount(phys);

        // 查询引用计数
        uint16_t refcount;
        cow_get_refcount(phys, &refcount);

        // 减少引用计数
        cow_dec_refcount(phys);
        cow_dec_refcount(phys);
        // refcount=0 时页面自动从 COW 跟踪中移除
    }
}

// 获取 COW 统计信息
void cow_stats_example(void) {
    cow_stats_t stats;
    if (cow_get_stats(&stats) == COW_OK) {
        klog_info("COW Faults Handled: %llu\n", stats.cow_faults_handled);
        klog_info("COW Pages Allocated: %llu\n", stats.cow_pages_allocated);
        klog_info("COW Current Blocks: %llu\n", stats.cow_current_blocks);
    }
}
```

### 哈希表使用示例

```c
#include "base/hashmap.h"

// 创建哈希表
void hashmap_example(void) {
    // 创建哈希表 (使用指针哈希函数)
    hashmap_t* map = hashmap_create(64, hash_ptr, eq_ptr);

    if (map) {
        // 插入键值对
        uint64_t key = 0x1000;
        uint64_t value = 0xDEADBEEF;
        hashmap_put(map, &key, &value);

        // 查找值
        void* result = hashmap_get(map, &key);
        if (result) {
            uint64_t found = *(uint64_t*)result;
        }

        // 删除键值对
        hashmap_remove(map, &key);

        // 获取大小
        size_t size = hashmap_size(map);

        // 销毁哈希表
        hashmap_destroy(map);
    }
}
```

### 异常处理示例

```c
#include "interrupt/exception.h"

// 获取异常统计信息
void exception_stats_example(void) {
    exception_stats_t stats;
    exception_get_stats(&stats);

    klog_info("Double Faults: %llu\n", stats.df_count);
    klog_info("Stack Faults: %llu\n", stats.ss_count);
    klog_info("GPF (User): %llu\n", stats.gp_user_count);
    klog_info("GPF (Kernel): %llu\n", stats.gp_kernel_count);
}

// 解析 GPF 错误码
void gpf_parse_example(uint64_t error_code) {
    gpf_error_info_t info;
    gp_parse_error_code(error_code, &info);

    if (info.external) {
        klog_info("External event\n");
    }
    if (info.idt_descriptor) {
        klog_info("IDT descriptor violation\n");
    }
    if (info.gdt_table) {
        klog_info("GDT/LDT violation, selector index: %u\n",
                  info.selector_index);
    }
}
```

---

## 与前一阶段对比

| 特性 | Stage 17 (kmalloc/kfree) | Stage 18 (COW 与异常处理) |
|------|-------------------------|---------------------------|
| 内存共享 | 无写时复制 | 完整 COW 支持 |
| 哈希表 | 无 | 泛型哈希表实现 |
| 异常处理 | 仅页错误 | GPF/SS/DF 处理器 |
| GDT/TSS | 使用 Bootloader | 内核完整管理 |
| IST 栈 | 无 | Double Fault/Stack Fault IST |
| 页错误 | 基础处理 | COW 集成处理 |
| 统计信息 | 堆统计 | COW + 异常统计 |
| 新增文件 | - | 15+ 个 |

---

## 技术亮点

### 1. 哈希表支持的 COW 跟踪

使用泛型哈希表高效管理 COW 页面：

```c
// COW 块结构
typedef struct cow_block {
    physical_addr_t    orig_phys;     // 原始物理页地址
    uint16_t           refcount;      // 引用计数 (1-COW_MAX_REFCOUNT)
} cow_block_t;

// 哈希表查找
cow_block_t* cow_lookup_block(physical_addr_t phys) {
    return hashmap_get(cow_state.page_map, &phys);
}
```

### 2. COW 页表标志

利用页表保留位存储 COW 标志：

```c
#define COW_FLAG_MASK    (1ULL << 9)   // 使用第9位 (可用位)

// 设置 COW 标志
static inline uint64_t cow_set_cow_flag(uint64_t pte_flags) {
    return pte_flags | COW_FLAG_MASK;
}

// 检查 COW 页
static inline bool cow_is_cow_page(uint64_t pte_flags) {
    return (pte_flags & COW_FLAG_MASK) != 0;
}
```

### 3. IST 栈配置

为关键异常配置独立栈：

```c
#define IST_DF       1    // Double Fault 使用 IST1
#define IST_SS       4    // Stack Fault 使用 IST4
#define IST_STACK_SIZE  (16 * 1024)  // 16KB

void tss_set_ist_stack(uint8_t ist_index, virtual_addr_t stack_top);
```

### 4. COW 写入处理流程

```
写入 COW 页 → 页错误 (#PF) → 检测 COW 标志
    ↓
查找 COW 块 → 检查引用计数
    ↓
refcount = 1?     refcount > 1?
    ↓               ↓
直接设置写权限    分配新页 + 复制内容
    ↓               ↓
恢复执行
```

### 5. 异常三层处理

```c
typedef enum {
    EXC_SUCCESS,           // 处理成功，可继续
    EXC_TERMINATE_PROCESS, // 应终止当前进程
    EXC_KERNEL_PANIC,      // 不可恢复，必须停机
} exception_result_t;
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

- **阶段**: Stage 18
- **分支**: `stage/18_cow_exception_handle`
- **日期**: 2026-02-18
- **作者**: Charliechen114514

---

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../17_kmalloc_kfree/README.md](../17_kmalloc_kfree/) - 上一阶段文档 (如存在)

### 源码文件
- [`kernel/mm/vmm/cow.h`](../../kernel/mm/vmm/cow.h) - COW 模块接口
- [`kernel/mm/vmm/fault.h`](../../kernel/mm/vmm/fault.h) - 页错误处理接口
- [`kernel/interrupt/exception.h`](../../kernel/interrupt/exception.h) - 异常处理接口
- [`kernel/interrupt/gdt.h`](../../kernel/interrupt/gdt.h) - GDT 管理接口
- [`kernel/interrupt/tss.h`](../../kernel/interrupt/tss.h) - TSS 管理接口
- [`kernel/base/hashmap.h`](../../kernel/base/hashmap.h) - 哈希表接口
- [`kernel/demo/cow/cow_demo.h`](../../kernel/demo/cow/cow_demo.h) - COW 演示

### 外部参考
- [Copy-on-Write](https://en.wikipedia.org/wiki/Copy-on-write)
- [x86_64 Exceptions](https://wiki.osdev.org/Exceptions)
- [GDT and TSS](https://wiki.osdev.org/GDT_TSS)
- [IST (Interrupt Stack Table)](https://wiki.osdev.org/Interrupt_Stack_Table)
- [OSDev.org](https://wiki.osdev.org)

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-18
