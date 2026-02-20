# 实现 Round-Robin 调度类 —— Stage 22 调度类框架实战指南（四）

## 前言

前面三篇文章我们搭好了框架的骨架，现在终于要实现第一个具体的调度类了。Round-Robin（RR）是所有调度算法中最简单的一个：任务排队，轮流执行，用完时间片就去队尾等待下一轮。虽然简单，但它包含了调度类的所有核心要素，是理解框架工作原理的最佳起点。

说实话，实现 RR 调度类不需要太多新知识。我们已经有了一个定义良好的接口（`sched_class_t`），只需要把 RR 的逻辑填进去就行。但这恰恰是虚函数表模式的好处：框架不用关心具体算法，算法也不用关心框架怎么调用它，双方通过接口打交道，各司其职。

这篇文章我们会创建 `sched_rr.h` 和 `sched_rr.c`，实现一个完整的 Round-Robin 调度类。完成后，系统就可以按照时间片轮流调度多个进程了。

---

## 现在我们要做什么

这篇文章的目标是实现 Round-Robin 调度类的所有操作：

1. 创建 `sched_rr.h`，定义 RR 调度类的接口
2. 创建 `sched_rr.c`，实现所有调度类操作
3. 实现入队、出队、选择任务等基本操作
4. 实现时间片处理和抢占判断
5. 实现任务初始化函数
6. 注册 RR 调度类到框架

---

## 从0开始 —— RR 调度的设计思路

### 为什么先做 RR

我们选择先实现 RR 调度是有原因的：

第一，**最简单**。RR 只需要一个简单的 FIFO 队列，不需要复杂的数据结构。代码量少，逻辑清晰，适合作为第一个实现。

第二，**足够完整**。虽然简单，但 RR 包含了调度类的所有操作：入队、出队、选择任务、处理时间片。理解了 RR，就理解了调度类的基本工作流程。

第三，**实用价值**。RR 是普通用户进程的默认调度策略，能够提供公平的 CPU 分配。即使没有 Priority 调度，系统也可以正常工作。

### RR 的调度逻辑

RR 的调度逻辑可以概括为几点：

1. **FIFO 排队**：新任务加入队列尾部，调度器从队列头部取任务
2. **固定时间片**：每个任务获得固定的时间片（10ms），用完就切换
3. **不抢占**：只要当前任务还有时间片，就不会被新任务抢占
4. **循环执行**：用完时间片的任务回到队列尾部，等待下一轮

这个逻辑非常直观，就像食堂排队打饭：先来先打，打到就打一份（时间片），打完走了下一个再来，循环往复。

---

## 创建 sched_rr.h

### 文件结构

`kernel/process/sched_rr.h` 是 RR 调度类的头文件，内容很简洁：

```c
/* ==============================================================================
 * CCOS - Round-Robin Scheduling Class
 * ==============================================================================
 * This module implements a simple Round-Robin scheduling algorithm.
 * Each task gets a time slice and executes in FIFO order.
 * ==============================================================================
 */

#pragma once

#include "process/sched.h"

/* ==============================================================================
 * Round-Robin Constants
 * ==============================================================================
 */

/**
 * @brief Default RR time slice in milliseconds
 */
#define RR_TIMESLICE_DEFAULT    DEF_TIMESLICE_MS

/* ==============================================================================
 * Round-Robin Class API
 * ==============================================================================
 */

/**
 * @brief Initialize the Round-Robin scheduling class
 * @return 0 on success, negative on error
 */
int sched_rr_init(void);

/**
 * @brief Get the RR scheduling class structure
 * @return Pointer to the RR class
 */
sched_class_t* sched_rr_get_class(void);
```

这个头文件只做了两件事：定义时间片常量（使用框架提供的默认值），声明初始化和获取调度类结构的函数。

具体实现的所有函数都是 `static` 的，外部代码不需要直接调用它们。调度器通过 `sched_class_t` 结构体中的函数指针来间接调用。

---

## 实现 sched_rr.c

### 文件头部与包含

```c
/* ==============================================================================
 * CCOS - Round-Robin Scheduling Class Implementation
 * ==============================================================================
 */

#include "process/sched_rr.h"
#include "assert/assert.h"
#include "klogs/kprintf.h"
#include "process/process.h"
#include "process/sched.h"
```

包含了必要的头文件。`assert.h` 用于断言检查，虽然我们的代码很简单，但加上一些断言可以让错误更容易被发现。

### 前向声明

```c
/* ==============================================================================
 * Forward Declarations
 * ==============================================================================
 */

struct pcb;
struct sched_rq;
```

### 函数声明

声明所有我们要实现的函数。这些函数都是 `static` 的，只在文件内可见：

