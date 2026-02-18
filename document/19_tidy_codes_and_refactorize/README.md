# CCOS 进程管理与代码重构 文档中心

本目录包含 CCOS Stage 19 - 进程管理与代码重构的完整文档体系。

---

## 阶段概述

**Stage 19: 进程管理与代码重构 (Process Management & Code Refactoring)**

本阶段在 Stage 18 写时复制与异常处理基础上，实现了完整的进程管理子系统，包括进程控制块（PCB）、上下文切换、调度器和进程相关系统调用。同时进行了代码重构，修复了堆与 VMM 地址空间冲突问题。

### 核心成果

- **进程管理模块** ([`kernel/process/process.h`](../../kernel/process/process.h))
  - 进程控制块（PCB）结构与进程状态管理
  - 基于 bitmap 的 PID 分配器
  - 进程创建（fork）、退出（exit）、等待（wait4）实现
  - 汇编实现的上下文切换
  - 简单 Round-Robin 调度器

- **上下文切换实现** ([`kernel/process/switch.s`](../../kernel/process/switch.s))
  - `switch_context()` - 进程间上下文切换
  - `switch_to_first()` - 从内核切换到首个进程
  - callee-saved 寄存器保存与恢复
  - TSS RSP0 与 CR3 切换

- **TSS 扩展** ([`kernel/interrupt/tss.h`](../../kernel/interrupt/tss.h))
  - 新增 `tss_set_kernel_stack_ctx()` 函数
  - 支持进程上下文切换时的内核栈更新

- **VMM 扩展** ([`kernel/mm/vmm/vmm.h`](../../kernel/mm/vmm/vmm.h))
  - 新增 `vmm_load_pml4()` 函数
  - 支持地址空间切换

- **系统调用实现** ([`kernel/syscall/syscall_table.c`](../../kernel/syscall/syscall_table.c))
  - `sys_fork` - 创建新进程
  - `sys_exit` - 退出当前进程
  - `sys_wait4` - 等待子进程退出
  - `sys_getpid` - 获取进程 ID
  - `sys_getppid` - 获取父进程 ID

- **进程演示程序** ([`kernel/demo/process_simple/process_demo.h`](../../kernel/demo/process_simple/process_demo.h))
  - PID 分配测试
  - PCB 分配测试
  - 进程状态测试
  - 调度器初始化测试
  - PCB 列表管理测试
  - 内核栈管理测试
  - 内存上下文测试

---

## 目录结构

```
kernel/
├── process/
│   ├── process.h              # 进程管理接口
│   ├── process.c              # 进程管理实现
│   ├── process_defines.h      # 进程类型定义
│   ├── switch.s               # 上下文切换汇编
│   └── CMakeLists.txt         # 构建配置
├── demo/
│   └── process_simple/
│       ├── process_demo.h     # 进程演示接口
│       └── process_demo.c     # 进程演示实现
├── syscall/
│   └── syscall_table.c        # 系统调用表（修改）
├── interrupt/
│   ├── tss.h                  # TSS 接口（修改）
│   └── tss.c                  # TSS 实现（修改）
├── mm/
│   └── vmm/
│       ├── vmm.h              # VMM 接口（修改）
│       └── vmm.c              # VMM 实现（修改）
└── stacktrace/
    ├── symbols.h              # 符号表（修改）
    └── symbols.c              # 符号表实现（修改）
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要进程管理
- PCB 结构设计决策
- PID 分配方案选择
- 上下文切换实现原理
- 调度器设计思路
- 常见陷阱与注意事项（包含堆/VMM 地址冲突修复）
- 未来改进方向

**适合**:
- 理解设计思路
- 学习进程管理架构
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- PCB 结构详解
- CPU 上下文与陷阱帧
- 进程状态定义
- API 完整参考
- 上下文切换详解
- 系统调用实现
- 内存布局
- 符号表更新

**适合**:
- 查询 API 用法
- 理解数据结构
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 问题诊断

**内容**:
- 进程创建问题
- 上下文切换失败
- 调度器问题
- 内存管理问题
- PID 分配器问题
- 僵尸进程处理
- 堆/VMM 地址冲突（详细解决方案）

每个问题按"症状-原因-解决方案"格式组织。

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

### 4. [调试工具指南](./调试工具指南.md) 工具使用

**内容**:
- 进程状态查看方法
- PID 分配器调试
- 上下文切换调试
- TSS/VMM 调试
- GDB 调试技巧
- QEMU Monitor 使用
- 日志分析
- 性能分析

**适合**:
- 学习调试工具
- 查询具体命令
- 提升调试效率

---

## 快速开始

### 查看代码结构

1. **进程管理接口** → 查看 [`kernel/process/process.h`](../../kernel/process/process.h)
2. **上下文切换实现** → 查看 [`kernel/process/switch.s`](../../kernel/process/switch.s)
3. **系统调用实现** → 查看 [`kernel/syscall/syscall_table.c`](../../kernel/syscall/syscall_table.c)
4. **进程演示** → 查看 [`kernel/demo/process_simple/process_demo.c`](../../kernel/demo/process_simple/process_demo.c)

### 使用示例

```c
#include "process/process.h"

