# CCOS 简单进程管理文档中心

本目录包含 CCOS Stage 21 - 简单进程管理开发的完整文档体系。

---

## 阶段概述

**Stage 21: 简单进程管理**

本阶段在 Stage 20 系统调用框架基础上，实现了基础进程管理功能，标志着操作系统从单线程内核向多进程系统的跨越。实现了进程创建、调度、退出和等待等核心功能，为多任务处理奠定了基础。

### 核心成果

- **进程管理模块** ([`kernel/process/process.h`](../../kernel/process/process.h))
  - 进程控制块 (PCB) 管理
  - 进程状态机（就绪、运行、阻塞、僵尸）
  - 进程树管理（父子关系、兄弟关系）
  - PID 分配器（基于 bitmap）

- **进程调度器** ([`kernel/process/process.c`](../../kernel/process/process.c))
  - 简单轮转调度
  - 运行队列管理
  - 上下文切换协调

- **上下文切换** ([`kernel/process/switch.s`](../../kernel/process/switch.s))
  - 汇编实现的上下文切换
  - 寄存器保存/恢复
  - 地址空间切换 (CR3)
  - TSS 内核栈切换

- **内存隔离** ([`kernel/mm/vmm/vmm.c`](../../kernel/mm/vmm/vmm.c))
  - 每进程独立地址空间
  - 用户地址空间创建/销毁
  - PML4 切换

- **系统调用扩展** ([`kernel/syscall/syscall_table.c`](../../kernel/syscall/syscall_table.c))
  - fork() - 创建进程
  - exit() - 退出进程
  - wait4() - 等待子进程
  - getpid() - 获取进程 ID
  - getppid() - 获取父进程 ID

- **进程演示程序** ([`kernel/demo/process_simple/process_demo.h`](../../kernel/demo/process_simple/process_demo.h))
  - PID 分配测试
  - PCB 管理测试
  - 进程状态转换测试
  - 调度器测试

---

## 目录结构

```
kernel/
├── process/
│   ├── process.h                # 进程管理接口
│   ├── process.c                # 进程管理实现
│   ├── process_defines.h        # 进程相关定义
│   ├── switch.s                 # 上下文切换汇编
│   └── CMakeLists.txt          # 构建配置
├── demo/
│   ├── process_simple/
│   │   ├── process_demo.h      # 进程演示接口
│   │   └── process_demo.c      # 进程演示实现
│   └── CMakeLists.txt          # 演示构建配置
├── interrupt/
│   ├── tss.c                   # TSS 内核栈支持
│   └── tss.h                   # TSS 接口
├── mm/
│   └── vmm/
│       ├── vmm.c               # 用户地址空间管理
│       └── vmm.h               # VMM 接口
└── syscall/
    └── syscall_table.c         # 新增进程系统调用
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要进程管理
- 进程管理设计基础（进程状态、调度算法）
- 设计决策（简化 fork、bitmap PID、独立地址空间）
- 架构设计（模块协作、PCB 结构、调度器）
- 实现细节（初始化、fork、上下文切换、exit、wait4）
- 常见陷阱（僵尸进程、内存泄漏、竞态条件）
- 未来改进（写时复制、抢占式调度、信号）

**适合**:
- 理解设计思路
- 学习进程管理原理
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- 进程状态定义
- Process API 完整参考
- PID API 参考
- 数据结构定义（pcb_t、scheduler_t、cpu_context_t）
- 常量定义
- 算法说明（PID 分配、上下文切换、进程调度）
- 系统调用映射

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 问题诊断

**内容**:
- 初始化问题
- 进程创建问题（fork 失败、资源不足）
- 进程退出问题（僵尸进程、孤儿进程）
- 上下文切换问题（栈损坏、寄存器错误）
- 调试技巧（GDB 命令、日志分析）

每个问题按"症状-原因-解决方案"格式组织。

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

## 快速开始

### 查看代码结构

1. **进程管理接口** → 查看 [`kernel/process/process.h`](../../kernel/process/process.h)
2. **进程管理实现** → 查看 [`kernel/process/process.c`](../../kernel/process/process.c)
3. **上下文切换** → 查看 [`kernel/process/switch.s`](../../kernel/process/switch.s)
4. **进程演示程序** → 查看 [`kernel/demo/process_simple/process_demo.h`](../../kernel/demo/process_simple/process_demo.h)

### 使用示例

```c
#include "process/process.h"

// 内核初始化时调用
void kernel_init(void) {
    // 确保在 VMM 初始化之后调用
    proc_init();
}

// 创建新进程 (fork 系统调用实现)
int32_t proc_fork(void) {
    // 1. 分配新 PID
    // 2. 分配新 PCB
    // 3. 创建新地址空间
    // 4. 复制/设置上下文
    // 5. 添加到运行队列
    // ...
}

// 退出当前进程
void proc_exit(int exit_code) {
    // 1. 设置退出状态
    // 2. 唤醒等待的父进程
    // 3. 调度下一个进程
    // ...
}

// 等待子进程
int32_t proc_wait4(int32_t pid, int* wstatus, int options) {
    // 1. 查找子进程
    // 2. 如果子进程未退出，阻塞当前进程
    // 3. 收集退出状态
    // 4. 清理僵尸进程
    // ...
}
```

### 进程状态转换

```
                    fork()
    ┌─────────────────────────────────┐
    │                                 │
    ▼                                 │
┌───────┐    schedule()    ┌─────────┐
│ READY │ ────────────────> │ RUNNING │
└────────↑                  └────┬────┘
    │                          │
    │                          │ exit()
    │                          ▼
    │                    ┌─────────┐
    │                    │  ZOMBIE │
    │                    └─────────┘
    │                          │
    │                          │ wait4()
    │                          ▼
    └────────────────────  (reaped)
