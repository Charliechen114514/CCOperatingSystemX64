# 实现 fork 系统调用

## 前言

fork 是 UNIX 系统的传奇系统调用。它创建一个几乎和父进程完全相同的子进程，子进程复制了父进程的内存、文件描述符、寄存器状态等一切。从这个点开始，父子进程各自独立运行，父进程返回子进程的 PID，子进程返回 0。

这个设计看似简单，实则强大。通过 fork 和 exec 的组合，UNIX 可以创建任何类型的进程。shell 通过 fork 创建子进程，然后 exec 替换为用户命令。服务器通过 fork 创建 worker 进程，每个 worker 处理一个客户端连接。

fork 的实现非常复杂。我们需要复制父进程的地址空间、创建新的 PCB、分配内核栈、设置执行上下文、建立父子关系。任何一个环节出错都会导致系统崩溃或内存泄漏。

## fork 的工作流程

让我们先理解 fork 的完整流程：

1. 分配子进程的 PCB
2. 分配 PID
3. 设置子进程的基本信息（PID、PPID、状态等）
4. 复制父进程的地址空间
5. 分配内核栈
6. 设置执行上下文
7. 建立父子关系
8. 添加到运行队列
9. 返回：父进程返回子进程 PID，子进程返回 0

这个流程中，地址空间复制是最复杂的。传统的 fork 会逐页复制父进程的物理内存，这非常耗时。现代系统使用写时复制（COW）优化：父子进程共享物理页，标记为只读，当一方试图写入时才复制。

我们的实现会先做简化版本：给子进程分配一个新的空地址空间。这不是一个可用的 fork，但足以让我们把框架搭建起来。写时复制留到后续优化。

## 实现 proc_fork

```c
int32_t proc_fork(void) {
    pcb_t* parent = proc_current();
    if (!parent) {
        klog_error("[PROC] No current process to fork from\n");
        return -1;
    }
```

首先获取当前进程（父进程）。如果没有当前进程，说明 fork 被错误地调用了，返回错误。

分配子进程 PCB：

```c
    /* Allocate child PCB */
    pcb_t* child = proc_alloc_pcb();
    if (!child) {
        klog_error("[PROC] Failed to allocate child PCB\n");
        return -1;
    }
```

分配 PID：

```c
    /* Allocate PID */
    child->pid = pid_alloc();
    if (child->pid < 0) {
        klog_error("[PROC] Failed to allocate PID\n");
        proc_free_pcb(child);
        return -1;
    }
```

PID 耗尽时返回 -1。在真实系统中，这种情况应该返回 EAGAIN 错误，让调用者稍后重试。

设置子进程的基本信息：

```c
    /* Set up child's basic info */
    child->ppid = parent->pid;
    child->parent = parent;
    child->state = PROC_READY;
    child->start_time = 0; /* TODO: Get actual time */

    /* Set up thread group ID - new process is its own thread group leader */
    child->tgid = child->pid;
    child->is_thread = false;

    /* Reset mm_refcount for new address space */
    atomic_write(&child->mm_refcount, 1);

    /* Copy command name */
    for (int i = 0; i < 16; i++) {
        child->comm[i] = parent->comm[i];
    }
```

设置父进程 ID、父进程指针、状态等。新进程是自己的线程组领导者，所以 `tgid = pid`。命令名从父进程复制。

复制地址空间：

```c
    /* Copy address space */
    if (proc_copy_address_space(parent, child) != 0) {
        pid_free(child->pid);
        proc_free_pcb(child);
        return -1;
    }
```

这是最复杂的一步。我们用一个单独的函数来实现：

```c
static int proc_copy_address_space(pcb_t* parent, pcb_t* child) {
    /* Create new user address space */
    vmm_result_t result = vmm_create_user_space(&child->mm.pml4_phys);
    if (result != VMM_OK) {
        klog_error("[PROC] Failed to create user address space for child\n");
        return -1;
    }

    /* For now, we just create an empty address space */
    /* TODO: Implement proper COW fork by:
     * 1. Copying parent's page table entries to child
     * 2. Registering pages with COW subsystem
     * 3. Marking pages as read-only with COW flag
     */

    /* Copy memory context settings */
    child->mm.brk = parent->mm.brk;
    child->mm.stack_start = parent->mm.stack_start;

    return 0;
}
```

当前实现只是创建一个新的空地址空间，复制 brk 和 stack_start。完整的 fork 应该复制父进程的页表项，并标记为写时复制。这个优化留到后续实现。

建立父子关系：

```c
    /* Add to parent's children list */
    list_add_tail(&child->siblings, &parent->children);
```

把子进程加入到父进程的 children 链表。这个链表用于 wait4 系统调用遍历子进程。

初始化调度实体：

```c
    /* Initialize scheduling entity with RR class */
    sched_set_policy(child, SCHED_NORMAL, 0);
```

设置子进程使用 Round-Robin 调度。

添加到运行队列：

```c
    /* Add to scheduler run queue */
    sched_enqueue_task(child, false); /* false = add to tail */
```

