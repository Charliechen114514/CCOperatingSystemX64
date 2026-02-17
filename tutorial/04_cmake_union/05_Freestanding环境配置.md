# Freestanding 环境配置

我们一直在用 `-ffreestanding` 这些选项，但它们到底是干什么的？说实话，我第一次看到这些选项的时候也是一头雾水。这一篇我们来彻底搞懂，因为这是理解内核开发"为什么不一样"的关键。

---

## 什么是 Freestanding 环境

C 标准定义了两种执行环境，很多人写了很多年代码，但从来不知道这个区别。理解了这个，你对 C 语言的认识会更上一层楼。

### Hosted 环境（有宿主）

这是普通程序的环境，你日常写的所有程序几乎都是这个环境。你有操作系统，有标准库，可以随意调用各种函数。

```c
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Hello\n");      // 可以，有标准库
    malloc(1024);           // 可以，有内存分配器
    fopen("file.txt", "r"); // 可以，有文件系统
    return 0;
}
```

在这个环境里，程序运行前会先执行一堆初始化代码（在 `crt0.o`、`crti.o` 这些文件里）。这些代码会设置标准库、初始化堆、设置环境变量等。然后才调用你的 `main` 函数。程序退出时，还会有清理代码，关闭文件、释放内存等。

你享受的所有这些便利，都是操作系统和标准库提供的。但你可能从来没有意识到它们的存在。

### Freestanding 环境（独立运行）

这是裸机/内核环境。**没有操作系统，没有标准库，什么都没有。**

```c
// 没有 #include <stdio.h>，没有 #include <stdlib.h>
void kernel_main() {
    printf("Hello\n");      // ❌ 不行，没有 printf
    malloc(1024);           // ❌ 不行，没有 malloc
    fopen("file.txt", "r"); // ❌ 不行，没有文件系统
}
```

在这个环境里，你的程序直接运行在硬件上。没有操作系统来帮你，没有标准库来提供便利函数。所有的东西都要你自己来。

**我们的内核就是 Freestanding 环境。** 我们自己设置栈、自己清零 BSS、自己实现所有需要的功能。

---

## 编译选项详解

GCC 提供了一系列选项来控制 Freestanding 编译。这些选项你可能见过，但可能不太理解它们的作用。我们来一个个搞懂。

### 核心选项表格

| 选项 | 含义 | 为什么需要 |
|------|------|-----------|
| `-ffreestanding` | 声明 freestanding 环境 | 不依赖标准库，移除内置假设 |
| `-fno-builtin` | 禁用内置函数 | 避免 memcpy 等函数被优化成不存在 |
| `-fno-stack-protector` | 禁用栈保护 | 没有支持栈保护的运行时 |
| `-nostdlib` | 不链接标准库 | 我们没有 libc |
| `-nostdinc` | 不搜索标准头文件 | 防止意外包含系统头文件 |
| `-nostartfiles` | 不使用标准启动文件 | 没有 crt0.o 之类的东西 |
| `-nodefaultlibs` | 不链接默认库 | 不自动链接任何系统库 |
| `-mcmodel=large` | 大代码模型 | 支持任意大小的内核 |

### 完整编译命令

如果我们把所有选项都列出来，编译命令会很长：

```bash
gcc -c kernel_main.c -o kernel_main.o \
    -ffreestanding \
    -fno-builtin \
    -fno-stack-protector \
    -nostdlib \
    -nostdinc \
    -nostartfiles \
    -nodefaultlibs \
    -mcmodel=large \
    -m64
```

这看起来很吓人，但其实有些选项是重复的。

**简化一下**：`-ffreestanding` 已经包含了 `-fno-builtin` 和 `-fno-stack-protector`。`-nostdlib` 已经包含了 `-nostartfiles` 和 `-nodefaultlibs`。

所以我们最少需要：

```bash
gcc -c kernel_main.c -o kernel_main.o \
    -ffreestanding \
    -nostdlib \
    -mcmodel=large
```

这样就简洁多了。但理解每个选项的作用还是很重要的，这样出了问题才知道往哪里查。

---

## 各选项深入解释

### -ffreestanding

这个选项是核心，它告诉 GCC："这是 freestanding 环境，不要假设有标准库"。

**效果**：
- 不自动链接 libc
- 不假设 `main()` 是入口点
- 允许使用非标准扩展
- 移除一些对标准库的内置假设

**没有这个选项会怎样？**

GCC 可能生成对 `_start` 或 `__main` 的引用，链接时报错 `undefined reference to '_start'`。这些函数通常是标准启动文件提供的，但我们没有。

### -fno-builtin

这个选项禁用 GCC 的内置函数优化。GCC 有一个"贴心"的功能：它会用自己优化的版本替换标准库函数。

**GCC 会做什么"好事"？**

