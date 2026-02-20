# 实现 exit 和 wait4

## 前言

进程可以创建，也必须能够退出。exit 系统调用让进程主动结束执行，wait4 让父进程等待子进程退出并回收其资源。这两个系统调用是进程生命周期管理的关键。

exit 看起来很简单：进程说"我不玩了"，然后释放资源，结束执行。但实际上有很多细节需要处理。进程退出后变成"僵尸"，它的 PCB 还保留着，用于存储退出状态。父进程通过 wait4 收集这个状态，然后彻底回收僵尸进程。

如果父进程不 wait，子进程就会一直是僵尸状态，浪费系统资源。更糟糕的是，如果父进程先退出，子进程就成了"孤儿"，需要被 init 进程（PID 1）收养。这些边界条件需要仔细处理。

## exit 的工作流程

让我们先理解 exit 的完整流程：

1. 设置进程状态为 ZOMBIE
2. 保存退出码
3. 从运行队列移除
4. 唤醒等待的父进程
5. 将孤儿子进程移交给 init
6. 调度下一个进程

注意 PCB 不会在 exit 时立即释放，而是等到父进程 wait4 时才释放。这是因为父进程需要访问退出状态，而退出状态存储在 PCB 中。

## 实现 proc_exit

```c
void proc_exit(int exit_code) {
    pcb_t* current = proc_current();
    if (!current) {
        klog_error("[PROC] No current process to exit\n");
        /* Halt if no current process */
        while (1) {
            __asm__ volatile("hlt");
        }
    }

    klog_info("[PROC] Process %d exiting with code %d\n", current->pid, exit_code);
```

首先获取当前进程，如果没有当前进程说明出问题了。直接 halt 避免 further damage。

设置僵尸状态：

```c
    /* Set zombie state */
    current->state = PROC_ZOMBIE;
    current->exit_code = exit_code;

    /* Move to parent's zombie list */
    list_del(&current->run_list);
    if (current->parent) {
        list_add_tail(&current->zombie_children, &current->parent->zombie_children);
    }

    scheduler.nr_running--;
```

把进程标记为 ZOMBIE，从运行队列移除，加入到父进程的僵尸子进程链表。减少运行计数。

唤醒等待的父进程：

```c
    /* Wake up parent if it's waiting */
    if (current->parent && current->parent->state == PROC_BLOCKED) {
        current->parent->state = PROC_READY;
        sched_enqueue_task(current->parent, false);
    }
```

如果父进程处于 BLOCKED 状态（在 wait4 中等待），唤醒它。这只是简化的实现，更完善的版本应该使用等待队列而不是直接检查状态。

处理孤儿子进程：

```c
    /* Reparent children to init */
    pcb_t* child;
    pcb_t* next;
    list_for_each_entry_safe(child, next, &current->children, siblings) {
        child->parent = init_process;
        list_del(&child->siblings);
        list_add_tail(&child->siblings, &init_process->children);
    }
```

遍历当前进程的所有子进程，把它们重新父化到 init 进程（PID 1）。这样即使父进程退出，子进程也有父进程可以 wait。

调用调度器：

```c
    /* Schedule next process */
    schedule();

    /* Should never reach here */
    __builtin_unreachable();
}
```

调用 `schedule()` 选择下一个进程。当前进程已经是 ZOMBIE 状态，不会再被调度到。`__builtin_unreachable()` 告诉编译器这段代码永远不会执行，允许编译器做更多优化。

## wait4 的工作流程

wait4 让父进程等待子进程退出，并收集退出状态。它可以等待特定的子进程（通过 PID），也可以等待任意子进程（PID = -1）。

wait4 的完整流程：

1. 检查是否有僵尸子进程
2. 如果有，收集退出状态并回收 PCB
3. 如果没有，阻塞当前进程
4. 被唤醒后返回

## 实现 proc_wait4

```c
int32_t proc_wait4(int32_t pid, int* wstatus, int options) {
    (void)options; /* TODO: Implement options */

    pcb_t* current = proc_current();
    if (!current) {
        return -1;
    }
```

忽略 options 参数，完整的实现应该支持 WNOHANG（非阻塞）等选项。

查找僵尸子进程：

```c
    /* Check for zombie children */
    pcb_t* child;
    pcb_t* target = NULL;
    list_for_each_entry(child, &current->zombie_children, zombie_children) {
        if (pid == -1 || child->pid == pid) {
            target = child;
            break;
        }
    }
```

遍历僵尸子进程链表。如果 `pid` 是 -1，返回第一个找到的僵尸进程；否则只返回指定 PID 的进程。

收集退出状态：

```c
    if (target) {
        /* Found a zombie child */
        if (wstatus) {
            *wstatus = target->exit_code;
        }
        int32_t result = target->pid;

        /* Remove from zombie list and free */
        list_del(&target->zombie_children);
        proc_free_pcb(target);

        return result;
    }
```

如果找到僵尸进程，把退出码写入 `wstatus`（如果调用者提供了指针），然后从链表移除并释放 PCB。返回子进程的 PID。

阻塞当前进程：

```c
    /* No zombie children - block current */
    /* TODO: Implement proper blocking with wakeup */
    current->state = PROC_BLOCKED;
    schedule();

    return -1; /* Will be resumed when child exits */
}
```

如果没有僵尸子进程，把当前进程标记为 BLOCKED，调用 `schedule()` 让出 CPU。当有子进程退出时，父进程会被唤醒，重新执行 wait4。

