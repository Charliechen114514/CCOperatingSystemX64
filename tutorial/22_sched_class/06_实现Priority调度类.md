# 实现 Priority 调度类 —— Stage 22 调度类框架实战指南（六）

## 前言

前面我们实现了 RR 调度类，系统已经可以公平地分配 CPU 时间了。但 RR 有个明显的局限：所有进程平等，无法区分优先级。系统关键进程和普通用户进程用同样的调度策略，这显然不够理想。

比如说，处理键盘输入的进程需要快速响应，否则用户会觉得系统卡顿。后台编译任务则不需要太快的响应，只要能获得足够的 CPU 时间完成工作就行。这两种进程应该用不同的调度策略。

Priority 调度类就是为了解决这个问题。它支持 128 级优先级（0-127），高优先级任务可以抢占低优先级任务。为了保证公平性，还引入了 Active/Expired 队列机制，防止高优先级任务饿死低优先级任务。

说实话，Priority 调度类是整个框架中最复杂的部分。它有更复杂的数据结构（256 个队列），更复杂的选择逻辑（优先级判断），更复杂的时间片管理（动态计算）。但只要理解了它的设计思想，代码其实不难看懂。

---

## 现在我们要做什么

这篇文章的目标是实现完整的 Priority 调度类：

1. 创建 `sched_prio.h`，定义优先级常量和数据结构
2. 创建 `sched_prio.c`，实现所有调度类操作
3. 实现 Active/Expired 队列机制
4. 实现优先级抢占判断
5. 实现动态时间片分配
6. 注册 Priority 调度类到框架

---

## 从0开始 —— Priority 调度的设计思路

### 为什么需要优先级调度

优先级调度的应用场景很明确：系统关键进程需要更快的响应。比如：

- **中断处理下半部**：网络包到达后，协议栈处理需要快速完成
- **设备驱动**：音频播放需要及时填充缓冲区，否则会卡顿
- **系统服务**：键盘输入、鼠标事件需要快速响应

这些任务都有一个共同点：延迟敏感，而且工作量不大。如果它们和普通计算任务排同一个队，用户体验会很差。

### 优先级范围

我们定义了 128 个优先级（0-127）：

```c
#define PRIO_MAX      0      /* 最高优先级 */
#define PRIO_MIN      127    /* 最低优先级 */
#define PRIO_DEFAULT  64     /* 默认优先级 */
```

采用"数值越小优先级越高"的约定，这与 Linux 一致。这样设计是因为我们经常用数组索引来表示优先级，小索引（高优先级）先处理更自然。

### Active/Expired 队列机制

优先级调度有个明显的问题：如果高优先级任务一直存在，低优先级任务可能永远得不到运行机会，这就是"饥饿"问题。

Linux O(1) 调度器引入了 Active/Expired 队列机制来解决这个问题：

- **Active 队列**：当前可运行的任务队列
- **Expired 队列**：时间片用完的任务队列

调度器总是从 Active 队列中选择任务。当一个任务的时间片用完后，它会被移到 Expired 队列。当所有 Active 队列都空了，交换 Active 和 Expired 队列。

这样，每个任务都能获得运行机会，不会永久饥饿。

---

## 创建 sched_prio.h

### 文件结构

`kernel/process/sched_prio.h` 定义了 Priority 调度类的接口：

```c
/* ==============================================================================
 * CCOS - Priority Scheduling Class (System Processes Only)
 * ==============================================================================
 * This module implements a priority-based scheduling algorithm.
 * Higher priority (lower number) tasks run first.
 * Uses active/expired queue mechanism for fairness.
 * ==============================================================================
 */

#pragma once

#include "process/sched.h"
```

### 优先级常量

```c
/* ==============================================================================
 * Priority Constants
 * ==============================================================================
 */

/**
 * @brief Priority levels
 * Higher numeric value = lower priority (Linux-style)
 * Range: 0-127
 */
#define PRIO_MAX      0      /* Highest priority */
#define PRIO_MIN      127    /* Lowest priority */
#define PRIO_DEFAULT  64     /* Default priority */

/* Number of priority levels */
#define PRIO_LEVELS   128
```

### 动态时间片计算

不同优先级的任务应该有不同的时间片。高优先级任务需要更长的时间片，因为它们通常处理重要但短暂的工作：