```c
/* ==============================================================================
 * Round-Robin Class Operations
 * ==============================================================================
 */

/**
 * @brief Enqueue a task on the RR run queue
 * Simple FIFO enqueue
 */
static void rr_enqueue_task(struct sched_rq* rq, struct pcb* pcb, bool head);

/**
 * @brief Dequeue a task from the RR run queue
 */
static void rr_dequeue_task(struct sched_rq* rq, struct pcb* pcb);

/**
 * @brief Pick the next task from RR queue
 * Returns the first task in the queue (FIFO)
 */
static struct pcb* rr_pick_next_task(struct sched_rq* rq, struct pcb* prev);

/**
 * @brief Check if a task should preempt
 * In RR, tasks don't preempt based on priority
 * Only preempt if current task exhausted its time slice
 */
static bool rr_should_preempt(struct pcb* p, struct pcb* curr);

/**
 * @brief Handle timer tick for RR task
 */
static void rr_task_tick(struct sched_rq* rq, struct pcb* pcb);

/**
 * @brief Initialize a new task for RR
 */
static void rr_task_fork(struct pcb* pcb, int nice);

/**
 * @brief Get time slice for RR task
 */
static uint32_t rr_get_time_slice(const struct pcb* pcb);
```

每个函数都有清晰的注释说明它的作用。注意这些注释和 `sched.h` 中的接口声明注释是互补的：`sched.h` 描述"接口是什么"，这里的注释描述"RR 怎么实现"。

### RR 调度类结构

接下来是关键部分：定义 RR 调度类的虚函数表：

```c
/* ==============================================================================
 * Round-Robin Class Structure
 * ==============================================================================
 */

static sched_class_t rr_sched_class = {
    .name = "RR",
    .policy = SCHED_NORMAL,
    .enqueue_task = rr_enqueue_task,
    .dequeue_task = rr_dequeue_task,
    .pick_next_task = rr_pick_next_task,
    .should_preempt = rr_should_preempt,
    .task_tick = rr_task_tick,
    .task_fork = rr_task_fork,
    .get_time_slice = rr_get_time_slice,
};
```

这就是 RR 调度类的"身份证"。`name` 字段用于调试和日志，`policy` 标识这是普通调度策略。剩下的字段全是函数指针，指向我们要实现的各个函数。

这个结构体是 `static` 的，外部代码不会直接访问它。框架通过 `sched_class_register()` 注册它，然后通过 `scheduler.classes[]` 数组访问它。

---

## 入队操作实现

### rr_enqueue_task

入队操作把进程加入 RR 的运行队列：

```c
/* ==============================================================================
 * Round-Robin Operations Implementation
 * ==============================================================================
 */

/**
 * @brief Enqueue a task on the RR run queue
 */
static void rr_enqueue_task(struct sched_rq* rq, struct pcb* pcb, bool head) {
    if (head) {
        list_add(&pcb->sched_entity.run_list, &rq->queue);
    } else {
        list_add_tail(&pcb->sched_entity.run_list, &rq->queue);
    }

    rq->nr_running++;
}
```

这个函数的逻辑非常简单。根据 `head` 参数决定加到队列头部还是尾部。正常情况下（`head = false`），新任务加到尾部，保证 FIFO 顺序。但如果 `head = true`，任务会被加到头部，这可能用于某些特殊情况，比如唤醒一个高优先级的任务。

`run_list` 是 `sched_entity_t` 中的链表节点，用它把进程挂在队列上。`rq->queue` 是 `sched_rq_t` 中的队列头。

最后增加 `nr_running` 计数器，这个计数器在框架判断是否有任务可运行时很有用。

---

## 出队操作实现

### rr_dequeue_task

出队操作把进程从 RR 的运行队列移除：

```c
/**
 * @brief Dequeue a task from the RR run queue
 */
static void rr_dequeue_task(struct sched_rq* rq, struct pcb* pcb) {
    list_del_init(&pcb->sched_entity.run_list);
    rq->nr_running--;
}
```

使用 `list_del_init()` 从队列中删除节点。这个函数不仅删除节点，还会把节点的 `prev/next` 指针初始化为自己，防止野指针问题。

然后减少 `nr_running` 计数器。

---

## 任务选择实现

### rr_pick_next_task

选择下一个要运行的任务：

```c
/**
 * @brief Pick the next task from RR queue
 */
static struct pcb* rr_pick_next_task(struct sched_rq* rq, struct pcb* prev) {
    (void)prev; /* RR doesn't care about previous task */

    if (list_is_empty(&rq->queue)) {
        return NULL;
    }

    struct pcb* next = list_first_entry(&rq->queue, struct pcb, sched_entity.run_list);
    return next;
}
```

