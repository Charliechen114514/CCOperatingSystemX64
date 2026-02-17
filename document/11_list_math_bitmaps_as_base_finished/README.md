# CCOS 链表、数学库与位图 文档中心

本目录包含 CCOS Stage 11 - 链表、数学库与位图开发的完整文档体系。

---

## 阶段概述

**Stage 11: 链表、数学库与位图作为基础数据结构**

本阶段实现了 CCOS 的核心基础数据结构库，包括 Linux 风格的侵入式双向链表、位操作数学库和位图数据结构。

### 核心成果

- **Linux 风格侵入式双向链表** ([`kernel/list/list.h`](../../kernel/list/list.h))
  - 侵入式链表节点设计 (`list_head`)
  - O(1) 插入、删除、移动操作
  - 安全的遍历宏，支持遍历过程中删除节点
  - 链表拼接和切割操作
  - 正向和反向遍历支持

- **位操作数学库** ([`kernel/math/bits.h`](../../kernel/math/bits.h))
  - 2的幂次检测和计算 (`is_power_of_2`, `round_up/down_to_power_of_2`)
  - 内存对齐操作 (`align_up`, `align_down`, `is_aligned`)
  - 整数除法舍入 (`div_round_up/down/nearest`)
  - 数学工具函数 (`min`, `max`, `abs`, `clamp`)

- **位图数据结构** ([`kernel/bitmap/bitmap.h`](../../kernel/bitmap/bitmap.h))
  - 单比特位操作 (`bitmap_set`, `bitmap_clear`, `bitmap_test`, `bitmap_flip`)
  - 范围操作 (`bitmap_set_range`, `bitmap_clear_range`)
  - 位扫描 (`bitmap_find_first/next_zero/set`)
  - 位图逻辑运算 (`bitmap_and`, `bitmap_or`, `bitmap_xor`, `bitmap_complement`)
  - 工具函数 (`bitmap_weight`, `bitmap_full`, `bitmap_empty`)

- **栈回溯支持** ([`kernel/stacktrace/stacktrace.h`](../../kernel/stacktrace/stacktrace.h))
  - 基于 RBP 的栈帧遍历
  - 符号解析支持
  - 美观的栈回溯输出

---

## 目录结构

```
kernel/
├── list/
│   ├── list.h               # 链表头文件（含内联函数和宏）
│   └── list.c               # 链表非内联函数实现
├── math/
│   ├── math.h               # 数学工具函数
│   ├── bits.h               # 位操作内联函数
│   └── math.c               # 数学函数实现
├── bitmap/
│   ├── bitmap.h             # 位图头文件
│   ├── bitmap.c             # 位图实现
│   └── bitmap_helper.h      # 位图辅助宏
└── stacktrace/
    ├── stacktrace.h         # 栈回溯头文件
    └── stacktrace.c         # 栈回溯实现
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 设计决策与架构思考
- 代码组织与模块化
- 常见问题与解决方案
- 未来改进方向

**适合**:
- 理解设计思路
- 学习系统架构
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- 数据结构与算法详解
- API 函数参考
- 侵入式链表原理
- 位图操作规范

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 问题诊断

**内容**:
- 常见编译错误
- 运行时问题诊断
- 内存访问异常
- 调试技巧

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

### 4. [调试工具指南](./调试工具指南.md) 工具使用

**内容**:
- GDB 调试数据结构
- 内存检查技巧
- 栈回溯分析方法
- 性能分析方法

**适合**:
- 学习调试工具
- 查询具体命令
- 提升调试效率

---

## 快速开始

### 查看代码结构

1. **链表实现** → 查看 [`kernel/list/list.h`](../../kernel/list/list.h)
2. **数学库** → 查看 [`kernel/math/bits.h`](../../kernel/math/bits.h)
3. **位图实现** → 查看 [`kernel/bitmap/bitmap.h`](../../kernel/bitmap/bitmap.h)

### 使用示例

#### 1. 侵入式链表

```c
#include "list/list.h"

