# CCOS 调度类框架 文档中心

本目录包含 CCOS Stage 22 - 调度类框架 (Scheduling Class Framework) 开发的完整文档体系。

---

## 阶段概述

**Stage 22: 调度类框架 (Scheduling Class Framework)**

本阶段在 Stage 21 简单进程管理基础上，实现了模块化的调度类框架。灵感来源于 Linux 的调度器架构，该框架允许同时运行多种调度算法，并可轻松扩展新的调度策略。实现了 Round-Robin 和 Priority 两种调度类，为不同类型的进程提供不同的调度策略。

### 核心成果

- **调度类框架** ([`kernel/process/sched.h`](../../kernel/process/sched.h))
  - 模块化调度架构
  - 调度类抽象接口（虚函数表模式）
  - per-policy 运行队列管理
  - 时间片管理框架
  - 调度类注册机制

- **Round-Robin 调度类** ([`kernel/process/sched_rr.h`](../../kernel/process/sched_rr.h))
  - 简单 FIFO 调度
  - 固定时间片（10ms）
  - 公平的 CPU 分配
  - 适合普通进程

- **Priority 调度类** ([`kernel/process/sched_prio.h`](../../kernel/process/sched_prio.h))
  - 128 级优先级（0-127）
  - Active/Expired 队列机制
  - 优先级抢占支持
  - 动态时间片分配

- **进程管理扩展** ([`kernel/process/process.h`](../../kernel/process/process.h))
  - sched_entity_t 调度实体
  - 调度策略集成
  - 多策略共存

- **调度器演示** ([`kernel/demo/sched/sched_demo.h`](../../kernel/demo/sched/sched_demo.h))
  - RR 调度测试
  - Priority 调度测试
  - 单元测试框架

---

## 目录结构

```
kernel/
├── process/
│   ├── sched.h                  # 调度类框架接口
│   ├── sched.c                  # 调度类框架实现
│   ├── sched_rr.h               # Round-Robin 调度类接口
│   ├── sched_rr.c               # Round-Robin 调度类实现
│   ├── sched_prio.h             # Priority 调度类接口
│   ├── sched_prio.c             # Priority 调度类实现
│   ├── process.h                # 进程管理接口（已扩展）
│   ├── process.c                # 进程管理实现（已集成）
│   └── CMakeLists.txt           # 构建配置（已更新）
└── demo/
    └── sched/
        ├── sched_demo.h         # 调度器演示接口
        ├── sched_demo.c         # 调度器演示实现
        └── CMakeLists.txt       # 演示构建配置
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要调度类框架
- 调度类设计基础（虚函数表、调度实体）
- 设计决策（模块化架构、多策略共存）
- 架构设计（类注册、任务调度流程）
- 实现细节（task_tick、pick_next_task、抢占）
- 常见陷阱（时间片管理、优先级反转）
- 未来改进（CFS、多核支持）

**适合**:
- 理解设计思路
- 学习调度器架构
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- 调度策略枚举定义
- 调度类 API 完整参考
- Round-Robin 类 API
- Priority 类 API
- 数据结构定义（sched_class_t、sched_rq_t、sched_entity_t）
- 常量定义
- 算法说明

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 问题诊断

**内容**:
- 初始化问题（类注册失败、队列未初始化）
- 调度问题（任务不被调度、时间片异常）
- 抢占问题（高优先级不抢占、标志位未设置）
- 内存问题（class_data 分配、空指针）
- 调试技巧（GDB 命令、日志分析）

每个问题按"症状-原因-解决方案"格式组织。

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

## 快速开始

### 查看代码结构

1. **调度类框架接口** → 查看 [`kernel/process/sched.h`](../../kernel/process/sched.h)
2. **调度类框架实现** → 查看 [`kernel/process/sched.c`](../../kernel/process/sched.c)
3. **Round-Robin 调度类** → 查看 [`kernel/process/sched_rr.h`](../../kernel/process/sched_rr.h)
4. **Priority 调度类** → 查看 [`kernel/process/sched_prio.h`](../../kernel/process/sched_prio.h)
5. **演示程序** → 查看 [`kernel/demo/sched/sched_demo.h`](../../kernel/demo/sched/sched_demo.h)

### 使用示例

```c
#include "process/sched.h"
#include "process/sched_rr.h"
#include "process/sched_prio.h"