这个函数首先检查队列是否为空。如果为空，返回 `NULL` 表示没有可运行的任务。

如果有任务，使用 `list_first_entry()` 宏获取队列头部的第一个任务。注意这里我们获取的是 `struct pcb`，但链表节点是 `sched_entity.run_list`，所以宏的参数要指定正确的字段。

`prev` 参数被 RR 忽略了。RR 不关心上一个运行的任务是什么，它总是返回队列头部。但其他调度类（比如 Priority）可能会利用这个参数来做更智能的选择。

---

## 抢占判断实现

### rr_should_preempt

判断是否应该抢占当前任务：

```c
/**
 * @brief Check if a task should preempt
 * In RR, tasks don't preempt based on priority
 * Only preempt if current task exhausted its time slice
 */
static bool rr_should_preempt(struct pcb* p, struct pcb* curr) {
    (void)p; /* Not used in RR */

    /* Only preempt if current has no time left */
    return (curr->sched_entity.time_slice == 0);
}
```

RR 的抢占逻辑非常简单：只有当当前任务的时间片用完时，才允许"抢占"。这严格来说不是抢占，而是正常的时间片切换。

`p` 参数（候选任务）被忽略了。RR 不考虑任务本身的特性，所有任务都是平等的。

---

## 时间片处理实现

### rr_task_tick

这是定时器中断的回调函数，每毫秒被调用一次：

```c
/**
 * @brief Handle timer tick for RR task
 */
static void rr_task_tick(struct sched_rq* rq, struct pcb* pcb) {
    (void)rq; /* Not needed for RR */

    /* Decrement time slice */
    if (pcb->sched_entity.time_slice > 0) {
        pcb->sched_entity.time_slice--;
    }
}
```

RR 的逻辑非常简单：每毫秒递减时间片。当时间片减到 0 时，框架会在稍后触发调度，选择下一个任务运行。

这里我们没有直接设置重调度标志，是因为框架会检查时间片是否为 0，然后决定是否需要调度。如果时间片用完，框架会在合适的时候调用 `schedule()`。

---

## 任务初始化实现

### rr_task_fork

创建新进程时，这个函数会被调用来初始化调度实体：

```c
/**
 * @brief Initialize a new task for RR
 */
static void rr_task_fork(struct pcb* pcb, int nice) {
    (void)nice; /* Not used yet */

    pcb->sched_entity.sched_class = &rr_sched_class;
    pcb->sched_entity.policy = SCHED_NORMAL;
    pcb->sched_entity.priority = 0; /* Default priority */
    pcb->sched_entity.time_slice = RR_TIMESLICE_DEFAULT;
    pcb->sched_entity.time_slice_total = RR_TIMESLICE_DEFAULT;
    pcb->sched_entity.nice = 0;
}
```

这个函数设置进程调度实体的所有字段：

- `sched_class` 指向 RR 调度类结构
- `policy` 设置为 `SCHED_NORMAL`
- `priority` 设为 0（RR 不使用优先级，但初始化为默认值）
- `time_slice` 和 `time_slice_total` 设为默认的 10ms
- `nice` 目前未使用，设为 0

`nice` 参数目前被忽略了。未来我们可以利用它来实现动态优先级调整，比如让 nice 值为负的任务获得更长的时间片。

---

## 时间片获取实现

### rr_get_time_slice

返回 RR 的固定时间片长度：

```c
/**
 * @brief Get time slice for RR task
 */
static uint32_t rr_get_time_slice(const struct pcb* pcb) {
    (void)pcb; /* RR uses fixed time slice */
    return RR_TIMESLICE_DEFAULT;
}
```

RR 使用固定时间片，所以直接返回常量 `RR_TIMESLICE_DEFAULT`（10ms）。`pcb` 参数被忽略了。

对于 Priority 调度类，这个函数可能会根据优先级动态计算时间片，所以 `pcb` 参数在那里是有用的。

---

## 调度类注册

### sched_rr_init

初始化函数，把 RR 调度类注册到框架：

```c
/* ==============================================================================
 * Round-Robin Class Registration
 * ==============================================================================
 */

/**
 * @brief Initialize the RR scheduling class
 */
int sched_rr_init(void) {
    klog_info("[SCHED] Initializing Round-Robin scheduling class\n");

    extern scheduler_t scheduler;

    /* Run queues are initialized in sched_class_init, just register the class */
    /* Register the class */
    int ret = sched_class_register(&rr_sched_class, SCHED_NORMAL);
    if (ret != 0) {
        klog_error("[SCHED] Failed to register RR class\n");
        return ret;
    }

    klog_info("[SCHED] Round-Robin class initialized (timeslice=%d ms)\n", RR_TIMESLICE_DEFAULT);
    return 0;
}
```