```c
/**
 * @brief Time slice for each priority level (in ticks, at 1000Hz)
 *
 * Priority 0 (highest): 100ms - for init/kernel tasks that need to complete work
 * Priority 1-63: 50ms - high priority user tasks
 * Priority 64-127: 20ms - normal/low priority tasks
 *
 * Note: Higher priority (lower number) gets LONGER time slice because
 * these tasks are more important and should complete their work faster.
 */
#define PRIO_TIMESLICE(prio) \
    ((prio) == 0 ? 100 : \
     (prio) < 64 ? 50 : 20)
```

这是一个宏函数，根据优先级返回不同的时间片长度。优先级 0 获得最长的 100ms，优先级 1-63 获得 50ms，优先级 64-127 获得 20ms。

### 运行队列数据结构

Priority 调度类需要复杂的数据结构来存储 256 个队列（128 个 Active + 128 个 Expired）：

```c
/* ==============================================================================
 * Priority Run Queue Data
 * ==============================================================================
 */

/**
 * @brief Per-priority queue data for priority scheduler
 */
typedef struct prio_rq_data {
    list_head    active[PRIO_LEVELS];   /* Active queues per priority */
    list_head    expired[PRIO_LEVELS];  /* Expired queues per priority */
    uint32_t     nr_active;             /* Total active tasks */
    uint32_t     nr_expired;            /* Total expired tasks */
    int          highest_prio;          /* Current highest priority */
    bool         active_expired;        /* Whether to swap arrays */
} prio_rq_data_t;
```

`active[]` 和 `expired[]` 都是 128 个元素的数组，每个元素是一个链表头。`nr_active` 和 `nr_expired` 分别记录两个队列中的任务总数。`highest_prio` 缓存了当前 Active 队列中最高优先级（最小数值），加速查找。

### API 函数声明

```c
/* ==============================================================================
 * Priority Class API
 * ==============================================================================
 */

/**
 * @brief Initialize the Priority scheduling class
 * @return 0 on success, negative on error
 */
int sched_prio_init(void);

/**
 * @brief Get the Priority scheduling class structure
 * @return Pointer to the Priority class
 */
sched_class_t* sched_prio_get_class(void);
```

---

## 实现 sched_prio.c

### 文件头部与包含

```c
/* ==============================================================================
 * CCOS - Priority Scheduling Class Implementation
 * ==============================================================================
 */

#include "process/sched_prio.h"
#include "assert/assert.h"
#include "base/memory.h"
#include "klogs/kprintf.h"
#include "mm/heap/heap.h"
#include "process/process.h"
#include "process/sched.h"
```

注意这里包含了 `heap.h`，因为 Priority 调度类需要动态分配 `prio_rq_data_t` 结构体。

### 前向声明与函数声明

```c
/* ==============================================================================
 * Forward Declarations
 * ==============================================================================
 */

struct pcb;
struct sched_rq;

/* ==============================================================================
 * Priority Class Operations
 * ==============================================================================
 */

static void prio_enqueue_task(struct sched_rq* rq, struct pcb* pcb, bool head);
static void prio_dequeue_task(struct sched_rq* rq, struct pcb* pcb);
static struct pcb* prio_pick_next_task(struct sched_rq* rq, struct pcb* prev);
static bool prio_should_preempt(struct pcb* p, struct pcb* curr);
static void prio_task_tick(struct sched_rq* rq, struct pcb* pcb);
static void prio_task_fork(struct pcb* pcb, int nice);
static uint32_t prio_get_time_slice(const struct pcb* pcb);
```

### Priority 调度类结构

```c
/* ==============================================================================
 * Priority Class Structure
 * ==============================================================================
 */

static sched_class_t prio_sched_class = {
    .name = "PRIO",
    .policy = SCHED_PRIORITY,
    .enqueue_task = prio_enqueue_task,
    .dequeue_task = prio_dequeue_task,
    .pick_next_task = prio_pick_next_task,
    .should_preempt = prio_should_preempt,
    .task_tick = prio_task_tick,
    .task_fork = prio_task_fork,
    .get_time_slice = prio_get_time_slice,
};
```

---

## 辅助函数

### 获取运行队列数据

这是一个简单的辅助函数，从 `sched_rq` 的 `class_data` 字段获取 `prio_rq_data_t` 指针：

