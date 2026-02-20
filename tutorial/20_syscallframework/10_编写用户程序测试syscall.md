# 编写用户程序测试 syscall —— Stage 20 系统调用框架实战指南（十）

## 前言

在之前的九篇文章里，我们从零开始实现了完整的系统调用框架：MSR 配置、汇编入口、C 分发器、处理函数、统计系统。框架已经就绪，现在终于要迎来最激动人心的时刻——编写真正的用户态程序，通过 syscall 指令调用内核服务。

说实话，这是整个系列最有成就感的一步。当你看到用户程序发出的系统调用被内核正确处理，输出结果返回到用户态，那种感觉是无可比拟的。这标志着你的内核真正可以运行用户程序了，是一个重要的里程碑。

---

## 用户态程序与内核程序的区别

在开始之前，我们需要理解用户态程序和内核程序的区别：

**编译位置**：
- 内核程序：编译到内核镜像中，在 Ring 0 运行
- 用户程序：单独编译，加载到用户内存空间，在 Ring 3 运行

**调用方式**：
- 内核程序：直接调用函数
- 用户程序：通过 syscall 指令或 int 0x80

**权限限制**：
- 内核程序：可以访问所有资源
- 用户程序：只能访问自己的内存空间，需要通过系统调用请求内核服务

---

## 简单的用户程序框架

让我们先创建一个简单的用户程序框架。在项目中创建 `user/programs/simple/` 目录：

```bash
mkdir -p user/programs/simple
cd user/programs/simple
```

创建 `simple.c`：

```c
/**
 * @file simple.c
 * @brief Simple user program to test syscalls
 */

/* Syscall numbers (must match kernel) */
#define SYS_WRITE   13
#define SYS_GETPID  4
#define SYS_EXIT    0

/* Syscall result codes */
#define SYS_OK      0
#define SYS_ERR_NFILE -4

/**
 * @brief System call wrapper (syscall instruction)
 */
static long syscall0(long num) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static long syscall3(long num, long arg1, long arg2, long arg3) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * @brief Write to stdout
 */
static long my_write(int fd, const char* buf, int len) {
    return syscall3(SYS_WRITE, (long)fd, (long)buf, (long)len);
}

/**
 * @brief Get process ID
 */
static long my_getpid(void) {
    return syscall0(SYS_GETPID);
}

/**
 * @brief Print a string
 */
static void print(const char* str) {
    int len = 0;
    const char* p = str;
    while (*p++) len++;
    my_write(1, str, len);
}

/**
 * @brief Print a number
 */
static void print_num(long num) {
    char buf[32];
    int i = 30;
    buf[31] = '\0';

    if (num == 0) {
        print("0");
        return;
    }

    int negative = 0;
    if (num < 0) {
        negative = 1;
        num = -num;
    }

    while (num > 0 && i > 0) {
        buf[i--] = '0' + (num % 10);
        num /= 10;
    }

    if (negative) {
        buf[i--] = '-';
    }

    print(&buf[i + 1]);
}

/**
 * @brief Main function
 */
int main(void) {
    print("Hello from user space!\n");

    print("My PID is: ");
    long pid = my_getpid();
    print_num(pid);
    print("\n");

    print("Testing write syscall...\n");

    print("Exiting...\n");
    syscall0(SYS_EXIT);

    return 0;  /* Never reached */
}
```

这个程序实现了几个基本的系统调用封装，然后使用它们来打印消息和获取进程 ID。

---

## 用户态系统调用库

上面的程序中，我们直接在程序里实现了 syscall 封装。但在实际项目中，我们应该有一个独立的系统调用库。

创建 `user/syscall/x86_64/syscall.c`：

```c
/**
 * @file syscall.c
 * @brief User-space syscall wrapper for x86_64
 */

/* System call wrapper functions */
long _syscall0(long num) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long _syscall1(long num, long arg1) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long _syscall2(long num, long arg1, long arg2) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long _syscall3(long num, long arg1, long arg2, long arg3) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long _syscall6(long num, long arg1, long arg2, long arg3,
               long arg4, long arg5, long arg6) {
    long ret;
    long r10 = arg4;
    long r8 = arg5;
    long r9 = arg6;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}
```

创建对应的头文件 `user/syscall/include/syscall.h`：

