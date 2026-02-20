# 19-2 PCB 与 PID 分配器实现

## 前言

上一章我们把设计思路想清楚了，现在开始动手写代码。说实话，这一章是最基础的，但也是最繁琐的。PCB 结构和 PID 分配器虽然逻辑不算复杂，但涉及的数据结构比较多，字段定义需要非常仔细。任何一个字段定义错了，后续调试起来会非常痛苦，尤其是内存布局相关的字段。

## 环境说明

我们继续在 Stage 18 的代码基础上开发。确保你已经完成了所有前序阶段的工作，并且内核能够正常启动进入 shell。

本章需要创建的新文件：
- kernel/process/process_defines.h - 进程类型定义
- kernel/process/process.h - 进程管理接口
- kernel/process/process.c - 进程管理实现

需要修改的文件：
- kernel/CMakeLists.txt - 添加进程模块到构建系统

## 从头定义进程类型

首先我们创建 process_defines.h 文件，定义进程相关的类型和常量。

```c
/* kernel/process/process_defines.h */

#pragma once

#include "defines/types.h"

/* 进程状态枚举 */
typedef enum process_state {
    PROC_READY    = 0,    /* 就绪状态，在运行队列中 */
    PROC_RUNNING  = 1,    /* 正在运行 */
    PROC_BLOCKED  = 2,    /* 阻塞状态 */
    PROC_ZOMBIE   = 3,    /* 僵尸状态 */
} process_state_t;
```

这个枚举定义了进程的四种状态。值的分配从 0 开始递增，这样方便后面做数组索引或者比较操作。

接下来定义一些常量：

```c
/* 进程相关常量 */
#define PID_MAX           32768      /* 最大进程数 */
#define KERNEL_STACK_SIZE (16 * 1024) /* 内核栈大小 16KB */
#define USER_STACK        (USER_END - PAGE_SIZE) /* 用户栈位置 */
```

PID_MAX 设为 32768，这意味着系统最多可以有 32767 个用户进程（PID 0 保留）。内核栈大小设为 16KB，这对于大部分内核操作来说足够了。如果栈溢出，通常是代码设计有问题（比如无限递归），而不是栈太小。

## 定义 PCB 结构

PCB 结构是整个进程管理的核心，我们在 process.h 中定义：

```c
/* kernel/process/process.h */

#pragma once

#include "process_defines.h"
#include "defines/types.h"
#include "list/list.h"
#include "mm/vmm/vmm_config.h"
#include "mm/page_config.h"
#include "sync/atomic.h"

/* CPU 上下文结构 */
typedef struct PACKED cpu_context {
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;        /* 栈指针 */
    uint64_t rip;        /* 返回地址 */
} cpu_context_t;

/* 内存上下文结构 */
typedef struct memory_context {
    physical_addr_t pml4_phys;    /* 页表物理地址 */
    virtual_addr_t  brk;          /* 程序断点 */
    virtual_addr_t  stack_start;  /* 用户栈起始地址 */
} memory_context_t;

/* 进程控制块 */
typedef struct pcb {
    /* 进程标识 */
    int32_t            pid;            /* 进程 ID */
    int32_t            ppid;           /* 父进程 ID */

    /* 进程状态 */
    process_state_t    state;          /* 当前状态 */
    int32_t            exit_code;      /* 退出码 */

    /* 调度相关链表 */
    list_head          run_list;       /* 运行队列链表 */
    list_head          siblings;       /* 兄弟链表（在父进程的 children 中） */
    list_head          children;       /* 子进程链表 */
    list_head          zombie_children;/* 僵尸子进程链表 */

    /* 内存上下文 */
    memory_context_t   mm;             /* 地址空间信息 */

    /* CPU 上下文 */
    cpu_context_t*     cpu_ctx;        /* 保存的 CPU 上下文 */
    virtual_addr_t     kernel_stack;   /* 内核栈顶 */
    virtual_addr_t     kernel_stack_base; /* 内核栈底 */

    /* 用户模式相关 */
    bool               is_user_mode;   /* 是否用户进程 */
    virtual_addr_t     user_stack;     /* 用户栈顶 */
    size_t             user_stack_size;/* 用户栈大小 */

    /* 父子关系 */
    struct pcb*        parent;         /* 父进程指针 */

    /* 统计信息 */
    uint64_t           start_time;     /* 创建时间 */
    char               comm[16];       /* 命令名 */

    /* 线程支持（预留） */
    int32_t            tgid;           /* 线程组 ID */
    bool               is_thread;      /* 是否线程 */
    list_head          thread_list;    /* 线程组链表 */
    list_head          thread_group;   /* 在线程组中的位置 */

    /* 地址空间引用计数 */
    atomic_t           mm_refcount;    /* mm 结构的引用计数 */
} pcb_t;
```

这个结构看起来很长，但每个字段都有明确的用途。我来解释几个关键点：

首先，cpu_ctx 为什么是指针而不是内嵌结构？因为上下文切换时我们需要交换整个 CPU 上下文，如果是指针，只需要交换指针值即可。而且这样可以让 PCB 结构更紧凑。