// 内核初始化时调用
void kernel_init(void) {
    // 初始化进程子系统
    proc_init();

    // PID 分配器在 proc_init() 中自动初始化
}

// 创建新进程 (fork)
void fork_example(void) {
    int32_t pid = proc_fork();

    if (pid == 0) {
        // 子进程
        klog_info("Child process\n");
    } else if (pid > 0) {
        // 父进程
        klog_info("Parent process, child PID: %d\n", pid);
    } else {
        // fork 失败
        klog_error("Fork failed\n");
    }
}

// 退出进程
void exit_example(void) {
    proc_exit(0);  // 退出码为 0
}

// 等待子进程
void wait_example(void) {
    int status;
    int32_t pid = proc_wait4(-1, &status, 0);  // 等待任意子进程
    if (pid > 0) {
        klog_info("Child %d exited with status %d\n", pid, status);
    }
}

// 获取当前进程
void current_process_example(void) {
    pcb_t* current = proc_current();
    if (current) {
        klog_info("Current PID: %d\n", current->pid);
        klog_info("Parent PID: %d\n", current->ppid);
        klog_info("State: %d\n", current->state);
    }
}

// 查找进程
pcb_t* find_process_example(int32_t pid) {
    return proc_find(pid);
}
```

### PID 分配器使用示例

```c
#include "process/process.h"

// PID 分配器由进程子系统自动管理
// 通常不需要直接调用，以下为底层 API 示例

void pid_allocator_example(void) {
    // 初始化（通常由 proc_init 调用）
    pid_alloc_init();

    // 分配 PID
    int32_t pid = pid_alloc();  // 返回分配的 PID，或 -1 表示失败

    // 释放 PID
    pid_free(pid);
}
```

---

## 与前一阶段对比

| 特性 | Stage 18 (COW 与异常处理) | Stage 19 (进程管理与代码重构) |
|------|-------------------------|----------------------------|
| 进程管理 | 无 | 完整 PCB 管理 |
| PID 分配 | 无 | 基于 bitmap 的分配器 |
| 上下文切换 | 无 | 汇编实现 |
| 调度器 | 无 | Round-Robin 调度器 |
| fork 系统调用 | 框架仅 | 完整实现 |
| 进程状态 | 无 | READY/RUNNING/BLOCKED/ZOMBIE |
| TSS 扩展 | 基础功能 | 新增进程切换支持 |
| VMM 扩展 | 基础映射 | 新增地址空间切换 |
| 新增文件 | - | 8+ 个 |

---

## 技术亮点

### 1. 基于 Bitmap 的 PID 分配器

使用位图实现高效的 PID 分配与回收：

```c
#define PID_MAX 32768

// 每个位对应一个 PID
static byte_t s_pid_bitmap_buffer[PID_BITMAP_SIZE];
static struct bitmap s_pid_bitmap;