这个实现有一个问题：被唤醒后直接返回 -1，而没有重新检查僵尸子进程。更完善的实现应该用一个循环，在唤醒后重新查找僵尸进程。

## 系统调用包装

exit 和 wait4 最终通过系统调用被用户程序调用：

```c
int64_t sys_exit(syscall_frame_t* frame) {
    int exit_code = (int)frame->arg0;
    proc_exit(exit_code);
    /* Never returns */
    __builtin_unreachable();
}

int64_t sys_wait4(syscall_frame_t* frame) {
    int32_t pid = (int32_t)frame->arg0;
    int* wstatus = (int*)frame->arg1;
    int options = (int)frame->arg2;
    return proc_wait4(pid, wstatus, options);
}

int64_t sys_getpid(syscall_frame_t* frame) {
    (void)frame;
    pcb_t* current = proc_current();
    return current ? current->pid : -1;
}

int64_t sys_getppid(syscall_frame_t* frame) {
    (void)frame;
    pcb_t* current = proc_current();
    return current ? current->ppid : -1;
}
```

exit 是 noreturn 函数，永远不会返回。getpid 和 getppid 是简单的 getter，返回当前进程的 PID 和父进程 PID。

## 孤儿进程和 init 进程

如果父进程在子进程之前退出，子进程就成了孤儿。孤儿进程需要被重新父化到 init 进程（PID 1），这样它们就有父进程可以 wait。

init 进程是系统启动后创建的第一个进程，它的职责之一就是收养孤儿进程。当 init 调用 wait4 时，会回收所有僵尸子进程。

在我们的实现中，`proc_exit()` 会把所有子进程重新父化到 init。但这需要有一个全局的 `init_process` 指针。更完善的实现应该维护一个进程表，可以通过 PID 查找 init 进程。

## 僵尸进程的危害

僵尸进程虽然已经退出，但它的 PCB 还保留着。如果父进程忘记 wait，僵尸进程会一直占用内存和 PID 资源。在长时间运行的系统中，僵尸进程积累多了会导致 PID 耗尽，无法创建新进程。

查看僵尸进程的方法：

```bash
# 在 Linux 上
ps aux | grep Z
```

预防僵尸进程的方法：
1. 父进程 always wait，不遗漏任何子进程
2. 使用信号处理（SIGCHLD）自动回收
3. 双层 fork：子进程立即 exit，孙进程被 init 收养

## 测试 exit 和 wait4

让我们写一个测试来验证 exit 和 wait4：

```c
bool test_exit_wait(void) {
    klog_info("[PROC_DEMO] Test: Exit and Wait...\n");

    /* Create parent and child */
    pcb_t* parent = proc_alloc_pcb();
    pcb_t* child = proc_alloc_pcb();
    if (!parent || !child) {
        klog_error("[PROC_DEMO] Failed to allocate PCBs\n");
        return false;
    }

    parent->pid = 1;
    parent->ppid = 0;
    parent->state = PROC_RUNNING;
    INIT_LIST_HEAD(&parent->children);
    INIT_LIST_HEAD(&parent->zombie_children);

    child->pid = 2;
    child->ppid = 1;
    child->state = PROC_READY;
    child->parent = parent;

    list_add_tail(&child->siblings, &parent->children);

    /* Simulate child exit */
    scheduler.current = child;
    child->state = PROC_ZOMBIE;
    child->exit_code = 42;
    list_del(&child->run_list);
    list_add_tail(&child->zombie_children, &parent->zombie_children);

    /* Simulate parent wait */
    scheduler.current = parent;
    int wstatus;
    int32_t pid = proc_wait4(-1, &wstatus, 0);

    /* Verify */
    if (pid != 2) {
        klog_error("[PROC_DEMO] Wait returned wrong PID\n");
        return false;
    }

    if (wstatus != 42) {
        klog_error("[PROC_DEMO] Wait returned wrong exit code\n");
        return false;
    }

    klog_info("[PROC_DEMO] Exit and Wait test PASSED\n");
    return true;
}
```

## 常见陷阱

**忘记回收 PCB** - 如果在 wait4 中忘记调用 `proc_free_pcb()`，会导致内存泄漏。僵尸进程的 PCB 会一直占用内存。

**双重回收** - 如果 wait4 被调用多次，可能会尝试释放同一个 PCB。PCB 释放后访问会导致 use-after-free bug。更完善的实现应该在 PCB 中添加一个"已释放"标志。

**竞态条件** - 在 exit 和 wait4 之间可能有竞态条件。如果子进程在父进程检查僵尸列表之后退出，父进程可能会错误地进入阻塞状态。这需要用锁或原子操作来保护。

**死锁** - 如果父进程在持有锁的情况下调用 wait4，而子进程退出时需要获取同一个锁，可能会导致死锁。我们的实现通过不在 exit 时获取任何锁来避免这个问题。

## 接下来

exit 和 wait4 完成了进程的生命周期管理。现在我们可以创建进程（fork）、退出进程（exit）、等待子进程（wait4）。这些是进程管理的核心功能。

接下来的扩展是线程支持。线程是轻量级的执行流，共享进程的地址空间但有独立的栈和寄存器状态。线程可以让我们在同一个进程内并行执行多个任务，提高并发性能。

在下一篇文章中，我们会实现线程管理，学习如何创建内核线程和用户线程，如何实现 join 和 detach 操作。

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 实现fork系统调用](./08_实现fork系统调用.md) | [实现线程管理 →](./10_实现线程管理.md)

</div>