// 定义包含链表节点的数据结构
typedef struct task {
    int pid;
    char name[32];
    list_head list;  // 链表节点嵌入数据结构
} task_t;

// 初始化链表头
LIST_HEAD(task_list);

// 创建任务
task_t task1 = { .pid = 1, .name = "init" };
INIT_LIST_HEAD(&task1.list);

// 添加到链表
list_add(&task1.list, &task_list);

// 遍历链表
task_t* pos;
list_for_each_entry(pos, &task_list, list) {
    klog_info("PID: %d, Name: %s\n", pos->pid, pos->name);
}
```

#### 2. 数学库

```c
#include "math/bits.h"
#include "math/math.h"

// 2的幂次检测
if (is_power_of_2(size)) {
    // size 是 2 的幂次
}

// 内存对齐
size_t aligned = align_up(ptr, 16);  // 16 字节对齐

// 除法向上取整
size_t pages = div_round_up(bytes, PAGE_SIZE);

// 数学工具
int val = clamp(x, 0, 100);  // 限制在 [0, 100] 范围内
```

#### 3. 位图

```c
#include "bitmap/bitmap.h"

// 定义位图缓冲区
uint8_t buffer[128];  // 1024 位
bitmap bm;

// 初始化位图
bitmap_init(&bm, buffer, 1024);

// 设置位
bitmap_set(&bm, 10);
bitmap_clear(&bm, 20);

// 测试位
if (bitmap_test(&bm, 10)) {
    // 第 10 位已设置
}

// 查找第一个零位
ssize_t idx = bitmap_find_first_zero(&bm);
if (idx >= 0) {
    bitmap_set(&bm, idx);  // 分配
}
```

---

## 技术亮点

### 1. 侵入式链表设计

链表节点直接嵌入数据结构中，避免额外的内存分配：

```c
typedef struct list_head {
    struct list_head* next;
    struct list_head* prev;
} list_head;
```

通过 `container_of` 宏获取包含链表节点的结构体：
```c
#define list_entry(ptr, type, member) \
    ((type*)((char*)(ptr) - (unsigned long)(&((type*)0)->member)))
```

### 2. 位操作优化

使用位操作实现高效计算：

- **2的幂次检测**: `n & (n - 1) == 0`
- **向上对齐**: `(value + alignment - 1) & ~(alignment - 1)`
- **除法向上取整**: `(n + d - 1) / d`

### 3. 位图高效实现

- 单字节多位操作
- Brian Kernighan 算法计算比特权重
- 逻辑运算支持复合操作

### 4. 栈回溯

- 基于 RBP 的栈帧遍历
- 符号解析支持
- 帧指针验证防止崩溃

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

- **阶段**: Stage 11
- **分支**: `stage/11_list_math_bitmaps_as_base_finished`
- **提交**:
  - `5b5451b` - finish the merge of stage11
  - `6f0c411` - stacktrace supports
  - `1d1f81e` - support dump stacks
- **日期**: 2026-02-17
- **作者**: Charliechen114514

---

## 相关资源

### 项目文档
- [../PROGRESS.md](../PROGRESS.md) - 项目进度
- [../10_base_with_format_log/README.md](../10_base_with_format_log/) - 上一阶段文档

### 源码文件
- [`kernel/list/list.h`](../../kernel/list/list.h)
- [`kernel/math/bits.h`](../../kernel/math/bits.h)
- [`kernel/bitmap/bitmap.h`](../../kernel/bitmap/bitmap.h)

### 外部参考
- [Linux Kernel List](https://kernel.org/doc/html/latest/core-api/kernel-api.html)
- [Bit Twiddling Hacks](https://graphics.stanford.edu/~seander/bithacks.html)

---

**作者**: Charliechen114514
**最后更新**: 2026-02-17
