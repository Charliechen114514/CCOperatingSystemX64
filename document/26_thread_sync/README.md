# CCOS 线程同步原语文档中心

本目录包含 CCOS Stage 26 - 线程同步原语开发的完整文档体系。

---

## 阶段概述

**Stage 26: 线程同步原语**

本阶段在 Stage 25 EXT2 文件系统基础上，实现了完整的线程同步基础设施。这是多线程操作系统最核心的组件之一，提供了自旋锁、互斥锁、信号量、条件变量、读写锁、原子操作和等待队列七种同步原语，同时完善了线程支持。

### 核心成果

- **自旋锁** ([`kernel/sync/spinlock.h`](../../kernel/sync/spinlock.h))
  - 基于原子操作的忙等待锁
  - 中断安全版本 (irqsave/irqrestore)
  - 尝试锁支持 (trylock)

- **互斥锁** ([`kernel/sync/mutex.h`](../../kernel/sync/mutex.h))
  - 可睡眠锁，支持递归
  - 所有者跟踪
  - 等待队列管理

- **信号量** ([`kernel/sync/semaphore.h`](../../kernel/sync/semaphore.h))
  - 资源计数机制
  - 支持多资源访问
  - 生产者-消费者模式

- **条件变量** ([`kernel/sync/condvar.h`](../../kernel/sync/condvar.h))
  - 与 mutex 配合使用
  - 条件等待和通知
  - 广播支持

- **读写锁** ([`kernel/sync/rwlock.h`](../../kernel/sync/rwlock.h))
  - 读者优先策略
  - 多读者或单写者
  - 写者降级支持

- **原子操作** ([`kernel/sync/atomic.h`](../../kernel/sync/atomic.h))
  - 无锁编程基础
  - 完整的算术操作
  - CAS 操作支持
  - 内存屏障

- **等待队列** ([`kernel/sync/waitqueue.h`](../../kernel/sync/waitqueue.h))
  - 进程阻塞/唤醒基础设施
  - 独占/非独占等待
  - 中断唤醒支持

- **线程支持** ([`kernel/process/thread.c`](../../kernel/process/thread.c))
  - 内核线程创建
  - 用户线程创建
  - 线程 join/detach
  - 线程退出处理

- **同步演示程序** ([`kernel/demo/sync/sync_demo.h`](../../kernel/demo/sync/sync_demo.h))
  - 互斥锁使用示例
  - 信号量生产者-消费者
  - 条件变量使用
  - 读写锁演示

---

## 目录结构

```
kernel/
├── sync/
│   ├── spinlock.h/c              # 自旋锁
│   ├── mutex.h/c                 # 互斥锁
│   ├── semaphore.h/c             # 信号量
│   ├── condvar.h/c               # 条件变量
│   ├── rwlock.h/c                # 读写锁
│   ├── atomic.h/c                # 原子操作
│   ├── waitqueue.h/c             # 等待队列
│   └── CMakeLists.txt            # 构建配置
├── process/
│   ├── thread.c                  # 线程支持
│   ├── process.h/c               # 进程管理 (扩展)
│   └── switch.s                  # 上下文切换 (改进)
├── demo/
│   ├── sync/
│   │   ├── sync_demo.h/c         # 同步演示
│   │   └── sync_thread_test.c    # 线程测试
│   └── thread/
│       └── thread_demo.h/c       # 线程演示
└── syscall/
    └── syscall_table.c           # 新增线程系统调用
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要同步原语
- 同步设计基础（竞态条件、死锁、优先级反转）
- 设计决策（为什么需要多种原语、实现选择）
- 架构设计（分层设计）
- 实现细节（每种原语的实现机制）
- 常见陷阱（死锁、活锁、饥饿）
- 未来改进方向

**适合**:
- 理解设计思路
- 学习同步原理
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- 同步原语 API 完整参考
- 线程 API 参考
- 数据结构定义
- 常量定义
- 系统调用扩展
- 使用示例

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

## 快速开始

### 查看代码结构

1. **同步原语接口** → 查看 [`kernel/sync/`](../../kernel/sync/)
2. **线程支持** → 查看 [`kernel/process/thread.c`](../../kernel/process/thread.c)
3. **同步演示** → 查看 [`kernel/demo/sync/sync_demo.h`](../../kernel/demo/sync/sync_demo.h)

### 使用示例

#### 自旋锁

```c
#include "sync/spinlock.h"

spinlock_t lock;
spin_init(&lock);

// 获取锁
spin_lock(&lock);
// 临界区
critical_section();
spin_unlock(&lock);