```c
/* ==============================================================================
 * Helper Functions
 * ==============================================================================
 */

/**
 * @brief Get the run queue data for priority scheduler
 */
static inline prio_rq_data_t* prio_rq_data(struct sched_rq* rq) {
    return (prio_rq_data_t*)rq->class_data;
}
```

### 查找最高优先级

这个函数遍历 Active 队列，找到有任务的最高优先级（最小数值）：

```c
/**
 * @brief Find the highest priority with active tasks
 */
static int find_highest_prio(prio_rq_data_t* data) {
    for (int i = 0; i < PRIO_LEVELS; i++) {
        if (!list_is_empty(&data->active[i])) {
            return i;
        }
    }
    return PRIO_MIN;
}
```

注意这里从 0 开始遍历，找到第一个非空队列就返回。这保证了总是返回最高优先级（最小数值）。

### 交换 Active 和 Expired 队列

当所有 Active 队列都空了，需要交换 Active 和 Expired：

```c
/**
 * @brief Swap active and expired arrays
 */
static void prio_swap_active_expired(prio_rq_data_t* data) {
    for (int i = 0; i < PRIO_LEVELS; i++) {
        list_head temp;
        INIT_LIST_HEAD(&temp);

        /* Swap active[i] with expired[i] */
        list_splice_init(&data->active[i], &temp);
        list_splice_init(&data->expired[i], &data->active[i]);
        list_splice_init(&temp, &data->expired[i]);
    }

    /* Swap counters */
    uint32_t temp_count = data->nr_active;
    data->nr_active = data->nr_expired;
    data->nr_expired = temp_count;

    data->highest_prio = find_highest_prio(data);
    data->active_expired = false;
}
```

`list_splice_init()` 把一个链表的内容移动到另一个链表，同时清空源链表。我们使用一个临时 `temp` 链表来完成三步交换。

---

## 入队操作实现

### prio_enqueue_task

Priority 的入队操作比 RR 复杂得多：

```c
/**
 * @brief Enqueue a task on the priority run queue
 */
static void prio_enqueue_task(struct sched_rq* rq, struct pcb* pcb, bool head) {
    prio_rq_data_t* data = prio_rq_data(rq);
    int prio = pcb->sched_entity.priority;

    /* Clamp priority to valid range */
    if (prio < PRIO_MAX) {
        prio = PRIO_MAX;
    } else if (prio > PRIO_MIN) {
        prio = PRIO_MIN;
    }

    /* If time slice is exhausted, enqueue to expired queue instead of active.
     * This handles the case where a task's time slice expired while it was running. */
    bool to_expired = (pcb->sched_entity.time_slice == 0);

    if (to_expired) {
        /* Enqueue to expired queue */
        if (head) {
            list_add(&pcb->sched_entity.run_list, &data->expired[prio]);
        } else {
            list_add_tail(&pcb->sched_entity.run_list, &data->expired[prio]);
        }
        data->nr_expired++;
    } else {
        /* Enqueue to active queue */
        if (head) {
            list_add(&pcb->sched_entity.run_list, &data->active[prio]);
        } else {
            list_add_tail(&pcb->sched_entity.run_list, &data->active[prio]);
        }
        data->nr_active++;

        /* Update highest priority if needed */
        if (prio < data->highest_prio) {
            data->highest_prio = prio;
        }
    }

    rq->nr_running++;
}
```

这里的逻辑是：

1. 首先把优先级限制在有效范围 [0, 127]
2. 如果时间片已用完（`time_slice == 0`），加入 Expired 队列
3. 否则加入 Active 队列
4. 更新相应的计数器

---

## 出队操作实现

### prio_dequeue_task

```c
/**
 * @brief Dequeue a task from the priority run queue
 */
static void prio_dequeue_task(struct sched_rq* rq, struct pcb* pcb) {
    prio_rq_data_t* data = prio_rq_data(rq);

    /* Remove from whichever queue it's on */
    list_del_init(&pcb->sched_entity.run_list);

    /* We need to determine which queue the task was on.
     * Since we can't tell after removing it, we use a heuristic:
     * If time_slice is 0, it was likely on expired queue (just finished).
     * Otherwise, it was likely on active queue. */
    if (pcb->sched_entity.time_slice == 0 && data->nr_expired > 0) {
        data->nr_expired--;
    } else if (data->nr_active > 0) {
        data->nr_active--;
    }

    rq->nr_running--;

    /* Update highest priority */
    data->highest_prio = find_highest_prio(data);
}
```

