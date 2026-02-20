# 实现 PID 分配器

## 前言

在开始实现复杂的进程管理功能之前，我们需要先解决一个基础问题：如何给每个进程分配一个唯一的 ID？这听起来很简单，但实现时有不少细节需要注意。

PID（Process ID）分配器是进程管理的基础设施。每次创建新进程时，都需要从 PID 分配器获取一个可用的 PID。进程退出时，需要把 PID 回收，让其他进程可以复用。这个操作必须高效、可靠，还要考虑多核环境下的并发访问。

## PID 分配器的需求

一个好的 PID 分配器需要满足以下要求：

**唯一性** - 每个 PID 在同一时刻只能被一个进程使用。这是最基本的要求，如果两个进程有相同的 PID，会引发各种奇怪的问题。

**高效性** - 分配和释放操作应该尽可能快。进程创建和退出是高频操作，PID 分配不能成为瓶颈。

**可复用** - 进程退出后，它的 PID 应该能被其他进程使用。否则 PID 会很快耗尽。

**范围限制** - PID 通常有一个上限（如 32768），不能无限制分配。达到上限时应该返回错误。

**并发安全** - 在多核环境下，多个 CPU 可能同时调用 `pid_alloc()`，必须避免竞争条件。

## Bitmap 分配方案

我们选择基于 bitmap 的分配方案，这是实现 PID 分配器的经典方法。

bitmap 是一串二进制位，每一位对应一个 PID。如果位是 0，表示对应的 PID 可用；如果是 1，表示已被占用。比如，bitmap 的第 5 位是 1，说明 PID 5 已被使用。

分配时，找到第一个为 0 的位，把它设为 1，返回对应的 PID。释放时，把对应的位清零。这种方法的时间复杂度是 O(n)，其中 n 是 bitmap 的大小。但利用 CPU 的位操作指令，实际运行非常快。

在我们的实现中，`PID_MAX` 定义为 32768，这意味着 bitmap 需要 32768 位 = 4096 字节 = 4KB。这是一个页的大小，内存占用非常小。

## 实现细节

首先定义 bitmap 存储和辅助函数。我们使用内核已有的 bitmap 模块：

```c
#define PID_BITMAP_SIZE ((PID_MAX + 7) / 8)

static byte_t s_pid_bitmap_buffer[PID_BITMAP_SIZE];
static struct bitmap s_pid_bitmap;
```

`s_pid_bitmap_buffer` 是 bitmap 的底层存储，`s_pid_bitmap` 是 bitmap 结构体，封装了操作 bitmap 的方法。

为了并发安全，我们需要一个自旋锁保护 bitmap 操作：

```c
static spinlock_t s_pid_lock = SPIN_LOCK_INIT;
```

这个锁确保在同一时刻只有一个 CPU 可以修改 bitmap。

## 初始化函数

`pid_alloc_init()` 在内核启动时调用，初始化 PID 分配器：

```c
void pid_alloc_init(void) {
    bitmap_init(&s_pid_bitmap, s_pid_bitmap_buffer, PID_MAX);
    bitmap_set(&s_pid_bitmap, 0); /* Reserve PID 0 */
}
```

首先调用 `bitmap_init()` 初始化 bitmap 结构，然后设置第 0 位为 1，保留 PID 0。PID 0 是特殊的，通常不分配给普通进程。

## 分配函数

`pid_alloc()` 分配一个新的 PID：

```c
int32_t pid_alloc(void) {
    spinlock_flags_t flags;
    spin_lock_irqsave(&s_pid_lock, &flags);

    int pid = bitmap_find_first_zero(&s_pid_bitmap);
    if (pid < 0 || pid >= PID_MAX) {
        spin_unlock_irqrestore(&s_pid_lock, flags);
        return -1;
    }
    bitmap_set(&s_pid_bitmap, pid);

    spin_unlock_irqrestore(&s_pid_lock, flags);

    return pid;
}
```

首先用 `spin_lock_irqsave()` 获取锁并保存中断标志，防止死锁。然后调用 `bitmap_find_first_zero()` 找到第一个为 0 的位。如果没有找到（返回值 < 0 或 >= PID_MAX），说明 PID 已耗尽，返回 -1。

找到可用 PID 后，用 `bitmap_set()` 设置对应的位为 1，标记为已占用。最后用 `spin_unlock_irqrestore()` 释放锁并恢复中断标志。

