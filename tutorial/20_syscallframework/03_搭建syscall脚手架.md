# 搭建 syscall 脚手架 —— Stage 20 系统调用框架实战指南（三）

## 前言

在之前的两篇文章里，我们讲了系统调用的概念和 syscall 指令的工作原理。现在终于要动手写代码了，说实话这可能是最让人兴奋的部分。但别着急，我们需要先搭建好"脚手架"——创建目录结构、配置构建系统、定义数据结构和常量。

这些基础工作虽然看起来不那么"硬核"，但它们决定了后续代码的组织方式和可维护性。如果你之前跟着这个系列做过其他阶段，应该已经熟悉我们的代码组织方式了。我们会在 `kernel/syscall/` 目录下创建所有系统调用相关的代码。

---

## 环境说明

开始之前，确保你的项目已经完成了 Stage 19 的代码重构。你应该有一个干净的内核代码库，CMake 构建系统正常工作。

```
工作目录: /path/to/CCOperatingSystemX64
构建工具: CMake 3.25+
编译器:   x86_64-elf-gcc
汇编器:   NASM 2.15+
```

---

## 第一步：创建目录结构

首先在 `kernel/` 目录下创建 `syscall/` 子目录：

```bash
cd kernel
mkdir syscall
cd syscall
```

这个目录将包含所有系统调用相关的代码。让我们先规划一下每个文件的用途：

```
kernel/syscall/
├── CMakeLists.txt         # 构建配置
├── syscall.h              # 公共接口定义
├── syscall.c              # 框架核心实现
├── syscall.asm            # 汇编入口/出口
├── syscall_numbers.h      # 系统调用号定义
└── syscall_table.c        # 系统调用处理表
```

现在创建这些空文件：

```bash
touch CMakeLists.txt syscall.h syscall.c syscall.asm syscall_numbers.h syscall_table.c
```

---

## 第二步：编写 CMakeLists.txt

CMakeLists.txt 定义了如何编译这些文件。我们先写一个简单的版本，后续根据需要添加。

打开 `kernel/syscall/CMakeLists.txt`，写入以下内容：

```cmake
# ============================================================================
# CCOS System Call Framework
# ============================================================================

# 汇编源文件
set(SYSCALL_ASM_SRC
    ${CMAKE_CURRENT_LIST_DIR}/syscall.asm
)

# C 源文件
set(SYSCALL_C_SRC
    ${CMAKE_CURRENT_LIST_DIR}/syscall.c
    ${CMAKE_CURRENT_LIST_DIR}/syscall_table.c
)

# 头文件路径
set(SYSCALL_INCLUDE_DIR
    ${CMAKE_CURRENT_LIST_DIR}
)

# 创建库
add_library(syscall_lib STATIC ${SYSCALL_ASM_SRC} ${SYSCALL_C_SRC})

# 设置包含目录
target_include_directories(syscall_lib PUBLIC
    ${SYSCALL_INCLUDE_DIR}
    ${KERNEL_INCLUDE_DIRS}
)

# 设置汇编编译选项
target_compile_options(syscall_lib PUBLIC
    -ffreestanding
    -mcmodel=large
    -mno-red-zone
    -mno-mmx
    -mno-sse
    -mno-sse2
)

# 链接到内核主目标
target_link_libraries(kernel PUBLIC syscall_lib)
```

这个 CMakeLists.txt 做了几件事：首先定义了汇编和 C 源文件，然后创建了一个静态库 `syscall_lib`，最后把这个库链接到内核主目标。

---

## 第三步：定义系统调用号

现在让我们定义系统调用号。打开 `syscall_numbers.h`，写入以下内容：

