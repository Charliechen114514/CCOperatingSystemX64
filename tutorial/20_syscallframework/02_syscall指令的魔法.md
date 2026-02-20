# syscall 指令的魔法 —— Stage 20 系统调用框架实战指南（二）

## 前言

在上一篇文章里，我们讲了为什么要系统调用，以及系统调用在整个操作系统中的地位。但我相信很多人心里还是有个疑问：一条 `syscall` 指令到底是怎么工作的？CPU 怎么知道要跳转到哪里？寄存器是怎么保存和恢复的？这些问题的答案藏在 x86_64 架构的一些微妙设计里。

说实话，我刚接触这个话题的时候也被搞糊涂了。Intel 的手册有几千页，关于 MSR 寄存器的描述散落在各个角落，网上的一些教程又写得不够详细。所以在这篇文章里，我会尽量把这些东西讲清楚，让你真正理解 syscall 指令背后的"魔法"。

---

## syscall 指令的工作原理

当用户态程序执行 `syscall` 指令时，CPU 不会简单地跳转到某个地址。它会执行一系列预定义的操作，这些操作是硬件硬编码的，无法修改。理解这些操作对于编写正确的系统调用入口代码至关重要。

CPU 执行 `syscall` 时会做以下事情：

**第一步：保存用户模式状态**

CPU 会把当前的 RIP（指令指针）和 RFLAGS（标志寄存器）保存到两个特定的寄存器中：

```
RCX ← 用户 RIP (syscall 指令下一条指令的地址)
R11 ← 用户 RFLAGS (当前的标志位状态)
```

为什么要保存到 RCX 和 R11？因为 syscall 指令的设计者假设系统调用处理函数会尽快使用 `sysretq` 返回，而 `sysretq` 恰好从 RCX 和 R11 恢复用户状态。这种设计避免了内存访问，提高了速度。

注意这里有个关键点：syscall 指令**不**保存其他通用寄存器（RAX、RBX、RBP、R12-R15 等）。这些寄存器的保存和恢复是我们的汇编入口代码需要手动处理的。

**第二步：加载内核模式状态**

接下来，CPU 需要知道要跳转到哪个地址执行内核代码。这个信息存储在 MSR（Model-Specific Register）寄存器中：

```
RIP ← IA32_LSTAR (syscall 目标地址，即我们的 syscall_handler)
CS  ← IA32_STAR[47:32] (内核代码段选择子)
SS  ← IA32_STAR[47:32] + 8 (内核数据段选择子)
```

IA32_LSTAR 是一个 64 位的 MSR 寄存器，它的值就是我们内核入口函数的地址。IA32_STAR 也是一个 MSR 寄存器，但它存储的是段选择子，而不是地址。具体来说，IA32_STAR 的第 47-32 位存储内核 CS，第 63-48 位存储用户 CS（用于 sysret 返回时）。

这里有个细节：CPU 会自动把 SS 设置为 CS + 8。这是因为 x86 的 GDT（全局描述符表）中，代码段和数据段通常是相邻的，代码段选择子 + 8 就是数据段选择子。

**第三步：修改 RFLAGS**

syscall 指令会清除 RFLAGS 中的某些位，这是由 IA32_FMASK 寄存器控制的：

```
RFLAGS ← RFLAGS & ~IA32_FMASK
```

IA32_FMASK 是一个 32 位的 MSR 寄存器，它的每一位代表一个需要清除的 RFLAGS 位。最常见的设置是 0x200，这会清除 IF 位（中断标志），即在系统调用处理期间禁用中断。当然，我们的处理函数可以在保存状态后重新启用中断。

**第四步：切换特权级**

最后，CPU 会将特权级从 Ring 3（用户态）切换到 Ring 0（内核态）。这个切换是通过修改 CS 段选择子的低 2 位（RPL）实现的。

特权级切换后，CPU 就开始执行从 RIP 加载的内核代码了。注意整个过程中，CPU **没有**访问栈内存，没有推送任何栈帧，这与 int 0x80 中断完全不同。

---

## sysretq 指令的工作原理

当内核处理完系统调用，需要返回用户态时，会执行 `sysretq` 指令（q 表示 quad，操作 64 位数据）。这条指令是 syscall 的逆操作：

**第一步：恢复用户模式状态**

```
RIP ← RCX (用户返回地址)
RFLAGS ← R11 (用户标志位)
```

这就是为什么我们的汇编入口需要把用户 RIP 和 RFLAGS 保存到 RCX 和 R11 —— 因为 sysretq 要求从这两个寄存器恢复。