这个函数调用框架的 `sched_class_register()` 函数，把 `rr_sched_class` 结构体注册为 `SCHED_NORMAL` 策略的调度类。

运行队列在 `sched_class_init()` 中已经初始化好了，这里不需要再处理。

### sched_rr_get_class

返回 RR 调度类结构体的指针：

```c
/**
 * @brief Get the RR class structure
 */
sched_class_t* sched_rr_get_class(void) {
    return &rr_sched_class;
}
```

这个函数主要用于调试和测试，正常流程中不需要直接获取调度类指针。

---

## 内核初始化集成

现在 RR 调度类已经实现完毕，我们可以在内核初始化流程中注册它了。打开 `kernel/main.c` 或类似的初始化代码：

```c
void kernel_init(void) {
    /* ... 其他初始化 ... */

    /* 初始化调度类框架 */
    sched_class_init();

    /* 注册 Round-Robin 调度类 */
    sched_rr_init();

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
```

这表示 RR 调度类已经成功注册。现在系统使用 RR 策略来调度进程了。

---

## 完整的调度流程

让我们梳理一下 RR 调度下，从进程创建到调度的完整流程：

```
进程创建 (proc_fork)
    │
    ├─> 调用 sched_set_policy(child, SCHED_NORMAL, 0)
    │       │
    │       └─> pcb->sched_entity.sched_class = s_classes[SCHED_NORMAL]
    │           │   (= rr_sched_class)
    │           │
    │           └─> rr_sched_class.task_fork(child, 0)
    │               └─> 初始化时间片、优先级等
    │
    ├─> 调用 sched_enqueue_task(child, false)
    │       │
    │       └─> rr_sched_class.enqueue_task(rr_rq, child, false)
    │           └─> list_add_tail(..., &rr_rq->queue)
    │
    └─> 进程就绪，等待调度

调度触发 (schedule)
    │
    ├─> 调用 sched_pick_next_task()
    │       │
    │       ├─> 检查 RR 队列是否为空
    │       ├─> rr_sched_class.pick_next_task(rr_rq, prev)
    │       │   └─> list_first_entry(&rr_rq->queue)
    │       │
    │       └─> 返回队列头部的任务
    │
    ├─> 调用 sched_reset_time_slice(next)
    │   └─> next->time_slice = next->time_slice_total
    │
    └─> 切换到 next 进程

定时器中断 (每 1ms)
    │
    ├─> 调用 sched_timer_tick_handler()
    │       │
    │       └─> current->sched_class->task_tick(rr_rq, current)
    │           └─> rr_task_tick(rr_rq, current)
    │               └─> current->time_slice--
    │                   │
    │                   └─> 如果 time_slice == 0，设置 need_resched
    │
    └─> 中断返回，如果 need_resched，稍后触发 schedule()
```

这个流程图展示了从进程创建到被调度运行，再到时间片耗尽的完整过程。理解这个流程对于调试调度问题非常有帮助。

---

## 常见问题

**问题：RR 调度类注册后，进程还是没有被调度**

首先要确认进程是否正确加入了运行队列。可以在 `sched_enqueue_task()` 中加日志：

```c
klog_info("[SCHED] Enqueue PID=%d to policy=%d queue\n", pcb->pid, pcb->sched_entity.policy);
```

如果日志显示进程已入队，但调度器还是选择 idle 任务，可能是 `schedule()` 函数没有正确调用 `sched_pick_next_task()`。

**问题：时间片没有生效，进程一直运行不被切换**

检查定时器回调是否正确注册。在 `sched_class_init()` 中应该有：

```c
timer_set_callback(sched_timer_tick_handler);
```

还要确认定时器确实在工作。可以在 `rr_task_tick()` 中加日志：

```c
klog_info("[RR] PID=%d time_slice=%d\n", pcb->pid, pcb->sched_entity.time_slice);
```

如果看不到日志，可能是定时器中断没有正确配置。

**问题：编译报错 undefined reference to `rr_sched_class`**

这是因为 `rr_sched_class` 被声明为 `static`，外部文件无法访问。如果你需要在外部访问它（比如在调试代码中），要么去掉 `static`，要么使用 `sched_rr_get_class()` 函数获取指针。

---

## 接下来

RR 调度类已经完成了，系统现在可以公平地分配 CPU 时间给各个进程。但 RR 有个明显的局限：所有进程平等，无法区分优先级。在下一篇文章中，我们会实现时间片与定时器的完整集成，确保调度器能够按时切换任务。

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 搭建调度类框架脚手架](03_搭建调度类框架脚手架.md)  | [时间片与定时器集成 →](05_时间片与定时器集成.md)

</div>