```c
/**
 * @file syscall_numbers.h
 * @brief System call number definitions
 */

#pragma once

#include "defines/types.h"

/* ============================================================================
 * System Call Numbers
 * ============================================================================ */

/**
 * @brief System call numbers
 *
 * These are the values passed in RAX when invoking a syscall.
 * Range 0-255 is reserved for kernel-defined syscalls.
 */
typedef enum syscall_number {
    /* Process management */
    SYS_EXIT       = 0,    /* Exit current process */
    SYS_EXECVE     = 1,    /* Execute a program */
    SYS_FORK       = 2,    /* Create a new process */
    SYS_WAIT4      = 3,    /* Wait for process to change state */
    SYS_GETPID     = 4,    /* Get process ID */
    SYS_GETPPID    = 5,    /* Get parent process ID */

    /* File I/O */
    SYS_OPEN       = 10,   /* Open a file */
    SYS_CLOSE      = 11,   /* Close a file descriptor */
    SYS_READ       = 12,   /* Read from file descriptor */
    SYS_WRITE      = 13,   /* Write to file descriptor */
    SYS_LSEEK      = 14,   /* Reposition file offset */
    SYS_IOCTL      = 15,   /* Device-specific operations */

    /* Memory management */
    SYS_BRK        = 20,   /* Change data segment size */
    SYS_MMAP       = 21,   /* Map files or devices into memory */
    SYS_MUNMAP     = 22,   /* Unmap files or devices from memory */

    /* System information */
    SYS_UNAME      = 30,   /* Get system information */
    SYS_GETTIME    = 31,   /* Get system time */

    /* Debug/Testing (for early development) */
    SYS_DEBUG_LOG  = 100,  /* Debug logging syscall */
    SYS_TEST       = 101,  /* Test syscall */

    /* Maximum syscall number (for table size) */
    SYS_MAX        = 256,
} syscall_number_t;

/**
 * @brief System call return codes
 */
typedef enum syscall_result {
    SYS_OK           = 0,   /* Success */
    SYS_ERR_INVAL    = -1,  /* Invalid argument */
    SYS_ERR_PERM     = -2,  /* Permission denied */
    SYS_ERR_NOMEM    = -3,  /* Out of memory */
    SYS_ERR_NFILE    = -4,  /* File table overflow */
    SYS_ERR_NOENT    = -5,  /* No such file or directory */
    SYS_ERR_IO       = -6,  /* I/O error */
    SYS_ERR_NOTIMPL  = -7,  /* Not implemented */
} syscall_result_t;
```

这里我们定义了三组系统调用号：进程管理、文件 I/O、内存管理、系统信息，还有一些用于调试和测试的系统调用。注意我们把调试系统调用的号设为 100 和 101，避免与正式系统调用冲突。

错误码的定义遵循 POSIX 的约定：负数表示错误，零或正数表示成功。

---

## 第四步：定义系统调用帧结构

系统调用帧是用户态和内核态之间传递数据的结构。打开 `syscall.h`，写入以下内容：

```c
/**
 * @file syscall.h
 * @brief System call framework for x86_64
 */

#pragma once

#include "defines/types.h"
#include "syscall_numbers.h"

/* ============================================================================
 * System Call Frame
 * ============================================================================ */

/**
 * @brief System call frame structure
 *
 * This structure represents the state passed from user mode to kernel mode
 * during a system call. It follows the System V AMD64 ABI convention.
 */
typedef struct PACKED {
    uint64_t syscall_number;    /* System call number (from RAX) */
    uint64_t arg0;              /* First argument (RDI) */
    uint64_t arg1;              /* Second argument (RSI) */
    uint64_t arg2;              /* Third argument (RDX) */
    uint64_t arg3;              /* Fourth argument (R10) */
    uint64_t arg4;              /* Fifth argument (R8) */
    uint64_t arg5;              /* Sixth argument (R9) - stored separately */
} syscall_frame_t;

/**
 * @brief Extended frame for legacy int 0x80 (includes arg5 inline)
 */
typedef struct PACKED {
    uint64_t syscall_number;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    uint64_t arg5;              /* Arg5 inline for int 0x80 */
} syscall_frame_int80_t;
```

