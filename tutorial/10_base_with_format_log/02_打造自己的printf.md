# 打造自己的 printf —— Stage 10 格式化输出实战指南

## 前言

说实话，`printf` 可能是 C 语言中最神奇的函数之一。你只需要写一行代码：

```c
printf("Hello, %s! The answer is %d.\n", "world", 42);
```

它就能自动处理不同类型的参数，格式化成字符串，然后输出到终端。但你有没有想过：它是怎么知道后面有几个参数的？怎么处理 `%d`、`%s` 这些占位符的？为什么内核里不能直接用标准库的 `printf`？

这些问题如果不想清楚，以后写内核调试的时候会非常痛苦。每次想打印一个变量，都不知道该怎么正确地格式化输出。所以这一篇我们会一起手把手实现一个完整的 `printf` 函数，从变长参数到格式化解析，每一步都会解释清楚。

准备好了吗？我们开始。

---

## 环境说明

在开始之前，先确认你已经完成了上一篇的文章，字符串工具库已经能正常工作了。

```
前置条件：
  - kernel/base/strhelpers.h/c 已实现
  - itoa/uitoa/strtol 等函数已通过测试
  - 串口驱动正常工作
```

可能有人会问，为什么不能直接用标准库的 `printf`？内核环境和用户态程序有本质区别。内核是最底层的代码，没法链接 libc，标准库的那些函数根本不存在。而且内核需要输出到串口、VGA 等不同的设备，而不是标准输出。更麻烦的是，内核里可能需要特殊的格式化选项，标准库的 `printf` 不一定支持。所以，我们得自己实现一个 `kprintf`（kernel printf）。

---

## 第一步：设计基础接口

我们先来设计一下接口。我们的 `kprintf` 需要支持哪些功能？

首先是格式化输出，类似 `printf` 的基本能力。然后是指定输出后端，可以选择串口、VGA 或者其他设备。格式符方面，我们需要支持常用的几种：`%c` 字符、`%s` 字符串、`%d` 有符号整数、`%u` 无符号整数、`%x` 十六进制、`%p` 指针。

创建 `kernel/klogs/kprintf_config.h`，存放配置信息：

```c
/**
 * @file kprintf_config.h
 * @brief kprintf 配置定义
 * @date 2026-02-16
 */
#pragma once

// 格式化缓冲区大小
#define KPRINTF_BUFFER_SIZE 512

// 默认日志级别过滤
#define KPRINTF_DEFAULT_FILTERED_LOGLEVEL 2  // INFO 级别
```

这里有个问题：为什么用 512 字节？这是一个经验值。太小可能导致格式化输出被截断，太大会浪费内核内存。你可以根据实际需求调整，但 512 字节对于大多数场景已经足够了。

---

## 第二步：实现变长参数支持

`printf` 最神奇的地方就是它接受可变数量的参数。在 C 语言中，这是通过 `<stdarg.h>` 来实现的。但内核环境里没有标准库，所以我们需要自己实现一个简化版。

创建 `kernel/base/varargs.h`：

```c
/**
 * @file varargs.h
 * @brief 变长参数支持（简化版）
 * @date 2026-02-16
 *
 * 这是针对 GCC x86_64 的简化实现
 * 不同架构的变长参数传递方式可能不同
 */
#pragma once

// va_list 类型：指向可变参数的指针
typedef __builtin_va_list va_list;

// 初始化 va_list
#define va_start(ap, last) __builtin_va_start(ap, last)

// 获取下一个参数
#define va_arg(ap, type) __builtin_va_arg(ap, type)

// 清理 va_list
#define va_end(ap) __builtin_va_end(ap)

// 复制 va_list
#define va_copy(dest, src) __builtin_va_copy(dest, src)
```

等等，为什么用 `__builtin_*`？好问题。GCC 提供了内置的变长参数支持，这些编译器内置函数在不同架构上都能正确工作。如果我们想自己实现，需要了解具体的调用约定（x86_64 用寄存器传递前几个参数，栈传递剩余参数），那就复杂多了。