这里有个技巧：我们从链表中删除节点后，无法知道它原来在哪个队列上。我们使用一个启发式规则：如果时间片是 0，说明它刚用完时间片，很可能在 Expired 队列中。

这不是 100% 准确，但在实际使用中足够可靠。更严格的实现需要为 `sched_entity` 添加一个标志位记录它在哪个队列上。

---

## 任务选择实现

### prio_pick_next_task

这是 Priority 调度类最复杂的函数：

```c
/**
 * @brief Pick the next task from priority queue
 */
static struct pcb* prio_pick_next_task(struct sched_rq* rq, struct pcb* prev) {
    (void)prev; /* Not used for priority selection */
    prio_rq_data_t* data = prio_rq_data(rq);

    /* If no active tasks, try to swap arrays */
    if (data->nr_active == 0) {
        if (data->nr_expired > 0) {
            prio_swap_active_expired(data);
        } else {
            return NULL; /* No tasks at all */
        }
    }

    /* Get highest priority queue */
    int highest = find_highest_prio(data);
    if (highest >= PRIO_LEVELS) {
        return NULL;
    }

    /* Return first task from highest priority queue */
    struct pcb* next = list_first_entry(&data->active[highest], struct pcb, sched_entity.run_list);
    return next;
}
```

逻辑是：

1. 如果没有 Active 任务，尝试交换 Active 和 Expired 队列
2. 如果还是没有任务，返回 NULL
3. 找到最高优先级（最小数值）
4. 返回该优先级队列的第一个任务

---

## 抢占判断实现

### prio_should_preempt

Priority 调度支持抢占：

```c
/**
 * @brief Check if a task should preempt
 * Higher priority (lower number) always preempts lower priority
 */
static bool prio_should_preempt(struct pcb* p, struct pcb* curr) {
    /* Higher priority (lower number) preempts */
    if (p->sched_entity.priority < curr->sched_entity.priority) {
        return true;
    }

    /* Same priority: only if current exhausted time slice */
    if (p->sched_entity.priority == curr->sched_entity.priority) {
        return (curr->sched_entity.time_slice == 0);
    }

    return false;
}
```

抢占规则是：

1. 更高优先级（更小数值）的任务总是可以抢占
2. 相同优先级的任务，只有当前任务时间片用完时才切换

---

## 时间片处理实现

### prio_task_tick

```c
/**
 * @brief Handle timer tick for priority task
 */
static void prio_task_tick(struct sched_rq* rq, struct pcb* pcb) {
    (void)rq; /* Not used for priority scheduling */

    /* Decrement time slice */
    if (pcb->sched_entity.time_slice > 0) {
        pcb->sched_entity.time_slice--;
    }

    /* If time slice expired, move to expired queue */
    /* This is done in prio_enqueue_task when the task is re-enqueued */
}
```

注意这里我们只是递减时间片，不移到 Expired 队列。移到 Expired 的操作会在任务被重新入队时，根据 `time_slice == 0` 来判断。

---

## 任务初始化实现

### prio_task_fork

```c
/**
 * @brief Initialize a new task for priority scheduling
 */
static void prio_task_fork(struct pcb* pcb, int nice) {
    pcb->sched_entity.sched_class = &prio_sched_class;
    pcb->sched_entity.policy = SCHED_PRIORITY;

    /* Set priority from nice value or use default */
    if (nice < 0) {
        pcb->sched_entity.priority = PRIO_DEFAULT + nice;
        if (pcb->sched_entity.priority < PRIO_MAX) {
            pcb->sched_entity.priority = PRIO_MAX;
        }
    } else {
        pcb->sched_entity.priority = PRIO_DEFAULT;
    }

    /* Clamp priority range */
    if (pcb->sched_entity.priority < PRIO_MAX) {
        pcb->sched_entity.priority = PRIO_MAX;
    } else if (pcb->sched_entity.priority > PRIO_MIN) {
        pcb->sched_entity.priority = PRIO_MIN;
    }

    /* Set time slice based on priority */
    pcb->sched_entity.time_slice = PRIO_TIMESLICE(pcb->sched_entity.priority);
    pcb->sched_entity.time_slice_total = pcb->sched_entity.time_slice;
    pcb->sched_entity.nice = nice;
}
```

这个函数根据 `nice` 参数设置优先级，然后根据优先级计算时间片。