```c
/**
 * @file syscall.h
 * @brief User-space syscall interface
 */

#pragma once

/* System call numbers */
#define SYS_EXIT    0
#define SYS_GETPID  4
#define SYS_GETPPID 5
#define SYS_WRITE   13
#define SYS_READ    12
#define SYS_OPEN    10
#define SYS_CLOSE   11

/* Syscall wrappers */
extern long _syscall0(long num);
extern long _syscall1(long num, long arg1);
extern long _syscall2(long num, long arg1, long arg2);
extern long _syscall3(long num, long arg1, long arg2, long arg3);
extern long _syscall6(long num, long arg1, long arg2, long arg3,
                      long arg4, long arg5, long arg6);

/* High-level wrappers */
static inline long sys_write(int fd, const void* buf, unsigned long count) {
    return _syscall3(SYS_WRITE, (long)fd, (long)buf, (long)count);
}

static inline long sys_getpid(void) {
    return _syscall0(SYS_GETPID);
}

static inline long sys_exit(int exit_code) {
    return _syscall1(SYS_EXIT, (long)exit_code);
}
```

---

## 用户程序的链接脚本

用户程序需要特殊的链接脚本，确保它被加载到正确的内存地址。创建 `user/programs/simple/linker.ld`：

```ld
ENTRY(_start)

SECTIONS {
    /* User programs start at 2MB */
    . = 0x200000;

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }

    /* Stack grows downward from 4MB */
    . = 0x400000;
    .stack : {
        _stack_bottom = .;
        . = . + 0x1000;  /* 4KB stack */
        _stack_top = .;
    }
}
```

同时需要修改用户程序的入口点。创建 `user/programs/simple/crt0.asm`：

```asm
section .text
bits 64
global _start

extern main

_start:
    ; Setup stack
    mov rsp, _stack_top

    ; Call main
    call main

    ; If main returns, exit
    mov rax, 0  ; SYS_EXIT
    mov rdi, 0  ; exit code
    syscall
```

---

## CMake 配置

现在需要配置 CMake 来编译用户程序。打开 `user/CMakeLists.txt`，添加以下内容：

```cmake
# Simple test program
add_executable(simple.elf
    programs/simple/crt0.asm
    programs/simple/simple.c
)

target_include_directories(simple.elf PRIVATE
    syscall/include
)

target_link_options(simple.elf PRIVATE
    -T ${CMAKE_CURRENT_SOURCE_DIR}/programs/simple/linker.ld
    -nostdlib
    -static
)

# Rename symbols to avoid conflicts
add_custom_command(TARGET simple.elf POST_BUILD
    COMMAND python3 ${CMAKE_SOURCE_DIR}/cmake/rename_symbols.py
        $<TARGET_FILE:simple.elf>
        ${CMAKE_CURRENT_BINARY_DIR}/simple_renamed.elf
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/simple_renamed.elf
        $<TARGET_FILE:simple.elf>
    COMMENT "Renaming symbols in simple.elf"
)
```

---

## 编译用户程序

从项目根目录运行：

```bash
cd build
make simple.elf
```

如果一切正常，应该会生成 `user/programs/simple/simple.elf` 文件。

---

## 加载并运行用户程序

现在我们需要在内核中加载并运行用户程序。这涉及到 ELF 加载、用户内存空间管理等内容，这些超出了本阶段的范围。

但我们可以用一个简单的方法来测试：把用户程序的内容嵌入到内核镜像中，然后跳转到用户代码执行。

在内核中添加一个测试函数：

```c
/* Embedded user program binary */
extern const char _binary_simple_elf_start[];
extern const char _binary_simple_elf_end[];

/**
 * @brief Load and run a simple user program
 */
void run_user_program(void) {
    klog_info("[USER] Loading user program...\n");

    /* TODO: Implement ELF loading */
    /* For now, just indicate the binary is available */
    size_t size = _binary_simple_elf_end - _binary_simple_elf_start;
    klog_info("[USER] User program size: %zu bytes\n", size);

    klog_warn("[USER] ELF loading not implemented yet\n");
    klog_warn("[USER] To test syscalls, use the kernel test function instead\n");
}
```

---

## 内核态测试替代方案

由于完整实现用户程序加载比较复杂，我们可以先在内核态测试系统调用。添加一个测试函数：