// 内核初始化时调用
void kernel_init(void) {
    // 1. 初始化调度类框架
    sched_class_init();

    // 2. 注册 Round-Robin 调度类
    sched_rr_init();

    // 3. 注册 Priority 调度类
    sched_prio_init();
}

// 为新进程设置调度策略
int32_t proc_fork(void) {
    pcb_t* child = proc_alloc_pcb();

    // 使用默认的 Round-Robin 调度
    sched_set_policy(child, SCHED_NORMAL, 0);

    // 或使用 Priority 调度
    // sched_set_policy(child, SCHED_PRIORITY, 50);  // priority = 50

    sched_enqueue_task(child, false);
    // ...
}
```

### 调度类选择流程

```
                    ┌─────────────────────┐
                    │   创建新进程         │
                    └──────────┬──────────┘
                               │
                    ┌──────────▼──────────┐
                    │  选择调度策略        │
                    └──────────┬──────────┘
                               │
               ┌───────────────┼───────────────┐
               │               │               │
               ▼               ▼               ▼
        ┌─────────────┐ ┌─────────────┐ ┌─────────────┐
        │ SCHED_NORMAL│ │SCHED_PRIORITY│ │  (未来扩展)  │
        │             │ │              │ │             │
        │ Round-Robin │ │   Priority   │ │     CFS     │
        │             │ │              │ │             │
        │ • 公平调度  │ │ • 优先级抢占 │ │ • 完全公平  │
        │ • 10ms时间片│ │ • 128级优先级│ │ • 红黑树    │
        └─────────────┘ └─────────────┘ └─────────────┘
               │               │               │
               └───────────────┼───────────────┘
                               ▼
                    ┌─────────────────────┐
                    │   sched_enqueue_task │
                    └─────────────────────┘
```

### 调度类虚函数表

```c
typedef struct sched_class {
    const char* name;              // 类名称
    sched_policy_t policy;         // 策略标识

    // 任务入队
    void (*enqueue_task)(struct sched_rq* rq, struct pcb* pcb, bool head);

    // 任务出队
    void (*dequeue_task)(struct sched_rq* rq, struct pcb* pcb);

    // 选择下一个任务
    struct pcb* (*pick_next_task)(struct sched_rq* rq, struct pcb* prev);

    // 抢占判断
    bool (*should_preempt)(struct pcb* p, struct pcb* curr);

    // 时间片处理（定时器回调）
    void (*task_tick)(struct sched_rq* rq, struct pcb* pcb);

    // 任务 fork 回调
    void (*task_fork)(struct pcb* pcb, int nice);

    // 获取时间片
    uint32_t (*get_time_slice)(const struct pcb* pcb);
} sched_class_t;
```

---

## 与前一阶段对比

| 特性 | Stage 21 (简单进程管理) | Stage 22 (调度类框架) |
|------|------------------------|----------------------|
| 调度架构 | 单一简单轮转 | 模块化调度类框架 |
| 调度算法 | 固定 Round-Robin | RR + Priority（可扩展） |
| 运行队列 | 单一全局队列 | per-policy 运行队列 |
| 时间片管理 | 简单递减 | 类定义时间片策略 |
| 抢占机制 | 无 | 优先级抢占支持 |
| 调度实体 | 简单 run_list | 完整 sched_entity_t |
| 优先级 | 不支持 | 128 级优先级 |
| Active/Expired | 无 | Priority 类支持 |
| 扩展性 | 低 | 高（虚函数表） |
| 新增文件 | - | 8 个 |

---

## 技术亮点

### 1. 模块化调度架构

借鉴 Linux 的调度类设计，使用虚函数表实现多态：

```c
// 每个调度类实现相同的接口
static sched_class_t rr_sched_class = {
    .name            = "RR",
    .policy          = SCHED_NORMAL,
    .enqueue_task    = rr_enqueue_task,
    .dequeue_task    = rr_dequeue_task,
    .pick_next_task  = rr_pick_next_task,
    .should_preempt  = rr_should_preempt,
    .task_tick       = rr_task_tick,
    .task_fork       = rr_task_fork,
    .get_time_slice  = rr_get_time_slice,
};