在 x86_64 上，函数调用遵循 System V AMD64 ABI。前六个整数参数通过寄存器传递（RDI, RSI, RDX, RCX, R8, R9），剩下的参数通过栈传递。变长参数的处理需要知道这些细节，但用 GCC 的内置函数我们就不需要关心这些了。如果你用其他编译器（如 Clang），可能需要调整，不过大多数现代编译器都支持 `__builtin_va_*`。

---

## 第三步：定义 kprintf 接口

现在我们来定义主要的接口函数。创建 `kernel/klogs/kprintf.h`：

```c
/**
 * @file kprintf.h
 * @brief 内核格式化输出系统
 * @date 2026-02-16
 */
#pragma once

#include "base/varargs.h"
#include "kprintf_backends.h"

/**
 * @brief 日志级别枚举
 */
typedef enum {
    KLOG_LEVEL_TRACE = 0,
    KLOG_LEVEL_DEBUG = 1,
    KLOG_LEVEL_INFO  = 2,
    KLOG_LEVEL_WARN  = 3,
    KLOG_LEVEL_ERROR = 4,
} klog_level_t;

/**
 * @brief 初始化 klog 子系统
 *
 * @param backend 默认使用的后端
 * @return true 初始化成功
 */
bool klog_init(klog_backend_t backend);

/**
 * @brief 使用指定后端进行格式化输出
 *
 * @param backend 输出后端
 * @param format 格式化字符串
 * @param ... 可变参数
 */
void kprintf(klog_backend_t backend, const char* format, ...);

/**
 * @brief 使用 va_list 进行格式化输出
 *
 * @param backend 输出后端
 * @param format 格式化字符串
 * @param args 可变参数列表
 */
void kvprintf(klog_backend_t backend, const char* format, va_list args);

/**
 * @brief 设置日志过滤级别
 *
 * @param level 最小输出级别
 */
void klog_set_level(klog_level_t level);

/**
 * @brief 获取当前日志过滤级别
 */
klog_level_t klog_get_level(void);

/**
 * @brief 获取日志级别名称
 */
const char* klog_level_name(klog_level_t level);

// 日志宏
#define klog_trace(format, ...) klog_log(KLOG_LEVEL_TRACE, format, ##__VA_ARGS__)
#define klog_debug(format, ...) klog_log(KLOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define klog_info(format, ...)  klog_log(KLOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define klog_warn(format, ...)  klog_log(KLOG_LEVEL_WARN, format, ##__VA_ARGS__)
#define klog_error(format, ...) klog_log(KLOG_LEVEL_ERROR, format, ##__VA_ARGS__)

// 内部函数
void klog_log(klog_level_t level, const char* format, ...);
```

现在先别急着编译，我们还需要实现后端抽象和核心格式化逻辑。稍后会详细讲。

---

## 第四步：实现核心格式化逻辑

这是 `printf` 的核心：解析格式化字符串，把不同类型的参数转换成字符串。我们会分步骤来实现这个函数，先从最简单的开始。

创建 `kernel/klogs/kprintf.c`，首先添加一些全局变量：

```c
/**
 * @file kprintf.c
 * @brief kprintf 核心实现
 * @date 2026-02-16
 */
#include "kprintf.h"
#include "backends/serial_backends.h"
#include "base/strhelpers.h"
#include "kprintf_config.h"

// 格式化缓冲区
static char g_buffer[KPRINTF_BUFFER_SIZE];

// 当前日志过滤级别
static klog_level_t g_log_level = KPRINTF_DEFAULT_FILTERED_LOGLEVEL;

// 数字转换缓冲区
static char g_format_buffer[32];
```

现在我们来实现 `format_string` 函数，这是格式化的核心。我们先从最简单的版本开始，只支持普通字符和 `%s`：

