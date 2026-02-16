# 打造自己的 printf —— Stage 10 格式化输出实战指南

## 前言

说实话，`printf` 可能是 C 语言中最神奇的函数之一。你只需要写一行代码：

```c
printf("Hello, %s! The answer is %d.\n", "world", 42);
```

它就能自动处理不同类型的参数，格式化成字符串，然后输出到终端。

但你想过没有：它是怎么知道后面有几个参数的？它怎么处理 `%d`、`%s` 这些占位符的？为什么内核里不能直接用标准库的 `printf`？

这些问题如果不想清楚，以后写内核调试的时候会非常痛苦。每次想打印一个变量，都不知道该怎么输出。

所以，这一篇我们会一起手把手实现一个完整的 `printf` 函数。别担心，我们不会跳过任何步骤。从变长参数到格式化解析，每一步我们都会解释清楚。

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

**为什么不能直接用标准库的 printf？**

内核环境有几个关键限制：

1. **没有标准库支持**：内核是最底层的，没法链接 libc
2. **输出目标不同**：内核需要输出到串口、VGA，而不是标准输出
3. **需要自定义格式**：可能需要特殊的格式化选项
4. **要考虑性能**：内核里的代码要尽可能高效

所以，我们得自己实现一个 `kprintf`（kernel printf）。

---

## 第一步：设计基础接口

我们先来设计一下接口。我们的 `kprintf` 需要支持以下功能：

1. 格式化输出（类似 `printf`）
2. 指定输出后端（串口、VGA 等）
3. 支持常用格式符：`%c`, `%s`, `%d`, `%u`, `%x`, `%p`

创建 `kernel/klogs/kprintf_config.h`：

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

⚠️ **注意**

为什么用 512 字节？这是一个经验值。太小可能导致格式化输出被截断，太大会浪费内核内存。你可以根据实际需求调整。

---

## 第二步：实现变长参数支持

`printf` 最神奇的地方就是它接受可变数量的参数。在 C 语言中，这是通过 `<stdarg.h>` 来实现的。

但内核环境里没有标准库，所以我们需要自己实现一个简化版。

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

**等等，为什么用 `__builtin_*`？**

好问题！GCC 提供了内置的变长参数支持，这些编译器内置函数在不同架构上都能正确工作。如果我们想自己实现，需要了解具体的调用约定（x86_64 用寄存器传递前几个参数，栈传递剩余参数），那就复杂多了。

⚠️ **注意**

这个实现是针对 GCC 的。如果你用其他编译器（如 Clang），可能需要调整。不过大多数现代编译器都支持 `__builtin_va_*`。

---

## 第三步：定义 kprintf 接口

现在我们来定义主要的接口函数。

创建 `kernel/klogs/kprintf.h`：

```c
/**
 * @file kprintf.h
 * @brief 内核格式化输出系统
 * @date 2026-02-16
 */

#pragma once

#include "base/varargs.h"

/**
 * @brief 后端类型枚举
 */
typedef enum {
    KLOG_BACKEND_NONE = 0,
    KLOG_BACKEND_SERIAL,
    KLOG_BACKEND_VGA,
} klog_backend_t;

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
```

现在先别急着编译，我们还需要实现后端抽象。稍后会详细讲。

---

## 第四步：实现核心格式化逻辑

这是 `printf` 的核心：解析格式化字符串，把不同类型的参数转换成字符串。

创建 `kernel/klogs/kprintf.c`：

```c
/**
 * @file kprintf.c
 * @brief kprintf 核心实现
 * @date 2026-02-16
 */

#include "kprintf.h"
#include "kprintf_config.h"
#include "base/strhelpers.h"

// 格式化缓冲区
static char g_buffer[KPRINTF_BUFFER_SIZE];

// 数字转换缓冲区（32 字节足够存储 64 位整数的任何进制表示）
static char g_format_buffer[32];

/**
 * @brief 简化版 vsnprintf 实现
 *
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 * @param format 格式化字符串
 * @param args 可变参数
 * @return 实际写入的字符数
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
                // 输出 '%'
                buffer[pos++] = '%';
                break;

            case 'c': {
                // 字符
                char c = (char)va_arg(args, int);
                buffer[pos++] = c;
                break;
            }

            case 's': {
                // 字符串
                const char* s = va_arg(args, const char*);
                if (s == NULL) {
                    s = "(null)";
                }
                while (*s != '\0' && pos < size - 1) {
                    buffer[pos++] = *s++;
                }
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

**这个函数做了什么？**

让我解释一下核心逻辑：

1. **遍历格式化字符串**：逐个字符处理
2. **遇到普通字符**：直接复制到缓冲区
3. **遇到 '%'**：查看下一个字符，确定格式符类型
4. **根据格式符处理**：
   - `%c`：取一个 int 参数，转为 char
   - `%s`：取一个 char* 参数，复制字符串
   - `%d`/`%i`：取一个 int 参数，用 itoa 转换
   - `%u`：取一个 unsigned int 参数，用 uitoa 转换
   - `%x`：取一个 unsigned int 参数，转为十六进制
   - `%p`：取一个 void* 参数，输出为 0x 前缀的十六进制
5. **处理未知格式符**：原样输出

⚠️ **注意缓冲区边界**

我们始终检查 `pos < size - 1`，确保不会溢出缓冲区。最后一位留给 `'\0'` 终止符。

---

## 第五步：实现 kvprintf 和 kprintf

现在我们来实现用户调用的函数。

在 `kprintf.c` 中添加：

```c
void kvprintf(klog_backend_t backend, const char* format, va_list args) {
    // 暂时先硬编码串口输出
    // 后续我们会实现后端抽象
    extern void serial_write_string(const char* str);

    // 格式化字符串
    format_string(g_buffer, KPRINTF_BUFFER_SIZE, format, args);

    // 输出到串口
    serial_write_string(g_buffer);
}

