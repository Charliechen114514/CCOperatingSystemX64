# 05 - Freestanding 环境配置

我们一直在用 `-ffreestanding` 这些选项，但它们到底是干什么的？这一篇我们来彻底搞懂。

---

## 什么是 Freestanding 环境

C 标准定义了两种执行环境：

### Hosted 环境（有宿主）

这是普通程序的环境。你有操作系统，有标准库，可以：

```c
#include <stdio.h>
int main() {
    printf("Hello\n");      // 可以
    malloc(1024);           // 可以
    fopen("file.txt", "r"); // 可以
}
```

### Freestanding 环境（独立运行）

这是裸机/内核环境。**没有操作系统，没有标准库**。

```c
// 没有 #include <stdio.h>
void kernel_main() {
    printf("Hello\n");      // ❌ 不行，没有 printf
    malloc(1024);           // ❌ 不行，没有 malloc
}
```

**我们的内核就是 Freestanding 环境**。

---

## 编译选项详解

GCC 提供了一系列选项来控制 Freestanding 编译。

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

**这么多选项！有没有简化的方式？**

有！`-ffreestanding` 相当于：

```bash
-fno-builtin -fno-stack-protector
```

所以我们最少需要：

```bash
gcc -c kernel_main.c -o kernel_main.o \
    -ffreestanding \
    -nostdlib \
    -mcmodel=large
```

---

## 各选项深入解释

### -ffreestanding

告诉 GCC："这是 freestanding 环境，不要假设有标准库"。

**效果**：
- 不自动链接 libc
- 不假设 `main()` 是入口点
- 允许使用非标准扩展

**没有这个选项会怎样？**

GCC 可能生成对 `_start` 或 `__main` 的引用，链接时报错。

### -fno-builtin

禁用 GCC 的内置函数优化。

**GCC 会做什么"好事"？**

```c
memcpy(dest, src, n);
```

GCC 看到这个，心想："我有优化的 memcpy！"，然后生成对内置 memcpy 的调用。但我们的环境没有 memcpy，链接就失败了。

**`-fno-builtin` 告诉 GCC**：别自作聪明，我用什么函数我自己决定。

### -nostdlib

不链接标准库（libc、libm 等）。

**普通程序链接**：

```bash
gcc hello.c -o hello
# 实际执行：gcc hello.c -o hello -lc -lm ...
```

**我们的内核**：

```bash
gcc -c kernel_main.c -nostdlib
# 不会自动链接任何库
```

### -mcmodel=large

这是 x86_64 特有的选项，控制代码如何访问内存和数据。

**三种代码模型**：

| 模型 | 限制 | 说明 |
|------|------|------|
| small | 代码+数据 < 2GB | 用相对寻址，快 |
| kernel | 代码在负地址空间 | 类似 small，但用于 Linux 内核 |
| large | 无限制 | 用绝对寻址，慢但灵活 |

**我们选择 large 的原因**：

- 内核未来可能增长很大
- 可能加载内核到高地址
- 更灵活的内存布局
- 避免将来迁移时重新编译

**代价**：

- 指针更大（需要 64 位绝对地址）
- 略微降低性能
- 代码体积稍大

但对于内核来说，灵活性更重要。

---

## 内联汇编

在 Freestanding 环境下，我们需要直接用汇编指令。

### 基本语法

```c
__asm__ volatile("hlt");
```

**语法分解**：

- `__asm__` — 内联汇编关键字（`asm` 的宏，避免和变量冲突）
- `volatile` — 告诉编译器："别优化这条指令"
- `"hlt"` — 汇编指令

### 为什么需要 volatile

没有 `volatile`，编译器可能这样"优化"：

```c
while (1) {
    __asm__("hlt");    // 编译器：这个循环什么都没做，删掉！
}
// 变成：while (1) { }
```

`volatile` 强制编译器保留这条指令。

### 更复杂的内联汇编

```c
// 读取 CR3 寄存器（页表基址）
uint64_t cr3;
__asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
```

**语法解释**：

- `%%cr3` — `%%` 转义成 `%`，`%cr3` 是寄存器
- `%0` — 第 0 个输出操作数
- `"=r"(cr3)` — 输出到变量 `cr3`，用任意寄存器

---

## volatile 关键字

除了内联汇编，`volatile` 还用于变量。

### 内存映射 I/O

```c
volatile uint16_t *vga = (uint16_t *)0xB8000;
*vga = 0x1F43;  // 写 VGA
```

**为什么需要 volatile？**

VGA 是内存映射 I/O，每次写入都有实际硬件效果。没有 `volatile`，编译器可能：

```c
*vga = 0x1F43;
*vga = 0x1F44;
*vga = 0x1F45;
// 编译器：只保留最后一个！
*vga = 0x1F45;
```

### 防止优化

```c
volatile int counter = 0;

void wait() {
    while (counter == 0) {
        // 等待中断修改 counter
    }
}
```

没有 `volatile`，编译器可能认为循环条件永远不变，直接优化成死循环或空循环。

---

## ⚠️ 注意

**栈对齐问题**：

SSE/AVX 指令要求栈 16 字节对齐。如果调用 C 函数时栈没对齐，会触发 General Protection Fault。

**在汇编入口设置栈时**：

```nasm
mov rsp, 0x80000 - 8
and rsp, -16    ; 强制对齐
```

**`and rsp, -16` 是什么操作？**

`-16` 的二进制是 `...11110000`，按位与清除低 4 位，实现 16 字节对齐。

---

## 没有 standard headers 怎么办

没有 `<stdio.h>`、`<stdint.h>`，怎么办？

**自己定义！**

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

**下一篇我们会详细讲类型定义**。

---

## 验证选项是否生效

```bash
# 查看实际使用的编译选项
gcc -c kernel_main.c -ffreestanding -nostdlib -v 2>&1 | grep "cc1"

# 查看生成的汇编
gcc -c kernel_main.c -ffreestanding -nostdlib -S -o kernel_main.s
cat kernel_main.s
```

---

## 下一步

很好，到这里我们已经理解了 Freestanding 环境的所有要点。

但有个实际问题：**没有标准库，我们连基本的类型定义都没有**。`uint8_t` 是什么？`size_t` 是什么？

下一篇我们会实现基础类型定义和一些简单的基础库函数。

---

**上一篇**：[04 - 链接脚本与内存布局](./04_链接脚本与内存布局.md)
**下一篇**：[06 - 类型定义与基础库](./06_类型定义与基础库.md)


---

<div align="center">

## 文档导航

[← 链接脚本与内存布局](04_链接脚本与内存布局.md)  | [类型定义与基础库 →](06_类型定义与基础库.md)

</div>