```c
/**
 * @brief 简化版 vsnprintf 实现（第一版：只支持 %%s）
 */
static int format_string(char* buffer, size_t size, const char* format, va_list args) {
    size_t pos = 0;
    const char* p = format;

    while (*p != '\0' && pos < size - 1) {
        if (*p != '%') {
            // 普通字符，直接复制
            buffer[pos++] = *p++;
            continue;
        }

        // 遇到 '%'，处理格式符
        p++;  // 跳过 '%'

        if (*p == '\0') {
            // 格式字符串以 '%' 结尾，当作普通字符处理
            buffer[pos++] = '%';
            break;
        }

        switch (*p) {
            case '%':
                buffer[pos++] = '%';
                break;
            case 's': {
                const char* s = va_arg(args, const char*);
                if (s == NULL) {
                    s = "(null)";
                }
                while (*s != '\0' && pos < size - 1) {
                    buffer[pos++] = *s++;
                }
                break;
            }
            default:
                // 未知格式符，原样输出
                buffer[pos++] = *p;
                break;
        }
        p++;
    }

    buffer[pos] = '\0';
    return (int)pos;
}
```

这个版本非常简单，只能处理 `%s` 和 `%%`（输出 `%`）。但我们先让它跑起来，然后逐步添加功能。这种逐步迭代的方式可以避免一次性写太多代码导致调试困难。

现在来添加对整数和字符的支持：

```c
case 'c': {
    // 字符
    char c = (char)va_arg(args, int);
    buffer[pos++] = c;
    break;
}
case 'd':
case 'i': {
    // 有符号十进制整数
    int val = va_arg(args, int);
    itoa(val, g_format_buffer, 10);
    char* s = g_format_buffer;
    while (*s != '\0' && pos < size - 1) {
        buffer[pos++] = *s++;
    }
    break;
}
case 'u': {
    // 无符号十进制整数
    unsigned int val = va_arg(args, unsigned int);
    uitoa(val, g_format_buffer, 10);
    char* s = g_format_buffer;
    while (*s != '\0' && pos < size - 1) {
        buffer[pos++] = *s++;
    }
    break;
}
```

注意这里我们用的是 `g_format_buffer` 作为临时缓冲区，每次转换数字的时候复用这个缓冲区。这样可以避免每次都分配新的内存。

最后添加十六进制和指针的支持：

```c
case 'x': {
    // 十六进制整数（小写）
    unsigned int val = va_arg(args, unsigned int);
    uitoa(val, g_format_buffer, 16);
    char* s = g_format_buffer;
    while (*s != '\0' && pos < size - 1) {
        buffer[pos++] = *s++;
    }
    break;
}
case 'p': {
    // 指针（输出为 0x 前缀的十六进制）
    void* ptr = va_arg(args, void*);
    buffer[pos++] = '0';
    if (pos < size - 1)
        buffer[pos++] = 'x';
    uitoa((unsigned int)(uintptr_t)ptr, g_format_buffer, 16);
    char* s = g_format_buffer;
    while (*s != '\0' && pos < size - 1) {
        buffer[pos++] = *s++;
    }
    break;
}
```

这里有个坑要注意：`%p` 的实现我们先输出 `0x` 前缀，然后输出十六进制数字。但我们需要检查缓冲区边界，避免溢出。如果 `pos` 已经是 `size - 1`，我们就只能输出一个字符，另一个会被丢弃。这种情况很少见，但处理边界情况很重要。

---

## 第五步：实现 kvprintf 和 kprintf

现在我们来实现用户调用的函数。`kvprintf` 接受 `va_list`，`kprintf` 接受可变参数：

```c
void kvprintf(klog_backend_t backend, const char* format, va_list args) {
    const KLogBackendOps* ops = klog_get_backend_ops(backend);
    if (ops == NULL || !ops->is_ready()) {
        return;
    }

    static char buffer[KPRINTF_BUFFER_SIZE];
    format_string(buffer, KPRINTF_BUFFER_SIZE, format, args);
    // level = -1 表示无级别，使用默认颜色
    ops->process(buffer, -1);
}

void kprintf(klog_backend_t backend, const char* format, ...) {
    va_list args;
    va_start(args, format);
    kvprintf(backend, format, args);
    va_end(args);
}
```