**第二步：加载用户模式段**

```
CS ← IA32_STAR[63:48] | 3 (用户代码段选择子，RPL=3)
SS ← IA32_STAR[63:48] + 8 | 3 (用户数据段选择子，RPL=3)
```

注意这里有一个容易被忽略的细节：CPU 会自动将用户 CS 的最低 2 位设置为 3（RPL=3），表示用户态特权级。如果我们配置的 IA32_STAR 值不正确，这里可能会出现问题。

**第三步：切换特权级**

CPU 将特权级从 Ring 0 切换回 Ring 3，然后开始执行用户代码。

同样，sysretq 不会自动恢复其他通用寄存器，这些寄存器的恢复是我们的汇编出口代码需要处理的。

---

## MSR 寄存器详解

MSR（Model-Specific Register）是 x86 架构中的一类特殊寄存器，用于控制 CPU 的各种行为。不同的 CPU 型号可能有不同的 MSR 寄存器，这也是"Model-Specific"这个名字的由来。

对于系统调用，我们需要关注三个 MSR 寄存器：IA32_LSTAR、IA32_STAR 和 IA32_FMASK。

### IA32_LSTAR (0xC0000082) - Long Mode SYSCALL Target Address

这个 MSR 寄存器存储 syscall 指令的目标地址，也就是我们的汇编入口函数的地址：

```c
uint64_t lstar = (uint64_t)syscall_handler;
wrmsr(0xC0000082, lstar);
```

`wrmsr` 指令用于写 MSR 寄存器，第一个参数是 MSR 地址，第二个参数是要写入的值。

### IA32_STAR (0xC0000081) - SYSCALL Target Address Register

这个 MSR 寄存器存储段选择子信息，布局如下：

```
63:48 - SYSRET CS (用户模式代码段选择子)
47:32 - syscall CS (内核模式代码段选择子)
31:0  - 保留
```

配置示例：

```c
// 内核 CS = GDT_KERNEL_CODE = 0x08
// 用户 CS = GDT_USER_CODE | 3 = 0x18 | 3 = 0x1B
uint64_t star = ((uint64_t)(GDT_USER_CODE | 3) << 48) |
                ((uint64_t)GDT_KERNEL_CODE << 32);
wrmsr(0xC0000081, star);
```

这里有个容易出错的地方：用户 CS 必须设置 RPL=3，即 `GDT_USER_CODE | 3`。如果你忘记这个，sysretq 返回时特权级会出错，触发异常。

### IA32_FMASK (0xC0000084) - SYSCALL Flag Mask

这个 MSR 寄存器指定需要在 syscall 时清除的 RFLAGS 位：

```c
#define SYSCALL_FMASK_DEFAULT 0x200  // 清除 IF 位（禁用中断）
wrmsr(0xC0000084, SYSCALL_FMASK_DEFAULT);
```

常见的 RFLAGS 位包括：
- 0x001 (CF): 进位标志
- 0x040 (TF): 陷阱标志（用于单步调试）
- 0x080 (SF): 符号标志
- 0x200 (IF): 中断标志（禁用/启用中断）

---

## System V AMD64 ABI 调用约定

系统调用需要遵循特定的调用约定，即参数如何传递、返回值如何返回。在 Linux 和大多数现代 Unix 系统上，这个约定是 System V AMD64 ABI。

对于系统调用，参数传递规则如下：

| 寄存器 | 用途 |
|--------|------|
| RAX | 系统调用号（输入）/ 返回值（输出） |
| RDI | 第 1 个参数 |
| RSI | 第 2 个参数 |
| RDX | 第 3 个参数 |
| R10 | 第 4 个参数 |
| R8  | 第 5 个参数 |
| R9  | 第 6 个参数 |

这里有个关键的区别：系统调用使用 **R10** 作为第 4 个参数，而普通的函数调用使用 **RCX**。为什么？因为 syscall 指令会用 RCX 保存用户 RIP，所以 RCX 不能用于传递参数。

这个区别很容易搞混，特别是在写汇编代码的时候。你可能习惯于 RCX 是第 4 个参数，但在系统调用的场景下，必须用 R10。

返回值通过 RAX 传递。正数或零表示成功，负数表示错误（遵循 POSIX 的错误码约定）。

---

## 与普通函数调用的区别

你可能疑惑：为什么不直接用函数调用？系统调用和函数调用有什么区别？

第一个区别是**特权级切换**。普通函数调用不改变特权级，而系统调用会从 Ring 3 切换到 Ring 0。这个切换是硬件强制检查的，用户代码无法伪造。