```c
memcpy(dest, src, n);
```

GCC 看到这个，心想："memcpy 调用太慢了，我有一个优化的版本！"，然后生成对内置 memcpy 的调用。但我们的环境没有 memcpy，链接就失败了。

更糟糕的是，有时候 GCC 会完全优化掉某些调用。比如 `memcpy(p, p, n)`（自己复制自己），GCC 可能直接删掉这行代码。这在普通环境是对的，但在内存映射 I/O 的场景下会出错。

**`-fno-builtin` 告诉 GCC**：别自作聪明，我用什么函数我自己决定。所有函数调用都按照源代码来生成。

### -nostdlib

这个选项告诉 GCC：不要链接标准库（libc、libm 等）。

**普通程序链接**：

```bash
gcc hello.c -o hello
# 实际执行：gcc hello.c -o hello -lc -lm -lgcc ...
```

GCC 会自动链接一堆库：libc（C 标准库）、libm（数学库）、libgcc（GCC 内置函数）等。

**我们的内核**：

```bash
gcc -c kernel_main.c -nostdlib
# 不会自动链接任何库
```

没有 `-nostdlib`，链接器会尝试链接 libc，但我们没有提供 libc，所以会报错 `cannot find -lc`。

### -mcmodel=large

这是 x86_64 特有的选项，控制代码如何访问内存和数据。很多人不知道这个选项，但它确实很重要。

**三种代码模型**：

| 模型 | 限制 | 说明 |
|------|------|------|
| small | 代码+数据 < 2GB | 用相对寻址，快 |
| kernel | 代码在负地址空间 | 类似 small，但用于 Linux 内核 |
| large | 无限制 | 用绝对寻址，慢但灵活 |

**small 模型**下，所有代码和数据在 2GB 范围内，编译器可以用相对寻址（`call`、`jmp` 的相对偏移）。这样生成的代码小、速度快。

**large 模型**下，代码和数据可以放在任意地址，编译器必须用绝对寻址（通过 GOT 或直接绝对地址）。这样生成的代码更大、更慢，但更灵活。

**我们选择 large 的原因**：

- 内核未来可能增长很大，超过 2GB
- 可能加载内核到高地址（比如 0xFFFFFFFF80000000）
- 更灵活的内存布局，方便后续扩展
- 避免将来迁移时重新编译

**代价**：

- 每个函数调用可能多几条指令（加载地址）
- 每个全局变量访问可能多几条指令
- 代码体积稍大（大约 10-20%）

但对于内核来说，灵活性更重要。而且内核代码不会特别大（不像浏览器那样动辄几百 MB），这点性能损失可以接受。

---

## 内联汇编

在 Freestanding 环境下，我们经常需要直接用汇编指令。GCC 的内联汇编语法有点怪，但理解了就不难。

### 基本语法

```c
__asm__ volatile("hlt");
```

**语法分解**：

- `__asm__` — 内联汇编关键字（`asm` 的宏，避免和变量名冲突）
- `volatile` — 告诉编译器："别优化这条指令"
- `"hlt"` — 汇编指令

为什么用 `__asm__` 而不是 `asm`？因为 `asm` 可能和变量名冲突，比如 `int asm;`。用 `__asm__` 就不会有这个问题。

### 为什么需要 volatile

没有 `volatile`，编译器可能这样"优化"你的代码：

```c
while (1) {
    __asm__("hlt");    // 编译器：这个循环什么都没做，删掉！
}
// 变成：while (1) { }
```

这显然不是我们想要的。`hlt` 指令有实际作用（让 CPU 停机等待中断），不能删掉。`volatile` 强制编译器保留这条指令。

### 更复杂的内联汇编

如果你需要读写寄存器，GCC 的内联汇编语法会更复杂一点：

```c
// 读取 CR3 寄存器（页表基址）
uint64_t cr3;
__asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
```

**语法解释**：

- `%%cr3` — `%%` 转义成 `%`，`%cr3` 是寄存器名
- `%0` — 第 0 个输出操作数（按出现顺序，从 0 开始）
- `"=r"(cr3)` — 输出到变量 `cr3`，用任意寄存器

这个语法确实有点怪，这是 GCC 的传统。它很像一个微型语言：冒号前面是汇编指令，冒号后面是操作数描述。

操作数描述的格式是：`"约束"(变量)`。`"=r"` 表示输出（`=`），用通用寄存器（`r`）。其他约束有 `"m"`（内存）、`"i"`（立即数）等。

内联汇编是一个大话题，可以写一整篇教程。现在你只需要知道基本用法，以后遇到复杂场景再深入。

---

## volatile 关键字

除了内联汇编，`volatile` 还用于变量。这个关键字的作用很多人理解不深，我们来说清楚。

### 内存映射 I/O