// 中断安全版本
unsigned long flags;
spin_lock_irqsave(&lock, &flags);
critical_section();
spin_unlock_irqrestore(&lock, &flags);
```

#### 互斥锁

```c
#include "sync/mutex.h"

mutex_t mutex;
mutex_init(&mutex);

// 获取锁
mutex_lock(&mutex);
// 临界区
critical_section();
mutex_unlock(&mutex);

// 非阻塞尝试
if (mutex_trylock(&mutex) == 0) {
    // 获得锁
    critical_section();
    mutex_unlock(&mutex);
}
```

#### 信号量

```c
#include "sync/semaphore.h"

semaphore_t sem;
sem_init(&sem, 0);  // 初始值为 0

// 生产者
sem_post(&sem);     // 增加信号量

// 消费者
sem_wait(&sem);     // 等待信号量
consume_item();
```

#### 条件变量

```c
#include "sync/condvar.h"

mutex_t mutex;
condvar_t cond;

mutex_init(&mutex);
condvar_init(&cond);

// 等待线程
mutex_lock(&mutex);
while (!condition) {
    condvar_wait(&cond, &mutex);
}
// 条件满足，执行操作
mutex_unlock(&mutex);

// 通知线程
mutex_lock(&mutex);
condition = true;
condvar_signal(&cond);  // 或 condvar_broadcast(&cond)
mutex_unlock(&mutex);
```

#### 读写锁

```c
#include "sync/rwlock.h"

rwlock_t rwlock;
rwlock_init(&rwlock);

// 读者
rwlock_read_lock(&rwlock);
read_data();
rwlock_read_unlock(&rwlock);

// 写者
rwlock_write_lock(&rwlock);
write_data();
rwlock_write_unlock(&rwlock);
```

#### 原子操作

```c
#include "sync/atomic.h"

atomic_t counter;
atomic_set(&counter, 0);

// 原子增加
atomic_inc(&counter);
atomic_add(&counter, 10);

// 原子减少
atomic_dec(&counter);

// 原子读取
int value = atomic_read(&counter);

// CAS 操作
int old = 1;
int new = 2;
atomic_compare_and_exchange(&counter, &old, new);
```

#### 线程创建

```c
#include "process/process.h"

// 创建内核线程
pcb_t* thread = proc_create_kernel_thread(my_thread_func, arg);

// 创建用户线程
pcb_t* thread = proc_create_user_thread(entry_point, stack, arg);

// 等待线程结束
int ret_value;
proc_thread_join(thread->pid, &ret_value);

// 分离线程
proc_thread_detach(thread);
```

### 同步原语选择指南

| 场景 | 推荐原语 | 理由 |
|------|---------|------|
| 短临界区 (< 微秒) | spinlock | 忙等待快，避免调度开销 |
| 长临界区 | mutex | 可睡眠，不浪费 CPU |
| 资源计数 | semaphore | 天然支持计数 |
| 条件等待 | condvar + mutex | 标准模式 |
| 读多写少 | rwlock | 允许并发读 |
| 简单计数 | atomic | 无锁，最快 |
| 复杂等待 | waitqueue | 灵活可控 |

---

## 与前一阶段对比

| 特性 | Stage 25 (EXT2+VFS) | Stage 26 (线程同步) |
|------|---------------------|---------------------|
| 同步原语 | 无 | 7 种完整实现 |
| 线程支持 | 仅进程 | 内核+用户线程 |
| 原子操作 | 无 | 完整原子操作集 |
| 系统调用 | 基础文件系统 | + 线程管理 |
| 死锁处理 | 不适用 | 完整支持 |
| 上下文切换 | 基础版本 | 增强版 |
| 新增文件 | - | 20+ 个 |

---

## 技术亮点

### 1. 自旋锁实现

```c
typedef struct {
    volatile int locked;
} spinlock_t;

static inline void spin_lock(spinlock_t* lock) {
    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        // 忙等待
        __asm__ volatile("pause");
    }
}

static inline void spin_unlock(spinlock_t* lock) {
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}
```

### 2. 互斥锁实现

```c
typedef struct {
    atomic_t locked;
    pcb_t* owner;
    uint32_t count;
    spinlock_t wait_lock;
    list_head wait_list;
} mutex_t;

void mutex_lock(mutex_t* mutex) {
    // 快速路径：无竞争
    if (atomic_compare_and_exchange(&mutex->locked, 0, 1) == 0) {
        mutex->owner = proc_current();
        mutex->count = 1;
        return;
    }

    // 慢速路径：可能有竞争
    spin_lock(&mutex->wait_lock);

    // 检查递归
    if (mutex->owner == proc_current()) {
        mutex->count++;
        spin_unlock(&mutex->wait_lock);
        return;
    }

    // 添加到等待队列
    list_add_tail(&proc_current()->wait_list, &mutex->wait_list);
    spin_unlock(&mutex->wait_lock);

    // 阻塞当前进程
    proc_block();
}
```

### 3. 原子操作

```c
typedef struct {
    volatile int counter;
} atomic_t;