```

---

## 与前一阶段对比

| 特性 | Stage 20 (系统调用框架) | Stage 21 (进程管理) |
|------|------------------------|---------------------|
| 执行模式 | 单线程内核 | 多进程支持 |
| 进程管理 | 无 | 完整 PCB 管理 |
| 调度器 | 无 | 简单轮转调度 |
| 地址空间 | 单一内核空间 | 每进程独立空间 |
| 上下文切换 | 无 | 汇编实现切换 |
| PID 管理 | 无 | bitmap 分配器 |
| 系统调用 | 基础框架 | fork/exit/wait4 等 |
| TSS 功能 | IST 栈 | + 内核栈切换 |
| 新增文件 | - | 10+ 个 |

---

## 技术亮点

### 1. 进程控制块 (PCB)

```c
typedef struct pcb {
    int32_t pid;                    // 进程 ID
    int32_t ppid;                   // 父进程 ID
    process_state_t state;          // 进程状态
    int32_t exit_code;              // 退出码

    list_head run_list;             // 运行队列链表
    list_head siblings;             // 兄弟进程链表
    list_head children;             // 子进程链表
    list_head zombie_children;      // 僵尸子进程链表

    memory_context_t mm;            // 内存上下文
    cpu_context_t* cpu_ctx;         // 保存的 CPU 上下文
    trap_frame_t* trap_frame;       // 用户态陷阱帧
    virtual_addr_t kernel_stack;    // 内核栈顶
    virtual_addr_t kernel_stack_base; // 内核栈底

    struct pcb* parent;             // 父进程指针
    uint64_t start_time;            // 创建时间
    char comm[16];                  // 命令名
} pcb_t;
```

### 2. 上下文切换

```assembly
# switch_context(pcb_t** prev, pcb_t* next)
# 保存当前进程状态并切换到下一个进程
switch_context:
    # 保存 callee-saved 寄存器
    mov %rax, (%rdi)
    mov %rbx, 8(%rdi)
    mov %rbp, 16(%rdi)
    mov %r12, 24(%rdi)
    mov %r13, 32(%rdi)
    mov %r14, 40(%rdi)
    mov %r15, 48(%rdi)
    movq (%rsp), %rax    # 保存 RIP
    mov %rax, 56(%rdi)
    movq %rsp, 64(%rdi)   # 保存 RSP

    # 切换 TSS 内核栈
    mov 72(%rsi), %rdx    # next->kernel_stack
    mov %rdx, %gs:0       # 设置 TSS.rsp0

    # 切换地址空间
    mov 76(%rsi), %rdx    # next->mm.pml4_phys
    mov %rdx, %cr3

    # 恢复下一个进程状态
    mov 64(%rsi), %rsp    # 恢复 RSP
    mov 56(%rsi), %rax    # 恢复 RIP
    push %rax
    mov 8(%rsi), %rbx
    mov 16(%rsi), %rbp
    mov 24(%rsi), %r12
    mov 32(%rsi), %r13
    mov 40(%rsi), %r14
    mov 48(%rsi), %r15

    ret
```

### 3. PID 分配器

```c
// 基于 bitmap 的 PID 分配
#define MAX_PID 256

static uint64_t pid_bitmap[MAX_PID / 64];  // 256 PID, 64 位 bitmap

int32_t pid_alloc(void) {
    for (int i = 0; i < MAX_PID / 64; i++) {
        if (pid_bitmap[i] != 0xFFFFFFFFFFFFFFFF) {
            // 找到空闲位
            int bit = __builtin_ctzll(~pid_bitmap[i]);
            pid_bitmap[i] |= (1ULL << bit);
            return i * 64 + bit;
        }
    }
    return -1;  // 没有可用 PID
}

void pid_free(int32_t pid) {
    if (pid > 0 && pid < MAX_PID) {
        pid_bitmap[pid / 64] &= ~(1ULL << (pid % 64));
    }
}
```

### 4. 独立地址空间

```c
typedef struct memory_context {
    physical_addr_t pml4_phys;    // PML4 物理地址
    virtual_addr_t brk;           // 程序断点（堆结束）
    virtual_addr_t stack_start;   // 用户栈起始地址
} memory_context_t;

// 创建新用户地址空间
vmm_result_t vmm_create_user_space(physical_addr_t* out_pml4) {
    // 1. 分配新的 PML4 页
    // 2. 复制内核映射
    // 3. 设置用户空间区域
    // ...
}

// 切换地址空间
void vmm_load_pml4(physical_addr_t pml4_phys) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}
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

- **阶段**: Stage 21
- **分支**: `stage/21_process_simple`
- **日期**: 2026-02-18
- **作者**: Charliechen114514

---

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../20_syscallframework/README.md](../20_syscallframework/) - 上一阶段文档

### 源码文件
- [`kernel/process/process.h`](../../kernel/process/process.h) - 进程管理接口
- [`kernel/process/process.c`](../../kernel/process/process.c) - 进程管理实现
- [`kernel/process/switch.s`](../../kernel/process/switch.s) - 上下文切换
- [`kernel/interrupt/tss.h`](../../kernel/interrupt/tss.h) - TSS 接口
- [`kernel/mm/vmm/vmm.h`](../../kernel/mm/vmm/vmm.h) - VMM 接口
- [`kernel/demo/process_simple/process_demo.h`](../../kernel/demo/process_simple/process_demo.h) - 演示程序

### 外部参考
- [X86-64 Context Switching](https://wiki.osdev.org/Context_Switching)
- [Process Management](https://wiki.osdev.org/Process_Management)
- [Task State Segment](https://wiki.osdev.org/Task_State_Segment)
- [OSDev.org](https://wiki.osdev.org)

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-18
