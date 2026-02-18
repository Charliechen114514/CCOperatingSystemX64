# CCOS 系统调用框架 文档中心

本目录包含 CCOS Stage 20 - 系统调用框架 (Syscall Framework) 开发的完整文档体系。

---

## 阶段概述

**Stage 20: 系统调用框架 (Syscall Framework)**

本阶段实现了完整的 x86_64 系统调用框架，为用户空间程序提供与内核交互的标准接口。系统调用是操作系统的核心功能，允许用户程序请求内核服务，如进程管理、文件 I/O、内存管理等。

### 核心成果

- **系统调用框架** ([`kernel/syscall/syscall.h`](../../kernel/syscall/syscall.h))
  - syscall/sysret 指令支持（x86_64 快速系统调用）
  - int 0x80 传统接口支持（向后兼容）
  - MSR 寄存器配置（IA32_LSTAR, IA32_STAR, IA32_FMASK）
  - 系统调用分发器
  - 系统调用统计信息
  - 动态注册机制

- **系统调用号定义** ([`kernel/syscall/syscall_numbers.h`](../../kernel/syscall/syscall_numbers.h))
  - 进程管理调用（fork, exit, wait4, getpid, getppid）
  - 文件 I/O 调用（write, read, open, close, lseek, ioctl）
  - 内存管理调用（brk, mmap, munmap）
  - 系统信息调用（uname, gettime）
  - 调试/测试调用（debug_log, test）

- **系统调用处理表** ([`kernel/syscall/syscall_table.c`](../../kernel/syscall/syscall_table.c))
  - 所有系统调用处理函数实现
  - 进程管理接口（fork, exit, wait4）
  - 基本文件 I/O（write）

- **汇编入口/出口** ([`kernel/syscall/syscall.asm`](../../kernel/syscall/syscall.asm))
  - syscall_handler：syscall 指令入口点
  - int0x80_handler：int 0x80 入口点
  - 栈对齐与寄存器保存/恢复
  - sysretq 返回用户模式

- **Mock 系统调用演示** ([`kernel/demo/mock_syscall/mock_syscall_demo.h`](../../kernel/demo/mock_syscall/mock_syscall_demo.h))
  - MSR 配置验证测试
  - 系统调用分发测试
  - 系统调用统计测试

---

## 目录结构

```
kernel/
├── syscall/
│   ├── syscall.h               # 系统调用框架接口
│   ├── syscall.c               # 系统调用框架实现
│   ├── syscall_numbers.h       # 系统调用号定义
│   ├── syscall_table.c         # 系统调用处理表
│   └── syscall.asm             # 系统调用汇编入口/出口
└── demo/
    └── mock_syscall/
        ├── mock_syscall_demo.h # Mock 系统调用演示接口
        └── mock_syscall_demo.c # Mock 系统调用演示实现
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要系统调用框架
- x86_64 系统调用机制详解
- syscall/sysret vs int 0x80 对比
- MSR 寄存器配置原理
- System V AMD64 ABI 调用约定
- 设计决策与架构
- 实现细节与关键技术
- 常见陷阱与注意事项
- 未来改进方向

**适合**:
- 理解设计思路
- 学习系统调用机制
- 查找开发经验

---

## 快速开始

### 查看代码结构

1. **系统调用框架接口** → 查看 [`kernel/syscall/syscall.h`](../../kernel/syscall/syscall.h)
2. **系统调用号定义** → 查看 [`kernel/syscall/syscall_numbers.h`](../../kernel/syscall/syscall_numbers.h)
3. **系统调用处理表** → 查看 [`kernel/syscall/syscall_table.c`](../../kernel/syscall/syscall_table.c)
4. **汇编入口** → 查看 [`kernel/syscall/syscall.asm`](../../kernel/syscall/syscall.asm)

### 使用示例

```c
#include "syscall/syscall.h"
#include "syscall/syscall_numbers.h"