其次，内核栈为什么要同时保存 base 和 top？base 用于释放内存时使用，top 用于设置 TSS 的 RSP0 字段。栈是从高地址向低地址增长的，所以 top = base + size。

第三，为什么有多个链表头？run_list 用于调度队列，children 管理子进程，zombie_children 管理已退出的子进程。siblings 用于把进程挂到父进程的 children 链表中。

## 实现 PID 分配器

PID 分配器使用位图实现。位图是一个位数组，每一位对应一个 PID。如果某位为 1，表示该 PID 已被占用；如果为 0，表示该 PID 可用。

```c
/* kernel/process/process.c */

#define PID_BITMAP_SIZE ((PID_MAX + 7) / 8)

static byte_t s_pid_bitmap_buffer[PID_BITMAP_SIZE];
static struct bitmap s_pid_bitmap;

/* 自旋锁保护 PID 分配 */
static spinlock_t s_pid_lock = SPIN_LOCK_INIT;

/**
 * @brief 初始化 PID 分配器
 */
void pid_alloc_init(void) {
    bitmap_init(&s_pid_bitmap, s_pid_bitmap_buffer, PID_MAX);
    bitmap_set(&s_pid_bitmap, 0); /* 保留 PID 0 */
}
```

初始化函数很简单，就是初始化位图结构，然后手动把 PID 0 设为占用状态。

分配函数需要注意边界检查：

```c
/**
 * @brief 分配一个新的 PID
 * @return 分配的 PID，或 -1 表示失败
 */
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

这里有个坑：bitmap_find_first_zero() 返回 -1 时表示没有找到空闲位。但我们还需要检查返回值是否超过 PID_MAX，防止数组越界。这个边界检查千万别漏，不然某个极端情况可能会让你崩溃好半天。

释放函数相对简单，但也要做边界检查：

```c
/**
 * @brief 释放一个 PID
 * @param pid 要释放的 PID
 */
void pid_free(int32_t pid) {
    if (pid > 0 && pid < PID_MAX) {
        spinlock_flags_t flags;
        spin_lock_irqsave(&s_pid_lock, &flags);
        bitmap_clear(&s_pid_bitmap, pid);
        spin_unlock_irqrestore(&s_pid_lock, flags);
    }
}
```

注意这里只释放大于 0 的 PID。PID 0 是保留的，不应该被释放。如果有人误调用 pid_free(0)，这个函数会直接忽略，不会破坏系统约定。

## 实现 PCB 分配与释放

PCB 的分配涉及多个步骤：分配 PCB 结构本身、分配 CPU 上下文、分配内核栈、初始化各种字段。

```c
/**
 * @brief 分配一个新的 PCB
 * @return 指向新 PCB 的指针，或 NULL 表示失败
 */
pcb_t* proc_alloc_pcb(void) {
    /* 1. 分配 PCB 结构 */
    pcb_t* pcb = (pcb_t*)kmalloc(sizeof(pcb_t));
    if (!pcb) {
        return NULL;
    }

    /* 2. 清零 */
    memset(pcb, 0, sizeof(pcb_t));

    /* 3. 分配 CPU 上下文 */
    pcb->cpu_ctx = (cpu_context_t*)kmalloc(sizeof(cpu_context_t));
    if (!pcb->cpu_ctx) {
        kfree(pcb);
        return NULL;
    }
    memset(pcb->cpu_ctx, 0, sizeof(cpu_context_t));

    /* 4. 分配内核栈 */
    pcb->kernel_stack_base = (virtual_addr_t)kmalloc(KERNEL_STACK_SIZE);
    if (!pcb->kernel_stack_base) {
        kfree(pcb->cpu_ctx);
        kfree(pcb);
        return NULL;
    }
    /* 栈从高地址向低地址增长，栈顶 = 基址 + 大小 */
    pcb->kernel_stack = pcb->kernel_stack_base + KERNEL_STACK_SIZE;

    /* 5. 初始化链表头 */
    INIT_LIST_HEAD(&pcb->run_list);
    INIT_LIST_HEAD(&pcb->siblings);
    INIT_LIST_HEAD(&pcb->children);
    INIT_LIST_HEAD(&pcb->zombie_children);

    /* 6. 初始化用户模式字段 */
    pcb->is_user_mode = false;
    pcb->user_stack = 0;
    pcb->user_stack_size = 0;

    /* 7. 初始化线程字段 */
    pcb->tgid = 0;
    pcb->is_thread = false;
    INIT_LIST_HEAD(&pcb->thread_list);
    INIT_LIST_HEAD(&pcb->thread_group);
    atomic_write(&pcb->mm_refcount, 1);

    return pcb;
}
```

这个函数有一个很重要的错误处理模式：每一步分配都可能失败，失败时需要释放之前已经分配的资源。注意看这个顺序：先分配 PCB，再分配 cpu_ctx，最后分配内核栈。释放时就要按相反顺序来。

内核栈的对齐问题值得注意。kmalloc 返回的地址通常是对齐的，但我们应该显式验证一下。16 字节对齐是 x86_64 ABI 的要求，SSE/AVX 指令需要这个对齐。

PCB 释放函数要处理更多的资源清理：

```c
/**
 * @brief 释放一个 PCB
 * @param pcb 要释放的 PCB
 */