```c
volatile uint16_t *vga = (uint16_t *)0xB8000;
*vga = 0x1F43;  // 写 VGA
```

**为什么需要 volatile？**

VGA 是内存映射 I/O，每次写入都有实际的硬件效果。屏幕上会显示一个字符。

没有 `volatile`，编译器可能：

```c
*vga = 0x1F43;
*vga = 0x1F44;
*vga = 0x1F45;
// 编译器：只保留最后一个！
*vga = 0x1F45;
```

编译器想："前两次写没用，直接覆盖了，不如删掉。"但这对 VGA 来说不对！每次写入都显示不同的字符，删掉就错了。

`volatile` 告诉编译器："每次写入都要执行，别优化。"

### 防止优化

另一个场景是等待中断：

```c
volatile int counter = 0;

void wait() {
    while (counter == 0) {
        // 等待中断修改 counter
    }
}
```

中断服务程序会修改 `counter`，让循环退出。没有 `volatile`，编译器可能认为循环条件永远不变（`counter` 始终是 0），直接优化成死循环或空循环。

`volatile` 告诉编译器："这个变量可能被外部修改，每次都要重新读取。"

---

## ⚠️ 栈对齐问题

这里有一个非常隐蔽但致命的坑点。SSE/AVX 指令要求栈 16 字节对齐。如果调用 C 函数时栈没对齐，会触发 General Protection Fault。而且这个错误很难以排查，因为症状不明显。

**在汇编入口设置栈时**：

```nasm
mov rsp, 0x80000 - 8
and rsp, -16    ; 强制对齐
```

**`and rsp, -16` 是什么操作？**

`-16` 的二进制是 `...11110000`，按位与清除低 4 位，实现 16 字节对齐。比如 `RSP = 0x80000 - 8 = 0x7FFF8`，与 `-16` 后变成 `0x7FFF0`，16 的倍数。

为什么是 `-8` 而不是直接 `0x80000`？因为 bootloader 用 `call` 跳转过来，已经压了 8 字节的返回地址。我们需要考虑这个已有的值，确保最终的栈是 16 字节对齐的。

栈对齐问题真的非常隐蔽。如果你的内核有时候正常、有时候崩溃，而且崩溃地点不固定，很可能是栈对齐问题。用 GDB 检查 RSP 的值，看看低 4 位是否为 0。

---

## 没有标准头文件怎么办

没有 `<stdio.h>`、`<stdint.h>`，怎么办？很简单，**自己定义！**

```c
// types.h - 自定义类型
#ifndef CCOS_TYPES_H
#define CCOS_TYPES_H

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef unsigned long      size_t;

#define NULL ((void*)0)

#endif
```

这不就是几行 `typedef` 吗？没错，这就是类型定义的全部秘密。标准库的 `<stdint.h>` 做的也是类似的事情，只是加了更多平台兼容性代码。

对于我们的内核，我们只支持 x86_64，所以可以简化很多。而且自己定义的好处是：你知道每一行做了什么，不会有"魔法"代码。

**下一篇我们会详细讲类型定义**，包括如何实现一个完整的基础库。

---

## 验证选项是否生效

配置完这些选项，如何验证它们生效了？

```bash
# 查看实际使用的编译选项
gcc -c kernel_main.c -ffreestanding -nostdlib -v 2>&1 | grep "cc1"

# 查看生成的汇编
gcc -c kernel_main.c -ffreestanding -nostdlib -S -o kernel_main.s
cat kernel_main.s
```

第一个命令会显示 GCC 的内部命令，你可以看到它实际用了哪些选项。第二个命令会生成汇编文件，你可以检查生成的代码是否符合预期。

比如，如果你看到汇编里有对 `memcpy` 的调用，说明 `-fno-builtin` 没生效。如果你看到 `call main` 而不是 `call kernel_main`，说明入口点设置有问题。

---

## 下一步

很好，到这里我们已经理解了 Freestanding 环境的所有要点。我们学会了：
- 什么是 Freestanding 环境
- 各种 GCC 选项的作用
- 内联汇编的基本语法
- volatile 关键字的用途
- 栈对齐的重要性

但有个实际问题：**没有标准库，我们连基本的类型定义都没有**。`uint8_t` 是什么？`size_t` 是什么？这些都要自己来。

下一篇我们会实现基础类型定义和一些简单的基础库函数。这是构建完整内核的基础，也是理解"没有标准库怎么活"的关键。

在继续之前，建议你试一下用这些选项编译一个简单的 C 文件，看看能生成什么样的汇编代码。理解了编译器的输出，你才能更好地配置它。

---

<div align="center">

## 文档导航

[← 链接脚本与内存布局](04_链接脚本与内存布局.md)  | [类型定义与基础库 →](06_类型定义与基础库.md)

</div>