---

## 时间片获取实现

### prio_get_time_slice

```c
/**
 * @brief Get time slice for priority task
 */
static uint32_t prio_get_time_slice(const struct pcb* pcb) {
    return PRIO_TIMESLICE(pcb->sched_entity.priority);
}
```

直接使用宏函数 `PRIO_TIMESLICE()` 根据优先级计算时间片。

---

## 调度类注册

### sched_prio_init

Priority 调度类的初始化比 RR 复杂，因为需要动态分配 `prio_rq_data_t`：

```c
/* ==============================================================================
 * Priority Class Registration
 * ==============================================================================
 */

/**
 * @brief Initialize the Priority scheduling class
 */
int sched_prio_init(void) {
    klog_info("[SCHED] Initializing Priority scheduling class\n");

    /* Allocate per-policy run queue data */
    prio_rq_data_t* data = (prio_rq_data_t*)kmalloc(sizeof(prio_rq_data_t));
    if (!data) {
        klog_error("[SCHED] Failed to allocate priority queue data\n");
        return -1;
    }

    /* Initialize queues */
    for (int i = 0; i < PRIO_LEVELS; i++) {
        INIT_LIST_HEAD(&data->active[i]);
        INIT_LIST_HEAD(&data->expired[i]);
    }
    data->nr_active = 0;
    data->nr_expired = 0;
    data->highest_prio = PRIO_MIN;
    data->active_expired = false;

    extern scheduler_t scheduler;

    /* Get the run queue for priority scheduling and set class_data */
    sched_rq_t* rq = &scheduler.rq[SCHED_PRIORITY];

    rq->class_data = data;

    /* Register the class */
    int ret = sched_class_register(&prio_sched_class, SCHED_PRIORITY);
    if (ret != 0) {
        klog_error("[SCHED] Failed to register Priority class\n");
        kfree(data);
        return ret;
    }

    klog_info("[SCHED] Priority class initialized (levels=%d)\n", PRIO_LEVELS);
    return 0;
}
```

关键步骤：

1. 使用 `kmalloc()` 分配 `prio_rq_data_t` 结构体
2. 初始化 256 个队列头（128 active + 128 expired）
3. 初始化计数器和标志
4. 把数据结构指针赋给 `rq->class_data`
5. 注册调度类到框架

如果注册失败，记得释放分配的内存，避免内存泄漏。

---

## 内核初始化集成

```c
void kernel_init(void) {
    /* ... 其他初始化 ... */

    /* 初始化调度类框架 */
    sched_class_init();

    /* 注册 Round-Robin 调度类 */
    sched_rr_init();

    /* 注册 Priority 调度类 */
    sched_prio_init();

    /* ... 其他初始化 ... */
}
```

编译运行，你应该能看到类似的输出：

```
[SCHED] Initializing scheduler class framework
[SCHED] Scheduler class framework initialized
[SCHED] Initializing Round-Robin scheduling class
[SCHED] Registered class 'RR' for policy 0
[SCHED] Round-Robin class initialized (timeslice=10 ms)
[SCHED] Initializing Priority scheduling class
[SCHED] Registered class 'PRIO' for policy 1
[SCHED] Priority class initialized (levels=128)
```

---

## 常见问题

**问题：Priority 任务创建后没有运行**

检查 `sched_set_policy()` 是否正确调用。在创建进程后，需要显式设置调度策略：

```c
sched_set_policy(pcb, SCHED_PRIORITY, 50);  // priority = 50
```

**问题：class_data 分配失败**

确保内核堆已经初始化。`kmalloc()` 依赖于内核堆，如果在内核初始化早期调用，堆可能还没准备好。

**问题：Active/Expired 交换不工作**

检查 `prio_pick_next_task()` 中是否正确调用 `prio_swap_active_expired()`。确保交换后 `highest_prio` 被正确更新。

---

## 接下来

Priority 调度类已经实现了。现在系统支持两种调度策略：RR 用于普通进程，Priority 用于系统关键进程。但还有一个问题：如何把这些调度器集成到进程管理中？在下一篇文章中，我们会扩展 PCB，把调度实体嵌入进去，并修改 `fork()` 流程来支持调度策略设置。

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 时间片与定时器集成](05_时间片与定时器集成.md)  | [进程管理与调度器集成 →](07_进程管理与调度器集成.md)

</div>