void proc_free_pcb(pcb_t* pcb) {
    if (!pcb) {
        return;
    }

    /* 1. 释放 CPU 上下文 */
    if (pcb->cpu_ctx) {
        kfree(pcb->cpu_ctx);
    }

    /* 2. 释放内核栈 */
    if (pcb->kernel_stack_base) {
        kfree((void*)pcb->kernel_stack_base);
    }

    /* 3. 释放用户栈（如果是用户进程） */
    if (pcb->is_user_mode && pcb->user_stack != 0 && pcb->user_stack_size > 0) {
        size_t stack_pages = pcb->user_stack_size / PAGE_SIZE;
        for (size_t i = 0; i < stack_pages; i++) {
            virtual_addr_t vaddr = pcb->user_stack + (i * PAGE_SIZE);
            physical_addr_t paddr;
            if (page_virt_to_phys(pcb->mm.pml4_phys, vaddr, &paddr) == PAGE_OK) {
                pframe_free(paddr);
            }
            page_unmap_page(pcb->mm.pml4_phys, vaddr, false);
        }
    }

    /* 4. 销毁地址空间（使用引用计数） */
    if (pcb->mm.pml4_phys != 0) {
        if (atomic_dec_and_test(&pcb->mm_refcount)) {
            vmm_destroy_user_space(pcb->mm.pml4_phys);
        }
    }

    /* 5. 释放 PCB 结构本身 */
    kfree(pcb);
}
```

这里有个设计细节：地址空间的销毁使用引用计数。这是因为线程可以共享地址空间，只有当最后一个线程退出时才真正销毁地址空间。

## 进程查找函数

在开发阶段，我们实现一个简单的线性查找函数：

```c
/**
 * @brief 根据 PID 查找进程
 * @param pid 要查找的进程 ID
 * @return 指向 PCB 的指针，或 NULL 表示未找到
 */
pcb_t* proc_find(int32_t pid) {
    /* 先检查当前进程 */
    if (scheduler.current && scheduler.current->pid == pid) {
        return scheduler.current;
    }

    /* 遍历运行队列 */
    pcb_t* proc;
    list_for_each_entry(proc, &scheduler.run_queue, run_list) {
        if (proc->pid == pid) {
            return proc;
        }
    }

    return NULL;
}
```

这个函数的时间复杂度是 O(n)，对于当前的简单调度器来说足够了。如果以后需要支持更多进程，可以改用哈希表实现 O(1) 查找。

## 编译验证

现在我们把代码加入到构建系统中。修改 kernel/CMakeLists.txt：

```cmake
# 添加进程模块
add_subdirectory(process)

# 链接进程库
target_link_libraries(kernel PRIVATE process)
```

创建 kernel/process/CMakeLists.txt：

```cmake
# 进程管理模块

add_library(process OBJECT
    process.c
    switch.s
)

target_include_directories(process PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${KERNEL_SOURCE_DIR}
)
```

现在我们编译一下看看：

```bash
mkdir -p build && cd build
cmake ..
make -j4
```

如果一切正常，你应该看到编译成功，没有错误。如果有错误，仔细检查一下头文件包含路径和函数声明是否匹配。

## 踩坑预警

开发过程中有几个常见的坑，这里提前说一下：

第一个坑是 PID 0 的处理。PID 0 是保留的，不应该分配给用户进程。如果忘记在初始化时设置 bitmap_set(&s_pid_bitmap, 0)，可能会导致 PID 0 被意外分配。

第二个坑是内核栈对齐。x86_64 ABI 要求栈指针 16 字节对齐。如果 kmalloc 返回的地址没有正确对齐，可能导致某些指令（如 SSE 指令）触发异常。解决办法是在分配后显式检查对齐，或者在内核栈分配时使用对齐的分配函数。

第三个坑是 PCB 字段顺序。process.h 中定义的结构体顺序必须与 switch.s 中的汇编偏移量一致。如果两者不匹配，上下文切换会读写错误的字段，导致系统崩溃。我们会在下一章详细讨论这个问题。

## 验证输出

暂时没有测试程序，但我们可以通过打印日志来验证初始化是否成功。在主函数中添加：

```c
/* 在 kernel_main() 中 */
klog_info("[PROC] Initializing process subsystem...\n");
proc_init();
klog_info("[PROC] Process subsystem initialized\n");
```

编译运行后，你应该能在串口输出中看到这两行日志。

## 下一步

这一章我们实现了 PCB 结构和 PID 分配器，这是进程管理的基础设施。下一章我们会实现上下文切换的汇编代码，这是最底层也最关键的部分。说实话，写汇编代码的时候一定要非常小心，一个错误的偏移量或者一条错误的指令都可能导致难以调试的问题。

我们会在下一章详细讨论 x86_64 调用约定、callee-saved 寄存器、TSS RSP0 的作用，以及为什么上下文切换必须用汇编实现。
