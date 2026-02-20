# CCOS 用户态进程支持文档中心

本目录包含 CCOS Stage 23 - 用户态进程支持开发的完整文档体系。

---

## 阶段概述

**Stage 23: 用户态进程支持 (User Process Support)**

本阶段在 Stage 22 调度器分类基础上，实现了完整的用户态（Ring 3）进程支持，标志着操作系统从纯内核进程向真正的用户态应用程序执行环境的跨越。实现了用户态进程创建、用户内存管理、用户库构建和系统调用扩展等核心功能，为运行独立用户程序奠定了基础。

### 核心成果

- **用户态支持模块** ([`kernel/user/user.h`](../../kernel/user/user.h))
  - 用户态进程创建和销毁
  - 用户内存验证和安全访问
  - 用户栈管理 (1MB)
  - Ring 0 到 Ring 3 切换支持

- **用户态入口** ([`kernel/user/user_enter.asm`](../../kernel/user/user_enter.asm))
  - iretq 实现特权级切换
  - 用户上下文设置
  - 段选择器配置

- **用户内存管理**
  - brk() 系统调用 (程序断点管理)
  - mmap/munmap 系统调用 (内存映射)
  - 用户空间区域管理
  - 安全的内存访问验证

- **用户 C 库** ([`user/`](../../user/))
  - stdio.h - 标准 I/O (printf, puts, putchar)
  - stdlib.h - 标准库 (malloc, free, exit)
  - unistd.h - 系统调用包装 (write, read, uname)
  - x86_64 系统调用实现

- **用户程序构建系统**
  - 独立编译为二进制文件
  - 嵌入内核镜像
  - 符号重命名和页对齐
  - CMake 自定义命令链

- **系统调用扩展** ([`kernel/syscall/syscall_table.c`](../../kernel/syscall/syscall_table.c))
  - brk() - 修改程序断点
  - mmap() - 内存映射
  - munmap() - 解除内存映射
  - uname() - 获取系统信息
  - read() - 读取文件
  - close() - 关闭文件
  - lseek() - 文件定位
  - ioctl() - 设备控制

- **用户态演示程序** ([`kernel/demo/user/user_demo.h`](../../kernel/demo/user/user_demo.h))
  - 用户进程创建测试
  - 用户内存管理测试
  - uname 系统调用测试
  - 用户库功能验证

---

## 目录结构

```
kernel/
├── user/
│   ├── user.h                  # 用户态支持接口
│   ├── user.c                  # 用户态支持实现
│   ├── user_enter.asm          # Ring 3 切换汇编
│   └── CMakeLists.txt          # 构建配置
├── demo/
│   ├── user/
│   │   ├── user_demo.h         # 用户态演示接口
│   │   └── user_demo.c         # 用户态演示实现
│   └── CMakeLists.txt          # 演示构建配置
├── syscall/
│   └── syscall_table.c         # 新增用户态系统调用
├── process/
│   └── process.h               # PCB 扩展 (用户态字段)
└── mm/
    └── vmm/
        ├── vmm.c               # 用户空间管理扩展
        └── vmm.h               # VMM 接口

user/
├── include/
│   ├── stddef.h                # 标准定义
│   ├── stdint.h                # 标准整数类型
│   ├── stdio.h                 # 标准 I/O
│   ├── stdlib.h                # 标准库
│   └── unistd.h                # 系统调用接口
├── src/
│   ├── stdio.c                 # I/O 实现
│   ├── stdlib.c                # 标准库实现
│   └── unistd.c                # 系统调用包装
├── syscall/
│   ├── include/syscall.h       # 系统调用宏
│   └── x86_64/syscall.c        # x86_64 系统调用
├── programs/
│   └── demo/
│       ├── uname_test.c        # uname 测试程序
│       └── uname_test.h        # 程序声明
├── cmake/
│   └── rename_symbols.py.in    # 符号重命名脚本
└── CMakeLists.txt              # 用户库构建配置
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要用户态进程支持
- 用户态设计基础（特权级、内存隔离）
- 设计决策（用户栈布局、内存验证、系统调用）
- 架构设计（模块协作、用户上下文、内存管理）
- 实现细节（初始化、进程创建、内存映射、库构建）
- 常见陷阱（指针验证、内存泄漏、符号冲突）
- 未来改进（ELF 加载、动态链接、信号处理）

**适合**:
- 理解设计思路
- 学习用户态进程原理
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- 用户态常量定义
- User API 完整参考
- 用户库 API 参考
- 数据结构定义（user_context_t、user_region_t、utsname）
- 常量定义
- 算法说明（内存验证、Ring 3 切换、内存映射）
- 系统调用映射

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 问题诊断

**内容**:
- 初始化问题
- 用户进程创建问题
- 系统调用问题（权限、参数验证）
- 内存管理问题（brk、mmap 失败）
- 用户库链接问题
- 调试技巧（GDB 命令、日志分析）

每个问题按"症状-原因-解决方案"格式组织。

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

## 快速开始

### 查看代码结构

1. **用户态支持接口** → 查看 [`kernel/user/user.h`](../../kernel/user/user.h)
2. **用户态支持实现** → 查看 [`kernel/user/user.c`](../../kernel/user/user.c)
3. **Ring 3 切换** → 查看 [`kernel/user/user_enter.asm`](../../kernel/user/user_enter.asm)
4. **用户库** → 查看 [`user/include/unistd.h`](../../user/include/unistd.h)
5. **用户演示程序** → 查看 [`kernel/demo/user/user_demo.h`](../../kernel/demo/user/user_demo.h)

### 使用示例

```c
#include "user/user.h"