// 内核初始化时调用
void kernel_init(void) {
    // 初始化系统调用框架
    syscall_init();

    // syscall_init 内部会：
    // 1. 检查 CPU 是否支持 syscall/sysret
    // 2. 启用 CR4 的 SCE 位
    // 3. 配置 MSR 寄存器
    // 4. 注册 int 0x80 处理器
}

// 用户程序发起系统调用（汇编示例）
// 方法1: 使用 syscall 指令
asm volatile(
    "mov %0, %%rax\n"     // 系统调用号
    "mov %1, %%rdi\n"     // 参数1
    "syscall\n"           // 发起系统调用
    : : "r"(SYS_WRITE), "r"(fd) : "rax", "rdi", "rcx", "r11"
);

// 方法2: 使用 int 0x80（传统方式）
asm volatile(
    "int $0x80"
    : : "a"(SYS_WRITE), "D"(fd)
);
```

### 系统调用处理函数示例

```c
#include "syscall/syscall.h"

// 实现一个自定义系统调用
static int64_t sys_my_syscall(syscall_frame_t* frame) {
    int arg1 = (int)frame->arg0;
    void* arg2 = (void*)frame->arg1;
    size_t arg3 = (size_t)frame->arg2;

    // 处理系统调用逻辑
    // ...

    return SYS_OK;  // 返回结果
}

// 在 syscall_register_all() 中注册
syscall_register_handler(SYS_MY_SYSCALL, sys_my_syscall, "my_syscall");
```

### 获取系统调用统计

```c
#include "syscall/syscall.h"

// 获取统计信息
void print_syscall_stats(void) {
    syscall_stats_t stats;
    syscall_get_stats(&stats);

    klog_info("Total syscalls: %llu\n", stats.total_calls);
    klog_info("Errors: %llu\n", stats.errors);
    klog_info("Not implemented: %llu\n", stats.not_impl_count);

    // 打印详细统计
    syscall_dump_stats();
}
```

### 系统调用帧结构

```c
typedef struct PACKED {
    uint64_t syscall_number;    // 系统调用号 (RAX)
    uint64_t arg0;              // 参数1 (RDI)
    uint64_t arg1;              // 参数2 (RSI)
    uint64_t arg2;              // 参数3 (RDX)
    uint64_t arg3;              // 参数4 (R10)
    uint64_t arg4;              // 参数5 (R8)
    uint64_t arg5;              // 参数6 (R9)
} syscall_frame_t;
```

---

## 与前一阶段对比

| 特性 | Stage 19 (代码重构) | Stage 20 (系统调用框架) |
|------|---------------------|------------------------|
| 用户态接口 | 无 | 完整系统调用框架 |
| syscall/sysret | 无 | x86_64 快速系统调用 |
| int 0x80 | 无 | 传统接口兼容 |
| MSR 配置 | 无 | IA32_LSTAR/STAR/FMASK |
| 系统调用分发 | 无 | 动态注册表 + 分发器 |
| 系统调用统计 | 无 | 完整统计系统 |
| 进程管理接口 | 无 | fork/exit/wait4/getpid |
| 文件 I/O 接口 | 无 | write/read/open/close |
| 新增文件 | - | 7 个 |

---

## 技术亮点

### 1. syscall/sysret 快速系统调用

利用 x86_64 的专用系统调用指令，避免传统中断的开销：

```c
// 用户态执行 syscall 指令
// CPU 自动：
// 1. 保存用户 RIP 到 RCX
// 2. 保存用户 RFLAGS 到 R11
// 3. 加载内核 RIP 从 IA32_LSTAR
// 4. 加载内核 CS/SS 从 IA32_STAR
// 5. 清除 RFLAGS 中的 IF 位（根据 IA32_FMASK）

// 返回时执行 sysretq
// CPU 自动：
// 1. 从 RCX 恢复用户 RIP
// 2. 从 R11 恢复用户 RFLAGS
// 3. 从 IA32_STAR 恢复用户 CS/SS
```

### 2. MSR 寄存器配置

```c
// IA32_LSTAR (0xC0000082) - syscall 目标地址
wrmsr(0xC0000082, (uint64_t)syscall_handler);