// 通过函数指针调用
pcb->sched_entity.sched_class->task_tick(rq, pcb);
```

### 2. Per-Policy 运行队列

每个调度策略维护独立的运行队列：

```c
typedef struct sched_rq {
    sched_class_t* sched_class;    // 关联的调度类
    list_head        queue;         // 运行队列
    uint32_t         nr_running;    // 运行任务数
    void*            class_data;    // 类特定数据
} sched_rq_t;

// 全局运行队列数组
static sched_rq_t s_run_queues[SCHED_MAX];
```

### 3. 优先级 Active/Expired 队列

Priority 调度类使用两级队列保证公平性：

```c
typedef struct prio_rq_data {
    list_head    active[PRIO_LEVELS];   // 活跃队列
    list_head    expired[PRIO_LEVELS];  // 过期队列
    uint32_t     nr_active;
    uint32_t     nr_expired;
    int          highest_prio;
} prio_rq_data_t;

// 时间片用完后，任务从 active 移到 expired
// 所有 active 队列为空时，交换 active/expired
```

### 4. 调度实体集成

PCB 中嵌入调度实体：

```c
typedef struct pcb {
    // ... 其他字段 ...

    sched_entity_t sched_entity;   // 调度实体

    // ...
} pcb_t;

typedef struct sched_entity {
    sched_policy_t    policy;         // 调度策略
    sched_class_t*    sched_class;    // 调度类指针
    uint32_t          time_slice;     // 剩余时间片
    uint32_t          time_slice_total;
    int               priority;       // 优先级
    int               nice;
    uint64_t          last_ran;
    list_head         run_list;
} sched_entity_t;
```

### 5. 任务选择流程

```
                    sched_pick_next_task()
                            │
                            ▼
                ┌───────────────────────┐
                │ 当前进程还有时间片？   │
                └───────────┬───────────┘
                            │ 是
                            ▼
                ┌───────────────────────┐
                │ 有高优先级任务等待？   │
                └───────────┬───────────┘
                            │ 是                    │ 否
                            ▼                      ▼
                    返回高优先级任务          保持当前进程

                            │ 否
                            ▼
                ┌───────────────────────┐
                │ 检查 Priority 队列     │
                └───────────┬───────────┘
                            │ 有任务              │ 无
                            ▼                    ▼
                    返回最高优先级任务    检查 RR 队列

                                                    │ 有任务          │ 无
                                                    ▼                ▼
                                            返回 RR 队列首任务   返回 idle
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
                    │ 调度类框架   │
                    │ sched/       │
                    └─────────────┘
```

---

## 版本信息

- **阶段**: Stage 22
- **分支**: `stage/22_sched_class`
- **日期**: 2026-02-20
- **作者**: CharlieChen114514

---

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../21_process_simple/README.md](../21_process_simple/) - 上一阶段文档

### 源码文件
- [`kernel/process/sched.h`](../../kernel/process/sched.h) - 调度类框架接口
- [`kernel/process/sched.c`](../../kernel/process/sched.c) - 调度类框架实现
- [`kernel/process/sched_rr.h`](../../kernel/process/sched_rr.h) - RR 调度类接口
- [`kernel/process/sched_rr.c`](../../kernel/process/sched_rr.c) - RR 调度类实现
- [`kernel/process/sched_prio.h`](../../kernel/process/sched_prio.h) - Priority 调度类接口
- [`kernel/process/sched_prio.c`](../../kernel/process/sched_prio.c) - Priority 调度类实现
- [`kernel/process/process.h`](../../kernel/process/process.h) - 进程管理接口
- [`kernel/demo/sched/sched_demo.h`](../../kernel/demo/sched/sched_demo.h) - 演示程序

### 外部参考
- [Linux CFS Scheduler](https://www.kernel.org/doc/html/latest/scheduler/sched-design-CFS.html)
- [Linux Scheduling Classes](https://elixir.bootlin.com/linux/v6.0/source/kernel/sched)
- [OSTEP Scheduling](https://github.com/ostep/ostep-projects/blob/master/scheduling-intro)
- [OSDev.org Scheduling](https://wiki.osdev.org/Scheduling_Algorithms)