// 内核初始化时调用
void kernel_init(void) {
    // 确保在 VMM 和进程初始化之后调用
    user_init();
}

// 创建用户态进程
int create_user_process_example(void) {
    pcb_t* user_pcb = allocate_pcb();

    // 创建用户地址空间
    vmm_create_user_space(&user_pcb->mm.pml4_phys);

    // 创建用户态进程结构
    user_create_process(entry_point, user_pcb);

    // 设置用户上下文
    user_context_t ctx = {
        .entry = 0x400000,        // 用户代码入口
        .stack_top = user_pcb->user_stack,
        .cs = USER_CS,            // 0x18 | 3
        .ss = USER_SS,            // 0x20 | 3
        .rflags = 0x202           // IF = 1
    };

    // 切换到用户态 (不返回)
    user_switch_to_usermode(&ctx);
}

// 安全的用户内存访问
int syscall_example(char* user_buf, size_t count) {
    // 验证用户指针
    if (!user_validate_pointer(user_buf, count, true)) {
        return -1;
    }

    // 从用户空间复制数据
    char kernel_buf[256];
    int64_t copied = user_copy_from_user(kernel_buf, user_buf, count);
    if (copied < 0) {
        return -1;
    }

    // 处理数据...
    return 0;
}
```

### 用户程序编写

```c
// user/programs/demo/my_program.c

// 系统调用号
#define SYS_WRITE   13
#define SYS_EXIT    0

// 内联系统调用包装
static inline long syscall3(long num, long arg1, long arg2, long arg3) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
                      : "rcx", "r11", "memory");
    return ret;
}

static long write(int fd, const void* buf, long count) {
    return syscall3(SYS_WRITE, fd, (long)buf, count);
}

static void exit(int status) {
    __asm__ volatile("syscall" : : "a"(SYS_EXIT), "D"(status));
    __builtin_unreachable();
}

void _start(void) {
    const char* msg = "Hello from user mode!\n";
    write(1, msg, 25);
    exit(0);
}
```

### 特权级切换流程

```
┌─────────────────────────────────────────────────────────┐
│                     内核态 (Ring 0)                     │
│                  user_switch_to_usermode()              │
└────────────────────────┬────────────────────────────────┘
                         │ 设置 iretq 栈帧
                         │ SS:RSP, RFLAGS, CS:RIP
                         │ 切换段寄存器
                         ▼
                    ┌─────────┐
                    │  iretq  │
                    └────┬────┘
                         │ CPU 自动切换特权级
                         │ 加载用户栈
                         ▼
┌─────────────────────────────────────────────────────────┐
│                     用户态 (Ring 3)                     │
│                   用户程序执行 (用户栈)                  │
└────────────────────────┬────────────────────────────────┘
                         │ syscall 指令
                         ▼
                    ┌─────────┐
                    │ syscall │
                    └────┬────┘
                         │ CPU 自动切换特权级
                         │ 保存用户 RIP/RSP
                         ▼
┌─────────────────────────────────────────────────────────┐
│                     内核态 (Ring 0)                     │
│                  系统调用处理程序                        │
└─────────────────────────────────────────────────────────┘
```

---

## 与前一阶段对比

| 特性 | Stage 22 (调度器分类) | Stage 23 (用户态进程) |
|------|----------------------|----------------------|
| 执行模式 | 内核态进程 (Ring 0) | 用户态进程 (Ring 3) |
| 用户栈 | 无 | 1MB 用户栈 |
| 内存隔离 | 进程间隔离 | 内核/用户空间隔离 |
| 系统调用 | 基础进程管理 | brk/mmap/uname 等 |
| 用户库 | 无 | 完整 C 库 |
| Ring 3 切换 | 无 | iretq 实现 |
| 内存验证 | 无 | 用户指针验证 |
| 用户程序 | 无 | 独立编译嵌入 |
| physical_addr_t | 32-bit | 64-bit |
| 新增文件 | - | 20+ 个 |

---

## 技术亮点

### 1. Ring 0 到 Ring 3 切换

```assembly
; user_switch_to_usermode(user_context_t* ctx)
global user_switch_to_usermode
user_switch_to_usermode:
    ; 加载用户上下文
    mov rax, [rdi + user_context.entry]      ; 用户 RIP
    mov rbx, [rdi + user_context.stack_top]  ; 用户 RSP
    mov rcx, [rdi + user_context.cs]         ; 用户 CS (0x1B)
    mov rdx, [rdi + user_context.ss]         ; 用户 SS (0x23)
    mov rsi, [rdi + user_context.rflags]     ; 用户 RFLAGS

    ; 设置 iretq 栈帧
    push rdx                 ; SS
    push rbx                 ; RSP
    push rsi                 ; RFLAGS
    push rcx                 ; CS
    push rax                 ; RIP

    ; 设置数据段寄存器
    mov ax, 0x23             ; USER_SS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    iretq                    ; 切换到 Ring 3