这里为什么需要两个函数？`kvprintf` 是一个内部函数，接受 `va_list` 参数。这样其他函数也可以复用格式化逻辑，比如我们的 `klog_log` 就会用到它。`kprintf` 是用户调用的函数，它接受可变参数，然后调用 `kvprintf`。这是一个常见的设计模式：`printf` → `vprintf` → 内部实现。

---

## 第六步：实现日志系统

现在我们来实现分级日志功能。首先是级别管理函数：

```c
void klog_set_level(klog_level_t level) {
    g_log_level = level;
}

klog_level_t klog_get_level(void) {
    return g_log_level;
}

const char* klog_level_name(klog_level_t level) {
    switch (level) {
        case KLOG_LEVEL_TRACE:
            return "TRACE";
        case KLOG_LEVEL_DEBUG:
            return "DEBUG";
        case KLOG_LEVEL_INFO:
            return "INFO ";
        case KLOG_LEVEL_WARN:
            return "WARN ";
        case KLOG_LEVEL_ERROR:
            return "ERROR";
        default:
            return "?????";
    }
}
```

注意这里的 `INFO` 和 `WARN` 后面有个空格，这是为了让所有级别名称都是 5 个字符，输出时对齐更美观。

然后是 `klog_init` 函数：

```c
bool klog_init(klog_backend_t backend) {
    // 初始化串口后端
    if (backend == KLOG_BACKEND_SERIAL) {
        if (!klog_serial_backend_init()) {
            return false;
        }
        klog_register_backend(KLOG_BACKEND_SERIAL, klog_serial_backend_get_ops());
    }

    klog_set_default_backend(backend);

    klog_trace("Klog Finished, attempt to send followings...\n");
    klog_trace("========================================================");
    klog_trace("\tCurrent Filtered Level: %s\n", klog_level_name(g_log_level));
    klog_trace("\tCached printf size: %d", KPRINTF_BUFFER_SIZE);
    klog_trace("========================================================");

    return true;
}
```

最后是 `klog_log` 函数：

```c
void klog_log(klog_level_t level, const char* format, ...) {
    // 检查日志级别
    if (level < g_log_level) {
        return;
    }

    klog_backend_t backend = klog_get_default_backend();
    const KLogBackendOps* ops = klog_get_backend_ops(backend);
    if (ops == NULL || !ops->is_ready()) {
        return;
    }

    // 格式：[LEVEL] message\n
    int pos = 0;
    g_buffer[pos++] = '[';

    const char* level_str = klog_level_name(level);
    for (int i = 0; i < 5 && level_str[i] != '\0'; i++) {
        g_buffer[pos++] = level_str[i];
    }

    g_buffer[pos++] = ']';
    g_buffer[pos++] = ' ';

    // 格式化用户消息
    va_list args;
    va_start(args, format);
    int len = format_string(g_buffer + pos, KPRINTF_BUFFER_SIZE - pos - 2, format, args);
    va_end(args);

    pos += len;

    // 添加换行
    g_buffer[pos++] = '\n';
    g_buffer[pos] = '\0';

    ops->process(g_buffer, level);
}
```

这个函数首先检查日志级别，如果太低就直接返回。然后格式化输出为 `[LEVEL] message\n` 的形式，最后通过后端输出。

---

## 第七步：更新 CMakeLists.txt

现在我们需要把这些文件加入构建系统。创建 `kernel/klogs/CMakeLists.txt`：

```cmake
# Kernel logging system
add_library(klogs STATIC
    kprintf.c
)

target_include_directories(klogs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kernel
)

target_link_libraries(klogs PUBLIC
    base
)
```

确保 `kernel/CMakeLists.txt` 包含这个子目录：

```cmake
add_subdirectory(klogs)
```