```c
/**
 * @brief Test syscall mechanism from kernel
 */
void test_syscall_mechanism(void) {
    klog_info("[TEST] === Testing Syscall Mechanism ===\n");

    syscall_frame_t frame;
    int64_t result;

    /* Test 1: SYS_WRITE */
    klog_info("[TEST] Test 1: write syscall\n");
    frame.syscall_number = SYS_WRITE;
    frame.arg0 = 1;  /* stdout */
    frame.arg1 = (uint64_t)"Hello from write syscall!\n";
    frame.arg2 = 26;
    result = syscall_dispatch(&frame, 0);
    klog_info("[TEST] write returned: %lld (expected: 26)\n", result);

    /* Test 2: SYS_GETPID */
    klog_info("[TEST] Test 2: getpid syscall\n");
    frame.syscall_number = SYS_GETPID;
    result = syscall_dispatch(&frame, 0);
    klog_info("[TEST] getpid returned: %lld (expected: 1)\n", result);

    /* Test 3: SYS_EXIT (should print warning) */
    klog_info("[TEST] Test 3: exit syscall\n");
    frame.syscall_number = SYS_EXIT;
    frame.arg0 = 42;  /* exit code */
    result = syscall_dispatch(&frame, 0);
    klog_info("[TEST] exit returned: %lld (expected: 0)\n", result);

    /* Test 4: Invalid syscall */
    klog_info("[TEST] Test 4: invalid syscall\n");
    frame.syscall_number = 999;  /* Invalid */
    result = syscall_dispatch(&frame, 0);
    klog_info("[TEST] invalid syscall returned: %lld (expected: -1)\n", result);

    /* Test 5: Unimplemented syscall */
    klog_info("[TEST] Test 5: unimplemented syscall (read)\n");
    frame.syscall_number = SYS_READ;
    result = syscall_dispatch(&frame, 0);
    klog_info("[TEST] unimplemented syscall returned: %lld (expected: -7)\n", result);

    /* Show statistics */
    syscall_dump_stats();

    klog_info("[TEST] === Syscall Tests Complete ===\n");
}
```

在 `kernel_init()` 中调用这个测试函数。运行后你应该看到类似的输出：

```
[TEST] === Testing Syscall Mechanism ===
[TEST] Test 1: write syscall
[USER OUT] Hello from write syscall!
[TEST] write returned: 26 (expected: 26)
[TEST] Test 2: getpid syscall
[TEST] getpid returned: 1 (expected: 1)
[TEST] Test 3: exit syscall
[SYSCALL] exit(42) called
[SYSCALL] Process termination not fully implemented yet
[TEST] exit returned: 0 (expected: 0)
[TEST] Test 4: invalid syscall
[SYSCALL] Invalid syscall number: 999
[TEST] invalid syscall returned: -1 (expected: -1)
[TEST] Test 5: unimplemented syscall (read)
[SYSCALL] read: not implemented
[SYSCALL] Unimplemented syscall: 12
[TEST] unimplemented syscall returned: -7 (expected: -7)
[SYSCALL] Statistics:
[SYSCALL]   Total calls: 5
[SYSCALL]   Errors:      2
[SYSCALL]   Not impl:    1
[SYSCALL]   Top syscalls:
[SYSCALL]     13: 1 calls (write)
[SYSCALL]     4: 1 calls (getpid)
[SYSCALL]     0: 1 calls (exit)
```

如果看到这样的输出，恭喜！你的系统调用框架已经完全工作了。

---

## 真正的用户程序测试

要真正运行用户程序，你需要实现以下功能：
1. ELF 加载器
2. 用户内存空间管理
3. 用户栈管理
4. 进程切换机制

这些是后续阶段的内容（Stage 21+）。在本阶段，我们已经完成了系统调用框架本身，这是最重要的基础。

---

## 总结

在这个十篇教程的系列中，我们从零开始实现了一个完整的 x86_64 系统调用框架：

**第一篇**：了解了为什么要系统调用，以及 syscall/sysret 指令的优势
**第二篇**：深入理解了 syscall 指令的工作原理和 MSR 寄存器配置
**第三篇**：搭建了代码框架，定义了数据结构和常量
**第四篇**：实现了 MSR 寄存器配置，让 syscall 指令有了目标地址
**第五篇**：实现了汇编入口代码，处理寄存器保存、栈对齐等问题
**第六篇**：实现了 int 0x80 向后兼容支持
**第七篇**：实现了 C 语言分发器和系统调用表
**第八篇**：实现了第一个真正的系统调用处理函数
**第九篇**：添加了统计系统和调试支持
**第十篇**：编写了用户程序和测试代码

现在你的内核已经具备了完整的系统调用能力，可以接受用户程序的服务请求了。这是操作系统开发的一个重要里程碑，祝贺你！

---

## 接下来

系统调用框架已经完成，接下来你可以：

1. **实现 ELF 加载器**，让内核能够加载和运行用户程序
2. **实现进程管理**，支持多进程并发执行
3. **完善系统调用**，实现更多 POSIX 标准系统调用
4. **添加文件系统**，让 read/write/open 等系统调用真正工作

这些内容将在后续的阶段中逐步实现。继续加油！

---

<div align="center">

## 文档导航

[← 统计系统与调试支持](09_统计系统与调试支持.md)  | [返回目录](README.md)

</div>