```

### 2. 用户内存验证

```c
bool user_validate_pointer(const void* ptr, size_t size, bool write) {
    virtual_addr_t addr = (virtual_addr_t)ptr;

    // 检查是否在用户空间范围
    if (!vmm_is_user_addr(addr)) {
        return false;
    }

    // 检查溢出
    if (addr + size < addr) {
        return false;
    }

    // 检查超出用户空间
    if (addr + size > USER_END) {
        return false;
    }

    // 检查 NULL 页保护
    if (addr < USER_BASE) {
        return false;
    }

    return true;
}
```

### 3. 用户程序嵌入流程

```cmake
# 1. 编译为可执行文件
add_executable(uname_test programs/demo/uname_test.c)

# 2. 转换为纯二进制
add_custom_command(
    OUTPUT uname_test.bin
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:uname_test> uname_test.bin
)

# 3. 转换为对象文件 (页对齐)
add_custom_command(
    OUTPUT uname_test.o
    COMMAND ${CMAKE_OBJCOPY} -I binary -O elf64-x86-64 -B i386
        --rename-section .data=.user_uname_test,alloc,load,readonly,data,contents
        --set-section-alignment .user_uname_test=4096
        uname_test.bin uname_test.o
)

# 4. 重命名符号
add_custom_command(
    OUTPUT uname_test_symbols.o
    COMMAND python3 rename_symbols.py
        ${CMAKE_OBJCOPY} uname_test.o uname_test_symbols.o
)
```

### 4. 用户内存映射

```c
virtual_addr_t user_mmap(pcb_t* pcb, virtual_addr_t addr, size_t length,
                        int prot, int flags, int fd, size_t offset) {
    // 转换保护标志
    uint64_t vmap_flags = VMAP_FLAG_USER;
    if (prot & PROT_WRITE) vmap_flags |= VMAP_FLAG_WRITE;
    if (!(prot & PROT_EXEC)) vmap_flags |= VMAP_FLAG_NO_EXEC;

    // 分配并映射页
    for (size_t i = 0; i < length / PAGE_SIZE; i++) {
        physical_addr_t paddr;
        pframe_alloc(&paddr);

        vmm_map_to_user(pcb->mm.pml4_phys,
                       addr + (i * PAGE_SIZE),
                       paddr, 1, vmap_flags);

        memset((void*)phys_to_virt_offset(paddr), 0, PAGE_SIZE);
    }

    return addr;
}
```

### 5. 系统调用包装 (用户库)

```c
// user/syscall/x86_64/syscall.c
int64_t _syscall3(long num, long arg1, long arg2, long arg3) {
    int64_t ret;
    __asm__ volatile(
        "syscall;"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

// user/src/unistd.c
ssize_t write(int fd, const void* buf, size_t count) {
    return (ssize_t)_syscall3(SYS_WRITE, fd, (long)buf, (long)count);
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

- **阶段**: Stage 23
- **分支**: `stage/23_usr_proc`
- **日期**: 2026-02-20
- **作者**: Charliechen114514

---

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../22_sched_class/README.md](../22_sched_class/) - 上一阶段文档

### 源码文件
- [`kernel/user/user.h`](../../kernel/user/user.h) - 用户态支持接口
- [`kernel/user/user.c`](../../kernel/user/user.c) - 用户态支持实现
- [`kernel/user/user_enter.asm`](../../kernel/user/user_enter.asm) - Ring 3 切换
- [`user/include/unistd.h`](../../user/include/unistd.h) - 用户库接口
- [`user/programs/demo/uname_test.c`](../../user/programs/demo/uname_test.c) - 用户程序示例
- [`kernel/demo/user/user_demo.h`](../../kernel/demo/user/user_demo.h) - 演示程序

### 外部参考
- [x86-64 System Call](https://wiki.osdev.org/System_Calls)
- [User Mode](https://wiki.osdev.org/User_Mode)
- [IRETQ](https://wiki.osdev.org/Interrupt_Return_IRETQ)
- [OSDev.org](https://wiki.osdev.org)

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-20