第二个区别是**寄存器保存**。普通函数调用遵循 System V ABI， callee 只需要保存 callee-saved 寄存器（RBX、RBP、R12-R15）。而系统调用需要保存所有寄存器，因为用户程序期望系统调用"透明"地返回，就像什么都没发生过一样。

第三个区别是**栈**。普通函数调用使用当前的栈（用户栈或内核栈），而系统调用会切换到内核栈（通过 TSS）。但在 x86_64 的 syscall 指令中，栈切换不是自动的，我们需要在汇编入口中手动切换栈（如果需要的话）。

---

## CPU 特性检测

在配置 syscall 之前，我们需要先确认 CPU 是否支持 syscall/sysret 指令。不是所有 x86_64 CPU 都支持这个特性，虽然现代 CPU 几乎都支持了。

检测方法是通过 CPUID 指令：

```c
bool syscall_is_available(void) {
    uint32_t eax, ebx, ecx, edx;
    // CPUID leaf 0x80000001, EDX bit 11 = syscall/sysret support
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(0x80000001));
    return (edx & (1 << 11)) != 0;
}
```

CPUID 是一个特殊的指令，用于查询 CPU 的特性信息。不同的"leaf"（输入 EAX）返回不同的信息。leaf 0x80000001 是扩展功能信息，EDX 的第 11 位表示是否支持 syscall/sysret。

---

## 完整的调用流程示例

让我们看一个具体的例子：用户程序调用 `write(1, "Hello", 5)`，向标准输出写入 5 字节数据。

**用户代码**（用内联汇编展示）：

```c
const char* msg = "Hello";
long ret;
__asm__ volatile(
    "mov %1, %%rax\n"     // 系统调用号 SYS_WRITE
    "mov %2, %%rdi\n"     // 文件描述符 1
    "mov %3, %%rsi\n"     // 缓冲区地址
    "mov %4, %%rdx\n"     // 长度 5
    "syscall\n"           // 发起系统调用
    "mov %%rax, %0\n"     // 保存返回值
    : "=r"(ret)
    : "i"(SYS_WRITE), "r"(1), "r"(msg), "r"(5)
    : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
);
```

**CPU 自动操作**（syscall 指令执行时）：

```
1. RCX ← 用户 RIP (syscall 指令的下一条指令)
2. R11 ← 用户 RFLAGS
3. RIP ← IA32_LSTAR (syscall_handler 地址)
4. CS ← IA32_STAR[47:32] (0x08, 内核代码段)
5. SS ← 0x10 (内核数据段)
6. RFLAGS ← RFLAGS & ~IA32_FMASK (清除 IF 位)
7. 切换到 Ring 0
```

**汇编入口**（syscall_handler）：

```
1. 保存所有通用寄存器（RAX, RBX, RBP, R12-R15）
2. 单独保存 RCX 和 R11（用户 RIP/RFLAGS）
3. 加载内核数据段（DS, ES, FS, GS）
4. 栈对齐（16 字节）
5. 构建 syscall_frame_t 结构
6. 调用 C 分发器 syscall_dispatch
```

**C 分发器**（syscall_dispatch）：

```
1. 读取系统调用号 (frame->syscall_number = SYS_WRITE)
2. 查找系统调用表 (s_syscall_table[SYS_WRITE])
3. 调用处理函数 sys_write(frame)
```

**处理函数**（sys_write）：

```
1. 读取参数：fd=frame->arg0, buf=frame->arg1, count=frame->arg2
2. 验证文件描述符 (fd == 1 表示 stdout)
3. 输出数据到串口/VGA
4. 返回写入的字节数 (5)
```

**汇编返回**（syscall_handler 返回部分）：

```
1. 清理栈（删除 syscall_frame_t）
2. 恢复用户 RIP/FLAGS (RCX, R11)
3. 恢复所有通用寄存器
4. sysretq 指令返回用户态
```

**用户代码继续**：

```
ret 变量现在包含返回值 (5)
```

---

## 接下来

现在我们理解了 syscall 指令的工作原理，以及 MSR 寄存器是如何控制这个过程的。在下一篇文章中，我们会开始动手写代码，首先创建系统调用框架的基础结构：目录、CMakeLists.txt、头文件等。

搭建好脚手架之后，我们再逐步实现 MSR 配置、汇编入口、C 分发器等组件。一步一步来，别着急。

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 从轮询到syscall](01_从轮询到syscall.md)  | [搭建syscall脚手架 →](03_搭建syscall脚手架.md)

</div>