注意这里的错误处理：如果 PID 耗尽，我们返回 -1 而不是 PANIC。这是因为 PID 耗尽虽然严重，但不一定致命。调用者可以决定如何处理，比如等待一些进程退出后再重试。

## 释放函数

`pid_free()` 释放一个 PID：

```c
void pid_free(int32_t pid) {
    if (pid > 0 && pid < PID_MAX) {
        spinlock_flags_t flags;
        spin_lock_irqsave(&s_pid_lock, &flags);
        bitmap_clear(&s_pid_bitmap, pid);
        spin_unlock_irqrestore(&s_pid_lock, flags);
    }
}
```

首先检查 PID 是否在有效范围内（0 < pid < PID_MAX）。PID 0 不应该被释放，因为它被保留用于特殊用途。如果 PID 有效，获取锁，用 `bitmap_clear()` 清除对应的位，然后释放锁。

注意这里没有检查 PID 是否真的被占用。如果释放一个从未分配的 PID，bitmap 的对应位本来就是 0，清除它不会有问题。当然，这意味着调用者可能犯了错误，但这种情况下静默失败比崩溃要好。

## 测试 PID 分配器

让我们写一个简单的测试来验证 PID 分配器的正确性。这个测试可以放在演示程序中：

```c
bool test_pid_allocation(void) {
    klog_info("[PROC_DEMO] Test: PID Allocation...\n");

    /* Initialize PID allocator */
    pid_alloc_init();

    /* Allocate some PIDs */
    int32_t pid1 = pid_alloc();
    if (pid1 <= 0) {
        klog_error("[PROC_DEMO] First PID allocation failed\n");
        return false;
    }

    int32_t pid2 = pid_alloc();
    if (pid2 <= 0 || pid2 == pid1) {
        klog_error("[PROC_DEMO] Second PID allocation failed or duplicate\n");
        return false;
    }

    /* Free first PID */
    pid_free(pid1);

    /* Allocate again - should get the same PID back */
    int32_t pid3 = pid_alloc();
    if (pid3 != pid1) {
        klog_error("[PROC_DEMO] PID reuse after free failed\n");
        return false;
    }

    klog_info("[PROC_DEMO] PID Allocation test PASSED\n");
    return true;
}
```

这个测试做了几件事：首先分配两个 PID，验证它们不同且都有效。然后释放第一个 PID，再次分配，验证能复用刚释放的 PID。

## 性能考虑

Bitmap 分配的时间复杂度是 O(n)，其中 n 是 PID_MAX。在实际运行中，这通常不是问题，因为：

1. CPU 的位操作指令非常快，查找第一个零位只需要几条指令
2. bitmap 通常在 CPU 缓存中，访问延迟很低
3. 进程创建/退出相对于其他操作（如内存分配）来说已经是慢速操作

如果需要更快的分配，可以考虑使用更复杂的数据结构，如空闲链表或基数树。但这些实现更复杂，对于教学项目来说，bitmap 已经足够好。

## 并发安全性

我们的实现使用了自旋锁来保护 bitmap，确保在多核环境下的安全性。但要注意，自旋锁在锁竞争激烈时会浪费 CPU 周期。如果 PID 分配成为瓶颈（不太可能），可以考虑使用无锁算法或更细粒度的锁。

另一个要注意的是中断安全。我们使用 `spin_lock_irqsave()` 而不是 `spin_lock()`，这是因为 `pid_alloc()` 可能会在中断上下文中被调用。如果使用普通锁，可能发生死锁：中断处理程序尝试获取已被当前进程持有的锁，但当前进程无法继续执行来释放锁。

## 接下来

PID 分配器虽然简单，但它为进程管理奠定了基础。在下一篇文章中，我们会实现进程控制块（PCB）的管理，包括分配、释放和查找操作。PCB 是进程存在的物理载体，每个进程都有一个 PCB 存储所有信息。

PCB 管理比 PID 分配器复杂一些，因为涉及内存分配、链表操作、内核栈管理等。但这些操作都是标准的内核编程技巧，掌握之后对任何内核开发都有帮助。

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 创建进程管理脚手架](./03_创建进程管理脚手架.md) | [实现进程控制块管理 →](./05_实现进程控制块管理.md)

</div>