注意 `syscall_frame_t` 的字段顺序与 System V AMD64 ABI 完全一致：RAX（系统调用号）、RDI（arg0）、RSI（arg1）、RDX（arg2）、R10（arg3）、R8（arg4）、R9（arg5）。这个顺序不能错，否则汇编代码构造帧结构时会出现问题。

`syscall_frame_int80_t` 是为 int 0x80 设计的，因为 int 0x80 的调用约定不同，所有参数都可以在栈上传递。

---

## 第五步：定义处理函数类型

继续在 `syscall.h` 中添加处理函数类型和统计结构：

```c
/* ============================================================================
 * System Call Handler Function Type
 * ============================================================================ */

/**
 * @brief Type for system call handler functions
 *
 * @param frame Pointer to the syscall frame
 * @return int64_t Return value (negative for errors, per syscall_result_t)
 */
typedef int64_t (*syscall_handler_fn)(syscall_frame_t* frame);

/* ============================================================================
 * System Call Statistics
 * ============================================================================ */

/**
 * @brief System call statistics
 */
typedef struct {
    uint64_t total_calls;       /* Total syscall invocations */
    uint64_t syscall_calls[256];/* Per-syscall call count */
    uint64_t errors;            /* Total errors */
    uint64_t not_impl_count;    /* Unimplemented syscall count */
} syscall_stats_t;
```

`syscall_handler_fn` 是所有系统调用处理函数的函数类型。每个处理函数接收一个 `syscall_frame_t*` 参数，返回一个 `int64_t` 结果。

---

## 第六步：定义 MSR 访问函数

继续添加 MSR 相关的定义和函数：

```c
/* ============================================================================
 * MSR Configuration Values
 * ============================================================================ */

/* FMASK: RFLAGS bits to clear on syscall entry */
#define SYSCALL_FMASK_DEFAULT 0x200  /* Clear IF (interrupt flag) */

/* ============================================================================
 * MSR Access Functions
 * ============================================================================ */

/**
 * @brief Read an MSR register
 *
 * @param msr MSR address
 * @return uint64_t MSR value
 */
static inline __attribute__((always_inline)) uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

/**
 * @brief Write an MSR register
 *
 * @param msr MSR address
 * @param value Value to write
 */
static inline __attribute__((always_inline)) void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high) : "memory");
}
```

这两个内联函数封装了 `rdmsr` 和 `wrmsr` 指令。`always_inline` 属性确保这些函数总是被内联，避免函数调用的开销。MSR 操作本身就很敏感，我们希望这些调用尽可能高效。

---

## 第七步：定义公共 API

最后在 `syscall.h` 中添加公共 API 声明：

```c
/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize the system call framework
 *
 * This function:
 * 1. Enables the SCE (System Call Extension) bit in CR4
 * 2. Configures MSR registers for syscall/sysret
 * 3. Sets up the syscall handler table
 * 4. Registers the int 0x80 fallback
 *
 * Must be called after GDT/TSS initialization.
 */
void syscall_init(void);

/**
 * @brief Register a system call handler
 *
 * @param number System call number
 * @param handler Handler function
 * @param name Human-readable name (for debugging)
 * @return 0 on success, negative on error
 */
int syscall_register_handler(uint64_t number, syscall_handler_fn handler, const char* name);

/**
 * @brief System call dispatcher (called from assembly stub)
 *
 * @param frame Pointer to syscall frame
 * @param arg5 Sixth argument (passed in R9, separate from frame)
 * @return int64_t Return value
 */
int64_t syscall_dispatch(syscall_frame_t* frame, uint64_t arg5);

/**
 * @brief System call dispatcher for int 0x80
 *
 * @param frame Pointer to syscall frame (with inline arg5)
 * @return int64_t Return value
 */
int64_t syscall_dispatch_int80(syscall_frame_int80_t* frame);

/**
 * @brief Get system call statistics
 *
 * @param stats Pointer to stats structure to fill
 */
void syscall_get_stats(syscall_stats_t* stats);

/**
 * @brief Dump system call statistics for debugging
 */
void syscall_dump_stats(void);

/**
 * @brief Check if syscall/sysret is available
 *
 * @return true if CPU supports syscall/sysret
 */
bool syscall_is_available(void);
```