// IA32_STAR (0xC0000081) - 段选择器
// [63:48] = 用户 CS = GDT_USER_CODE | 3 = 0x1B
// [47:32] = 内核 CS = GDT_KERNEL_CODE = 0x08
uint64_t star = ((uint64_t)(GDT_USER_CODE | 3) << 48) |
                ((uint64_t)GDT_KERNEL_CODE << 32);
wrmsr(0xC0000081, star);

// IA32_FMASK (0xC0000084) - RFLAGS 掩码
// 清除 IF 位（禁用中断）
wrmsr(0xC0000084, 0x200);
```

### 3. System V AMD64 ABI 调用约定

```c
// 参数传递顺序
RAX = 系统调用号
RDI = arg0
RSI = arg1
RDX = arg2
R10 = arg3  // 注意：不是 RCX
R8  = arg4
R9  = arg5

// 返回值
RAX = 返回值
```

### 4. 双接口支持

同时支持现代 syscall/sysret 和传统 int 0x80：

```c
// 现代：syscall 指令
// 更快，专用指令
asm volatile("syscall");

// 传统：int 0x80
// 更慢，兼容性好
asm volatile("int $0x80");
```

### 5. 系统调用统计

```c
typedef struct {
    uint64_t total_calls;       // 总调用次数
    uint64_t syscall_calls[256];// 每个系统调用的次数
    uint64_t errors;            // 错误总数
    uint64_t not_impl_count;    // 未实现调用次数
} syscall_stats_t;
```

### 6. 系统调用流程

```
用户态程序
    │
    │ syscall 指令 / int 0x80
    ▼
汇编入口 (syscall_handler / int0x80_handler)
    │
    │ 保存寄存器，构建 syscall_frame_t
    ▼
C 分发器 (syscall_dispatch)
    │
    │ 查找系统调用表
    ▼
系统调用处理函数 (sys_xxx)
    │
    │ 返回结果
    ▼
汇编恢复寄存器，sysretq/iretq
    │
    ▼
返回用户态
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
        ┌────────────────────┴────────────────────┐
        │                                         │
┌───────▼────────┐                      ┌─────────▼──────┐
│   README.md    │                      │  开发笔记.md    │
│  (快速开始)     │                      │  (设计思路)     │
└────────────────┘                      └────────────────┘
        │                                         │
        └──────────────────┬──────────────────────┘
                           │
                   ┌──────▼──────┐
                   │   源代码     │
                   │ syscall/     │
                   └─────────────┘
```

---

## 版本信息

- **阶段**: Stage 20
- **提交**: `4eee7ca syscall framework`
- **日期**: 2026-02-18
- **作者**: CharlieChen

---

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../19_tidy_codes_and_refactorize/](../19_tidy_codes_and_refactorize/) - 上一阶段文档

### 源码文件
- [`kernel/syscall/syscall.h`](../../kernel/syscall/syscall.h) - 系统调用框架接口
- [`kernel/syscall/syscall.c`](../../kernel/syscall/syscall.c) - 系统调用框架实现
- [`kernel/syscall/syscall_numbers.h`](../../kernel/syscall/syscall_numbers.h) - 系统调用号定义
- [`kernel/syscall/syscall_table.c`](../../kernel/syscall/syscall_table.c) - 系统调用处理表
- [`kernel/syscall/syscall.asm`](../../kernel/syscall/syscall.asm) - 汇编入口/出口
- [`kernel/demo/mock_syscall/mock_syscall_demo.h`](../../kernel/demo/mock_syscall/mock_syscall_demo.h) - Mock 演示

### 外部参考
- [x86_64 System Calls](https://wiki.osdev.org/System_Calls)
- [syscall Instruction](https://www.felixcloutier.com/x86/syscall)
- [System V AMD64 ABI](https://gitlab.com/x86-psABIs/x86-64-ABI)
- [MSR Registers](https://wiki.osdev.org/Model-Specific_Registers)
- [OSDev.org](https://wiki.osdev.org)

---

**作者**: CharlieChen
**最后更新**: 2026-02-18