// O(1) 分配
int32_t pid_alloc(void) {
    int pid = bitmap_find_first_zero(&s_pid_bitmap);
    if (pid >= 0 && pid < PID_MAX) {
        bitmap_set(&s_pid_bitmap, pid);
        return pid;
    }
    return -1;
}
```

### 2. 汇编实现的上下文切换

精确控制寄存器保存与恢复：

```asm
; switch_context - 保存当前进程，恢复下一个进程
; 1. 保存 callee-saved 寄存器 (RBX, RBP, R12-R15)
; 2. 更新 TSS.RSP0
; 3. 切换 CR3 (地址空间)
; 4. 恢复下一个进程的寄存器
; 5. 返回到下一个进程
```

### 3. 简单高效的调度器

Round-Robin 调度算法：

```c
typedef struct scheduler {
    list_head     run_queue;    // 运行队列
    pcb_t*        current;      // 当前进程
    pcb_t*        idle;         // 空闲进程
    uint32_t      nr_running;   // 运行进程数
    bool          need_resched; // 重调度标志
} scheduler_t;
```

### 4. 进程状态机设计

```
                fork()
    ┌─────────────────────────┐
    │                         ▼
┌───────┐   schedule()   ┌─────────┐
│ READY │ ──────────────▶ │ RUNNING │
└───────┘                 └─────────┘
    ▲                        │    │
    │                        │    │ sleep/block
    │                        │    ▼
    │                   ┌─────────┐
    │                   │ BLOCKED │
    │                   └─────────┘
    │                        │
    │                        │ wakeup
    │                        ▼
    └────────────────────────┘
         │
         │ exit()
         ▼
    ┌─────────┐
    │ ZOMBIE  │
    └─────────┘
         │
         │ wait4()
         ▼
      (回收)
```

### 5. 堆与 VMM 地址空间分离

修复了堆分配器与通用内存分配的地址冲突：

```c
/* 原有设计 - 两者使用同一地址范围 */
#define KERNEL_HEAP_BASE    0xFFFFFFFF81000000ULL
// vmm_alloc_pages() 也从 KERNEL_HEAP_BASE 开始搜索

/* 修复后 - 分离为两个独立区域 */
#define KERNEL_HEAP_BASE    0xFFFFFFFF81000000ULL  // 堆专用
#define KERNEL_GENERAL_BASE 0xFFFFFFFF88000000ULL  // 通用分配
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

- **阶段**: Stage 19
- **前序提交**: `4eee7ca` - syscall framework
- **当前提交**: `0c9b114` - process support
- **日期**: 2026-02-18
- **作者**: Charliechen114514

---

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../18_cow_exception_handle/README.md](../18_cow_exception_handle/) - 上一阶段文档

### 源码文件
- [`kernel/process/process.h`](../../kernel/process/process.h) - 进程管理接口
- [`kernel/process/process.c`](../../kernel/process/process.c) - 进程管理实现
- [`kernel/process/switch.s`](../../kernel/process/switch.s) - 上下文切换汇编
- [`kernel/interrupt/tss.h`](../../kernel/interrupt/tss.h) - TSS 管理接口
- [`kernel/mm/vmm/vmm.h`](../../kernel/mm/vmm/vmm.h) - VMM 接口
- [`kernel/syscall/syscall_table.c`](../../kernel/syscall/syscall_table.c) - 系统调用实现
- [`kernel/demo/process_simple/process_demo.h`](../../kernel/demo/process_simple/process_demo.h) - 进程演示

### 外部参考
- [x86_64 System V ABI](https://gitlab.com/x86-psABIs/x86-64-ABI)
- [Process Management](https://wiki.osdev.org/Process_Management)
- [Context Switching](https://wiki.osdev.org/Context_Switching)
- [Scheduler](https://wiki.osdev.org/Scheduler_Algorithms)
- [OSDev.org](https://wiki.osdev.org)

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-18