把子进程加入到运行队列尾部，调度器会在合适的时机调度它。

返回：

```c
    klog_info("[PROC] Forked: parent PID=%d, child PID=%d\n", parent->pid, child->pid);

    /* If this is the child process, return 0 */
    if (proc_current() == child) {
        return 0;
    }

    /* Parent returns child's PID */
    return child->pid;
}
```

这里有一个问题：`proc_current()` 在这个阶段还是父进程，因为调度还没有切换到子进程。真正的 fork 实现需要在系统调用返回时根据返回路径设置不同的返回值。

## 系统调用包装

fork 最终会通过系统调用被用户程序调用。系统调用处理函数需要设置子进程的返回值为 0：

```c
int64_t sys_fork(syscall_frame_t* frame) {
    int32_t pid = proc_fork();

    if (pid == 0) {
        /* Child process - return 0 */
        return 0;
    } else if (pid > 0) {
        /* Parent process - return child's PID */
        return pid;
    } else {
        /* Error */
        return -1; /* Or appropriate error code */
    }
}
```

但这个实现还不够。真正的 fork 需要在系统调用入口处保存陷阱帧，然后在调度到子进程时恢复陷阱帧并返回 0。这需要修改系统调用框架，在下一篇文章中会详细讨论。

## 地址空间复制的优化

当前的实现非常简陋：子进程得到一个空地址空间。这意味着子进程无法访问父进程的任何内存，fork 之后几乎做不了任何事情。

完整的 fork 应该复制父进程的地址空间。有两种实现方式：

**完全复制** - 逐页复制父进程的物理内存到子进程。这需要遍历父进程的页表，对于每个有效页分配新的物理页并复制内容。这种方式简单但非常慢。

**写时复制（COW）** - 父子进程共享物理页，但标记为只读。当任何一方试图写入时触发页错误，页错误处理程序分配新的物理页并复制内容。这种方式快很多，但实现复杂。

我们的实现会分两步走：先实现完全复制，确保功能正确；然后再优化为写时复制。这样可以逐步验证每一步的正确性，避免一次性实现太多功能导致调试困难。

## 测试 fork

让我们写一个简单的测试来验证 fork：

```c
bool test_fork(void) {
    klog_info("[PROC_DEMO] Test: Fork...\n");

    /* Create a test parent process */
    pcb_t* parent = proc_alloc_pcb();
    if (!parent) {
        klog_error("[PROC_DEMO] Failed to allocate parent PCB\n");
        return false;
    }

    parent->pid = 1;
    parent->ppid = 0;
    parent->state = PROC_RUNNING;
    scheduler.current = parent;

    /* Fork */
    int32_t child_pid = proc_fork();

    if (child_pid < 0) {
        klog_error("[PROC_DEMO] Fork failed\n");
        proc_free_pcb(parent);
        return false;
    }

    /* Verify child was created */
    pcb_t* child = proc_find(child_pid);
    if (!child) {
        klog_error("[PROC_DEMO] Child not found\n");
        proc_free_pcb(parent);
        return false;
    }

    /* Verify parent-child relationship */
    if (child->ppid != parent->pid) {
        klog_error("[PROC_DEMO] Child's PPID is wrong\n");
        proc_free_pcb(parent);
        proc_free_pcb(child);
        return false;
    }

    /* Verify child is on run queue */
    if (child->state != PROC_READY) {
        klog_error("[PROC_DEMO] Child is not READY\n");
        proc_free_pcb(parent);
        proc_free_pcb(child);
        return false;
    }

    /* Cleanup */
    proc_free_pcb(parent);
    proc_free_pcb(child);

    klog_info("[PROC_DEMO] Fork test PASSED\n");
    return true;
}
```

这个测试验证了 fork 的基本功能：子进程被创建、父子关系正确、子进程处于就绪状态。

## 常见陷阱

**忘记设置返回值** - 子进程必须返回 0，父进程返回子进程 PID。如果设置错误，用户程序无法区分自己是父进程还是子进程。

**资源泄漏** - fork 失败时必须释放已分配的资源（PCB、PID、地址空间等）。如果忘记释放，会导致内存泄漏。

**死锁** - fork 期间获取锁时要小心。如果子进程继承了锁的状态但不知道要释放，会导致死锁。我们的实现通过在 fork 后不持有任何锁来避免这个问题。

**竞态条件** - 在多核环境下，fork 可能和其他 CPU 上的进程操作竞争。我们的实现使用 `g_scheduler_lock` 来保护关键区域。

## 接下来

fork 实现了进程的创建，但进程还需要能退出。在下一篇文章中，我们会实现 exit 和 wait4 系统调用，学习如何处理进程退出、管理僵尸进程、回收资源。

exit 看起来简单：进程结束执行，释放资源。但实际上有很多细节需要处理：父进程需要知道子进程的退出状态，僵尸进程需要被回收，孤儿进程需要被 init 进程收养。这些问题我们会在下一篇文章中详细讨论。

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 实现上下文切换汇编](./07_实现上下文切换汇编.md) | [实现exit和wait4 →](./09_实现exit和wait4.md)

</div>