这些是系统调用框架的公共接口。其他内核模块只需要调用 `syscall_init()` 来初始化框架，然后使用 `syscall_register_handler()` 注册系统调用处理函数。

---

## 第八步：初始化 syscall.c

现在让我们创建 `syscall.c` 的初始版本，包含一些基本的框架代码：

```c
/**
 * @file syscall.c
 * @brief System call framework implementation
 */

#include "syscall.h"
#include "base/memory.h"
#include "interrupt/gdt.h"
#include "interrupt/idt.h"
#include "interrupt/idt_constants.h"
#include "klogs/kprintf.h"

/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief System call handler entry
 */
typedef struct {
    syscall_handler_fn handler; /* Handler function */
    const char* name;           /* Name for debugging */
    bool registered;            /* Whether handler is registered */
} syscall_entry_t;

/**
 * @brief System call table
 */
static syscall_entry_t s_syscall_table[SYS_MAX] = {0};

/**
 * @brief System call statistics
 */
static syscall_stats_t s_stats = {0};

/**
 * @brief Initialization flag
 */
static bool s_initialized = false;

/* ============================================================================
 * Default/Stub Handlers
 * ============================================================================ */

/**
 * @brief Default handler for unimplemented syscalls
 */
static int64_t syscall_not_impl(syscall_frame_t* frame) {
    (void)frame;
    s_stats.not_impl_count++;
    s_stats.errors++;
    klog_warn("[SYSCALL] Unimplemented syscall: %lu\n", frame->syscall_number);
    return SYS_ERR_NOTIMPL;
}

/**
 * @brief Debug log syscall (for testing)
 */
static int64_t syscall_debug_log(syscall_frame_t* frame) {
    const char* msg = (const char*)frame->arg0;
    uint64_t len = frame->arg1;
    klog_info("[USER LOG] %.*s\n", (int)len, msg);
    return SYS_OK;
}

/**
 * @brief Test syscall (returns input value)
 */
static int64_t syscall_test(syscall_frame_t* frame) {
    return (int64_t)frame->arg0; /* Echo back first argument */
}

/* ============================================================================
 * CPUID Feature Detection
 * ============================================================================ */

/**
 * @brief Check if CPU supports syscall/sysret
 */
bool syscall_is_available(void) {
    uint32_t eax, ebx, ecx, edx;
    /* CPUID leaf 0x80000001, EDX bit 11 = syscall/sysret support */
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(0x80000001));
    return (edx & (1 << 11)) != 0;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int syscall_register_handler(uint64_t number, syscall_handler_fn handler, const char* name) {
    if (number >= SYS_MAX) {
        klog_error("[SYSCALL] Invalid syscall number: %lu\n", number);
        return -1;
    }

    if (handler == NULL) {
        klog_error("[SYSCALL] NULL handler for syscall %lu\n", number);
        return -2;
    }

    syscall_entry_t* entry = &s_syscall_table[number];
    entry->handler = handler;
    entry->name = name;
    entry->registered = true;

    klog_trace("[SYSCALL] Registered syscall %lu: %s\n", number, name ? name : "unnamed");
    return 0;
}

void syscall_get_stats(syscall_stats_t* stats) {
    if (stats) {
        *stats = s_stats;
    }
}
```

这个文件包含了系统调用表的内部状态、默认处理函数、CPU 特性检测，以及部分公共 API 的实现。`syscall_init()` 和 `syscall_dispatch()` 的实现我们会在后面的文章中完成。

---

## 第九步：创建汇编文件骨架