---

## 第八步：编写测试

现在我们来测试一下 `kprintf` 是否工作正常。在 `kernel_main.c` 中添加测试代码：

```c
#include "klogs/kprintf.h"
#include "klogs/backends/serial_backends.h"

void test_kprintf(void) {
    // 初始化日志系统
    klog_init(KLOG_BACKEND_SERIAL);

    klog_info("=== kprintf Test ===");

    // 测试 %c
    kprintf(KLOG_BACKEND_SERIAL, "Testing %%c: %c %c %c\n", 'A', 'B', 'C');

    // 测试 %s
    kprintf(KLOG_BACKEND_SERIAL, "Testing %%s: %s\n", "Hello, CCOS!");

    // 测试 %d
    kprintf(KLOG_BACKEND_SERIAL, "Testing %%d: %d %d %d\n", 123, -456, 0);

    // 测试 %u
    kprintf(KLOG_BACKEND_SERIAL, "Testing %%u: %u %u\n", 123, 0xFFFFFFFF);

    // 测试 %x
    kprintf(KLOG_BACKEND_SERIAL, "Testing %%x: %x %x\n", 255, 0xDEADBEEF);

    // 测试 %p
    kprintf(KLOG_BACKEND_SERIAL, "Testing %%p: %p\n", test_kprintf);

    // 组合测试
    kprintf(KLOG_BACKEND_SERIAL, "Combined: %s = %d (0x%x)\n", "answer", 42, 42);

    // 特殊字符测试
    kprintf(KLOG_BACKEND_SERIAL, "Special: %% %%%% %%%%\n");

    // 测试日志级别
    klog_info("This is an info message");
    klog_warn("This is a warning message");
    klog_error("This is an error message");

    klog_info("Test completed!");
}
```

编译并运行：

```bash
cd build
cmake ..
make
./run.sh
```

你应该在串口输出中看到类似这样的结果：

```
========================================================
        Current Filtered Level: INFO
        Cached printf size: 512
========================================================
[INFO ] === kprintf Test ===
Testing %c: A B C
Testing %s: Hello, CCOS!
Testing %d: 123 -456 0
Testing %u: 123 4294967295
Testing %x: ff deadbeef
Testing %p: 0x1000a0
Combined: answer = 42 (0x2a)
Special: % %% %%
[INFO ] This is an info message
[WARN ] This is a warning message
[ERROR] This is an error message
[INFO ] Test completed!
```

如果输出不对，先检查编译是否通过，然后看看 `klog_init` 是否被正确调用。我当时遇到过的问题是忘记注册后端，导致所有输出都消失了。

---

## 到这里我们完成了什么

让我们回顾一下这篇文章的内容。我们实现了完整的格式化输出系统，包括：

- `varargs.h` — 变长参数支持，使用 GCC 内置函数
- `format_string` — 核心格式化逻辑，支持多种格式符
- `kprintf` / `kvprintf` — 用户调用的接口函数
- `klog_init` — 日志系统初始化
- `klog_log` — 分级日志输出
- `klog_set_level` / `klog_get_level` — 日志级别管理

这些函数构成了内核调试的基础设施。虽然现在还是只有串口输出，但在下一篇文章中，我们会实现更灵活的后端抽象，让 `kprintf` 可以轻松支持多种输出方式。

说实话，实现 `printf` 的时候我踩了不少坑。最麻烦的是缓冲区边界检查，稍微不注意就会溢出。还有就是 NULL 指针的处理，如果忘记检查，输出 NULL 字符串会导致系统崩溃。这些细节看似简单，但处理不好会带来很隐蔽的 bug。

---

## 接下来

在下一篇文章中，我们会：

1. 设计后端抽象接口
2. 实现后端注册表
3. 实现串口后端
4. 让 kprintf 支持多种输出目标

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 从字符串工具库开始](01_从字符串工具库开始.md)  | [后端抽象与串口输出 →](03_后端抽象与串口输出.md)

</div>