void kprintf(klog_backend_t backend, const char* format, ...) {
    va_list args;
    va_start(args, format);
    kvprintf(backend, format, args);
    va_end(args);
}
```

**为什么需要两个函数？**

`kvprintf` 是一个内部函数，接受 `va_list` 参数。这样其他函数也可以复用格式化逻辑。`kprintf` 是用户调用的函数，它接受可变参数，然后调用 `kvprintf`。

这是一个常见的设计模式：`printf` → `vprintf` → 内部实现。

---

## 第六步：更新 CMakeLists.txt

现在我们需要把这些文件加入构建系统。

创建 `kernel/klogs/CMakeLists.txt`：

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

同时，确保 `kernel/CMakeLists.txt` 包含这个子目录：

```cmake
add_subdirectory(klogs)
```

---

## 第七步：编写测试

现在我们来测试一下 `kprintf` 是否工作正常。

在 `kernel_main.c` 中添加测试代码：

```c
#include "klogs/kprintf.h"
#include "driver/serial/serial.h"

void test_kprintf(void) {
    // 初始化串口（假设你已经实现了）
    serial_init();

    klog_info("=== kprintf Test ===\n");

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

    klog_info("Test completed!\n");
}

void kernel_main(void) {
    // ... 其他初始化代码 ...

    test_kprintf();

    // ... 其他代码 ...
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
=== kprintf Test ===
Testing %c: A B C
Testing %s: Hello, CCOS!
Testing %d: 123 -456 0
Testing %u: 123 4294967295
Testing %x: ff deadbeef
Testing %p: 0x1000a0
Combined: answer = 42 (0x2a)
Special: % %% %%
Test completed!
```

⚠️ **如果输出不对**

检查以下几点：
1. `klogs` 库是否被正确链接
2. `serial_write_string` 是否正确实现
3. 格式化字符串是否正确
4. 可变参数处理是否正确

---

## 踩坑预警

### 坑 1：变长参数的类型提升

```c
// 错误写法
char c = 'A';
kprintf(KLOG_BACKEND_SERIAL, "%c", c);  // 会有警告

// 正确写法
kprintf(KLOG_BACKEND_SERIAL, "%c", 'A');  // 字符常量是 int 类型
```

在 C 语言中，可变参数会被自动提升：`char` → `int`，`float` → `double`。所以 `va_arg` 总是用 `int` 来获取字符。

### 坑 2：缓冲区溢出

```c
// 危险写法
char huge_buffer[1000];
kprintf(KLOG_BACKEND_SERIAL, "%s", huge_buffer);  // 可能溢出
```

我们的 `format_string` 会检查缓冲区边界，但如果格式化字符串太长，输出会被截断。确保 `KPRINTF_BUFFER_SIZE` 足够大。

### 坑 3：NULL 指针

```c
// 我们在代码中处理了 NULL 指针
if (s == NULL) {
    s = "(null)";
}
```

如果忘记处理，输出 NULL 字符串会导致系统崩溃。这是一个常见的安全漏洞。

### 坑 4：格式字符串不匹配

```c
// 错误写法
kprintf(KLOG_BACKEND_SERIAL, "%d", "hello");  // 类型不匹配！

// 正确写法
kprintf(KLOG_BACKEND_SERIAL, "%s", "hello");
```

格式符和参数类型不匹配会导致未定义行为。编译器可能会警告，但不会报错。

---

## 检查清单

在继续下一篇文章之前，请确认：

- [ ] 创建了 `kernel/klogs/` 目录
- [ ] 实现了 `varargs.h`（变长参数支持）
- [ ] 实现了 `format_string` 函数
- [ ] 实现了 `kprintf` 和 `kvprintf`
- [ ] 更新了 CMakeLists.txt
- [ ] 编译通过，没有警告
- [ ] 测试输出正确
- [ ] 理解格式化解析的流程
- [ ] 理解为什么需要 kvprintf

如果以上全部勾选，恭喜你！你已经实现了一个完整的格式化输出系统。虽然现在还是硬编码串口输出，但在下一篇文章中，我们会实现更灵活的后端抽象。

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