创建 `syscall.asm` 的初始版本：

```asm
; ============================================================================
; syscall.asm - System call entry/exit stubs for x86_64
; ============================================================================
; This file contains low-level assembly code for syscall/sysret instruction
; support and traditional int 0x80 fallback.
; ============================================================================

section .text
bits 64

; ============================================================================
; syscall Instruction Entry Point
; ============================================================================

; Entry point for syscall instruction
; Stack layout on entry (pushed by hardware):
;   [RSP]    = RCX (saved user RIP)
;   [RSP+8]  = R11 (saved user RFLAGS)
;   (Note: syscall does NOT push SS/RSP like interrupts do)
global syscall_handler
extern syscall_dispatch
syscall_handler:
    ; TODO: Implement syscall handler
    ; For now, just return
    sysretq

; ============================================================================
; int 0x80 Entry Point (Legacy Interface)
; ============================================================================

; Entry point for int 0x80 instruction
; Stack layout on entry (pushed by CPU):
;   [RSP]    = SS (ignored in long mode)
;   [RSP+8]  = old RSP
;   [RSP+16] = RFLAGS
;   [RSP+24] = CS
;   [RSP+32] = RIP
global int0x80_handler
extern syscall_dispatch_int80
int0x80_handler:
    ; TODO: Implement int 0x80 handler
    ; For now, just return
    iretq
```

这个汇编文件目前只有空的实现，我们会在后续文章中填充完整代码。

---

## 第十步：编译验证

现在让我们验证一下代码能否编译。首先需要更新主 CMakeLists.txt 来包含 syscall 子目录。

打开 `kernel/CMakeLists.txt`，在相应的位置添加：

```cmake
# Add syscall subdirectory
add_subdirectory(syscall)
```

现在从项目根目录运行构建：

```bash
cd build
cmake ..
make
```

如果一切正常，你应该看到类似以下的输出：

```
[ 10%] Building ASM object kernel/syscall/CMakeFiles/syscall_lib.dir/syscall.asm.obj
[ 20%] Building C object kernel/syscall/CMakeFiles/syscall_lib.dir/syscall.c.obj
[ 30%] Building C object kernel/syscall/CMakeFiles/syscall_lib.dir/syscall_table.c.obj
[ 30%] Linking C static library kernel/syscall/libsyscall_lib.a
[ 40%] Linking C executable kernel.elf
```

如果遇到编译错误，检查以下几点：
1. `defines/types.h` 是否存在并定义了基本类型
2. `base/memory.h` 是否提供了 `memset` 函数
3. `interrupt/gdt.h` 和 `interrupt/idt.h` 是否存在并定义了必要的常量

---

## 常见问题排查

如果你在编译时遇到问题，这里有一些常见的坑：

**问题1：找不到头文件**

错误信息类似 `fatal error: defines/types.h: No such file or directory`

解决方法：检查 `KERNEL_INCLUDE_DIRS` 是否正确设置。如果你用的是这个项目的标准结构，应该包含 `kernel` 目录。

**问题2：未定义的引用**

错误信息类似 `undefined reference to 'memset'`

解决方法：确保链接了 `base_lib` 或提供内存操作函数的库。

**问题3：汇编编译错误**

错误信息类似 `error: operation size not specified`

解决方法：确保在 syscall.asm 中正确使用了 `qword`、`dword` 等操作数大小指示符。

---

## 接下来

到这里我们已经搭建好了系统调用框架的脚手架：目录结构、构建配置、数据结构、基本函数都准备好了。在下一篇文章中，我们会实现 MSR 寄存器的配置，让 syscall 指令真正工作起来。

这部分代码需要直接操作硬件寄存器，配置稍微复杂一点。但别担心，我们会一步步来，确保每个细节都讲清楚。

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← syscall指令的魔法](02_syscall指令的魔法.md)  | [MSR寄存器配置实战 →](04_MSR寄存器配置实战.md)

</div>