// CAS 操作
static inline int atomic_compare_and_exchange(atomic_t* v,
                                               int* oldval,
                                               int newval) {
    return __atomic_compare_exchange_n(&v->counter, oldval, newval,
                                       0, __ATOMIC_SEQ_CST,
                                       __ATOMIC_SEQ_CST);
}

// 内存屏障
#define mb()  __asm__ volatile("mfence" ::: "memory")
#define rmb() __asm__ volatile("lfence" ::: "memory")
#define wmb() __asm__ volatile("sfence" ::: "memory")
```

### 4. PCB 扩展支持线程

```c
typedef struct pcb {
    // 原有字段...

    // 新增线程支持
    int32_t tgid;                  // 线程组 ID
    bool is_thread;                // 是否为线程
    list_head thread_list;         // 同组线程链表
    list_head thread_group;        // 线程组链表

    // 线程入口
    thread_entry_t thread_entry;
    void* thread_arg;

    // 用户栈
    virtual_addr_t user_stack;
    size_t user_stack_size;

    // Join/Detach
    list_head join_waiters;
    bool detached;
    void* return_value;
} pcb_t;
```

### 5. 同步原语层次结构

```
┌─────────────────────────────────────────────────────────┐
│                    高层同步原语                           │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐   │
│  │ Mutex   │  │Semaphor │  │CondVar  │  │RWLock   │   │
│  │         │  │         │  │         │  │         │   │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘   │
└───────┼────────────┼────────────┼────────────┼──────────┘
        │            │            │            │
        └────────────┴────────────┴────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────┐
│                    中层同步原语                           │
│  ┌─────────┐                  ┌─────────┐               │
│  │WaitQueue│  ◄─────────────► │Spinlock │               │
│  └─────────┘                  └─────────┘               │
└─────────────────────────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────┐
│                    底层同步原语                           │
│  ┌─────────────────────────────────────────────────┐   │
│  │              Atomic Operations                    │   │
│  │  atomic_inc/dec, atomic_add/sub, CAS, barriers   │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
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
┌───────▼──────┐   ┌─────────▼──────┐   ┌─────────▼──────┐
│ 开发笔记     │   │ 技术参考        │   │ README.md      │
│ (设计思路)    │   │ (API手册)       │   │ (快速开始)      │
└──────────────┘   └────────────────┘   └────────────────┘
        │                    │
        └──────────────────┬─────────┘
                           │
                   ┌──────▼──────┐
                   │ sync/ 源代码  │
                   │ thread.c     │
                   └─────────────┘
```

---

## 版本信息

- **阶段**: Stage 26 (Final)
- **分支**: `stage_fin/26_thread_sync`
- **日期**: 2026-02-18
- **作者**: CharlieChen

---

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../25_ext2_vfs/README.md](../25_ext2_vfs/) - 上一阶段文档

### 源码文件
- [`kernel/sync/spinlock.h`](../../kernel/sync/spinlock.h) - 自旋锁
- [`kernel/sync/mutex.h`](../../kernel/sync/mutex.h) - 互斥锁
- [`kernel/sync/semaphore.h`](../../kernel/sync/semaphore.h) - 信号量
- [`kernel/sync/condvar.h`](../../kernel/sync/condvar.h) - 条件变量
- [`kernel/sync/rwlock.h`](../../kernel/sync/rwlock.h) - 读写锁
- [`kernel/sync/atomic.h`](../../kernel/sync/atomic.h) - 原子操作
- [`kernel/sync/waitqueue.h`](../../kernel/sync/waitqueue.h) - 等待队列
- [`kernel/process/thread.c`](../../kernel/process/thread.c) - 线程支持
- [`kernel/demo/sync/sync_demo.h`](../../kernel/demo/sync/sync_demo.h) - 演示程序

### 外部参考
- [Spinlocks](https://wiki.osdev.org/Spinlock)
- [Mutex](https://en.wikipedia.org/wiki/Mutual_exclusion)
- [Semaphores](https://en.wikipedia.org/wiki/Semaphore_(programming))
- [Monitors](https://en.wikipedia.org/wiki/Monitor_(synchronization))
- [Readers-writers problem](https://en.wikipedia.org/wiki/Readers%E2%80%93writers_problem)
- [OSDev.org](https://wiki.osdev.org)

---

**作者**: CharlieChen
**最后更新**: 2026-02-18
