# MSR 寄存器配置实战 —— Stage 20 系统调用框架实战指南（四）

## 前言

在之前的文章里，我们讲了 syscall 指令的工作原理，也搭建好了基本的代码框架。现在要进入真正硬核的部分了：配置 MSR 寄存器，让 syscall 指令真正工作起来。

说实话，这部分代码是我写的时候最小心的一段。为什么？因为 MSR 寄存器配置错误不会马上崩溃，而是会在你调用 syscall 的时候触发一个诡异的 General Protection 异常，调试起来非常痛苦。所以在这篇文章里，我会把每一步的细节都讲清楚，确保你不会踩那些我曾经踩过的坑。

---

## 配置前的准备工作

在配置 MSR 之前，我们需要做两件事：检测 CPU 是否支持 syscall/sysret，以及启用 CR4 的 SCE 位。

### CPU 特性检测

我们之前在 `syscall.c` 中已经写了 `syscall_is_available()` 函数，它通过 CPUID 指令检测 CPU 是否支持 syscall/sysret。这个函数会在初始化时被调用。

### CR4 的 SCE 位

CR4 是控制寄存器 4，其中的第 11 位（SCE，System Call Extension）控制是否启用 syscall/sysret 指令。如果这个位是 0，执行 syscall 指令会触发 Invalid Opcode 异常。

启用 SCE 位的代码很简单：

```c
/* Enable SCE bit in CR4 */
uint64_t cr4;
__asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
cr4 |= (1 << 11);  // Set bit 11
__asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
```

但这里有个重要的细节：**必须先启用 SCE 位，再配置 MSR 寄存器**。顺序错了会触发 GP 异常。这是 Intel 手册明确要求的，别问为什么，硬件就是这样设计的。

---

## 实现 syscall_init 函数

现在让我们完整实现 `syscall_init()` 函数。打开 `syscall.c`，在文件末尾添加以下代码：

```c
/* Forward declaration for registration function */
extern void syscall_register_all(void);

void syscall_init(void) {
    if (s_initialized) {
        klog_warn("[SYSCALL] Already initialized\n");
        return;
    }

    klog_info("[SYSCALL] Initializing system call framework...\n");

    /* Step 1: Check CPU support */
    if (!syscall_is_available()) {
        klog_error("[SYSCALL] CPU does not support syscall/sysret\n");
        return;
    }
    klog_info("[SYSCALL] syscall/sysret supported\n");

    /* Step 2: Clear syscall table */
    memset(s_syscall_table, 0, sizeof(s_syscall_table));

    /* Step 3: Register default handlers */
    syscall_register_handler(SYS_DEBUG_LOG, syscall_debug_log, "debug_log");
    syscall_register_handler(SYS_TEST, syscall_test, "test");

    /* Step 4: Register all syscall handlers from syscall_table.c */
    syscall_register_all();

    /* Step 5: Enable SCE bit in CR4 */
    klog_trace("[SYSCALL] About to enable SCE bit in CR4...\n");

    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    klog_trace("[SYSCALL] Current CR4 = 0x%016llX\n", cr4);

    if (cr4 & (1 << 11)) {
        klog_trace("[SYSCALL] SCE bit already set in CR4\n");
    } else {
        cr4 |= (1 << 11);
        klog_trace("[SYSCALL] Setting CR4 to 0x%016llX (before write)\n", cr4);
        __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
        klog_trace("[SYSCALL] CR4 write completed\n");
    }

    klog_trace("[SYSCALL] SCE bit enabled in CR4\n");

    /* Step 6: Configure MSR registers */
    extern void syscall_handler(void);
    uint64_t lstar = (uint64_t)syscall_handler;

    /* Configure IA32_LSTAR (0xC0000082) - syscall entry point */
    klog_trace("[SYSCALL] About to write IA32_LSTAR...\n");
    wrmsr(0xC0000082, lstar);
    klog_trace("[SYSCALL] IA32_LSTAR = 0x%016llX\n", lstar);

    /* Configure IA32_STAR (0xC0000081)
     * STAR[63:48] = SYSRET CS (user mode code) = GDT_USER_CODE | 3 = 0x18 | 3 = 0x1B
     * STAR[47:32] = syscall CS (kernel mode code) = GDT_KERNEL_CODE = 0x08
     */
    klog_trace("[SYSCALL] About to write IA32_STAR...\n");
    uint64_t star = ((uint64_t)(GDT_USER_CODE | 3) << 48) |
                    ((uint64_t)GDT_KERNEL_CODE << 32);
    wrmsr(0xC0000081, star);
    klog_trace("[SYSCALL] IA32_STAR = 0x%016llX\n", star);

    /* Configure IA32_FMASK (0xC0000084) - RFLAGS bits to clear on syscall */
    klog_trace("[SYSCALL] About to write IA32_FMASK...\n");
    wrmsr(0xC0000084, SYSCALL_FMASK_DEFAULT);
    klog_trace("[SYSCALL] IA32_FMASK = 0x%08X\n", SYSCALL_FMASK_DEFAULT);

    /* Step 7: Register int 0x80 as fallback (vector 128, after IRQ range) */
    extern void int0x80_handler(void);
    idt_set_gate(128, (uint64_t)int0x80_handler,
                 IDT_USER_INTERRUPT_GATE, GDT_KERNEL_CODE);
    klog_trace("[SYSCALL] int 0x80 fallback registered at vector 128\n");

    s_initialized = true;
    klog_info("[SYSCALL] System call framework initialized\n");
    klog_info("[SYSCALL]   syscall/sysret: enabled\n");
    klog_info("[SYSCALL]   int 0x80 fallback: vector 128\n");
}
```

这个函数做了七件事：检测 CPU 支持、清空系统调用表、注册默认处理函数、注册所有系统调用、启用 SCE 位、配置 MSR 寄存器、注册 int 0x80 处理器。

你可能注意到有很多 `klog_trace` 调用。这是有意的，MSR 配置是关键步骤，详细的日志可以帮助我们在出问题时快速定位。

---

## GDT 段选择子的问题

代码中用到了 `GDT_KERNEL_CODE` 和 `GDT_USER_CODE`，这些常量应该在你的 `interrupt/gdt.h` 中定义。让我们确认一下这些值：

```c
/* GDT segment selectors */
#define GDT_NULL        0x00
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
```

这些值是你 GDT 中各个段的索引乘以 8。如果你的 GDT 定义不同，需要相应调整代码。

重点注意 `GDT_USER_CODE | 3` 这一行。这里的 `| 3` 是设置 RPL（Requestor Privilege Level）为 3，表示用户态。如果你忘记这个，sysretq 返回时会因为特权级不匹配而触发异常。

---

## 配置顺序的重要性

让我再强调一次配置顺序，因为这是最容易出错的地方：

**正确顺序**：
1. 启用 CR4 的 SCE 位
2. 配置 IA32_LSTAR
3. 配置 IA32_STAR
4. 配置 IA32_FMASK

**错误顺序**（会触发 GP 异常）：
1. 配置 MSR 寄存器
2. 启用 CR4 的 SCE 位

原因是：当 SCE 位为 0 时，MSR 寄存器被认为是"禁用"的，对它们写入会触发异常。只有在启用 SCE 位之后，才能安全地配置这些 MSR。

---

## 验证 MSR 配置

配置完成后，我们可以通过读取 MSR 来验证配置是否正确。在 `syscall_init()` 之后添加一个简单的测试：

```c
/* Verify MSR configuration (for debugging) */
uint64_t lstar_verify = rdmsr(0xC0000082);
uint64_t star_verify = rdmsr(0xC0000081);
uint64_t fmask_verify = rdmsr(0xC0000084);

klog_info("[SYSCALL] MSR verification:\n");
klog_info("[SYSCALL]   IA32_LSTAR = 0x%016llX (expected: 0x%016llX)\n",
          lstar_verify, lstar);
klog_info("[SYSCALL]   IA32_STAR  = 0x%016llX\n", star_verify);
klog_info("[SYSCALL]   IA32_FMASK = 0x%016llX\n", fmask_verify);

/* Check STAR values */
uint32_t kernel_cs = (star_verify >> 32) & 0xFFFF;
uint32_t user_cs = (star_verify >> 48) & 0xFFFF;
klog_info("[SYSCALL]   Kernel CS = 0x%04X (expected: 0x%04X)\n",
          kernel_cs, GDT_KERNEL_CODE);
klog_info("[SYSCALL]   User CS   = 0x%04X (expected: 0x%04X)\n",
          user_cs, GDT_USER_CODE | 3);
```

这段代码会读取并打印 MSR 寄存器的值，方便我们在调试时确认配置是否正确。

---

## 在内核初始化时调用

现在我们需要在内核初始化时调用 `syscall_init()`。打开 `kernel/kernel_init.c`，在适当的位置添加：

```c
#include "syscall/syscall.h"

void kernel_init(void) {
    /* ... other initializations ... */

    /* Initialize system call framework (after GDT/TSS) */
    klog_info("[INIT] Initializing system call framework...\n");
    syscall_init();

    /* ... other initializations ... */
}
```

重要：`syscall_init()` 必须在 GDT 和 TSS 初始化之后调用，因为我们需要 GDT 段选择子的值。如果你的初始化顺序不同，需要相应调整。

---

## 编译测试

现在让我们编译并测试一下。从项目根目录运行：

```bash
cd build
make
```

如果一切正常，你应该看到编译成功。现在运行 QEMU：

```bash
make run
```

你应该看到类似的串口输出：

```
[INIT] Initializing system call framework...
[SYSCALL] Initializing system call framework...
[SYSCALL] syscall/sysret supported
[SYSCALL] About to enable SCE bit in CR4...
[SYSCALL] Current CR4 = 0x0000000000060680
[SYSCALL] Setting CR4 to 0x0000000000062680 (before write)
[SYSCALL] CR4 write completed
[SYSCALL] SCE bit enabled in CR4
[SYSCALL] About to write IA32_LSTAR...
[SYSCALL] IA32_LSTAR = 0x0000000000101234
[SYSCALL] About to write IA32_STAR...
[SYSCALL] IA32_STAR = 0x001B000000000008
[SYSCALL] About to write IA32_FMASK...
[SYSCALL] IA32_FMASK = 0x00000200
[SYSCALL] int 0x80 fallback registered at vector 128
[SYSCALL] System call framework initialized
[SYSCALL]   syscall/sysret: enabled
[SYSCALL]   int 0x80 fallback: vector 128
```

注意 CR4 的值：启用 SCE 位后，第 11 位应该被设置。你可以用计算器验证一下：0x60680 | (1<<11) = 0x62680。

---

## 常见问题排查

如果遇到问题，这里有一些常见的坑：

**问题1：触发 General Protection 异常**

可能原因：
- CR4 的 SCE 位没有启用就配置 MSR
- IA32_STAR 的值配置错误（忘记 | 3）
- GDT 段选择子值不正确

解决方法：检查配置顺序，确认 GDT 常量值，验证 STAR 的 kernel_cs 和 user_cs 字段。

**问题2：syscall 不生效**

可能原因：
- 汇编入口 `syscall_handler` 没有正确导出
- IA32_LSTAR 的值不正确

解决方法：确认 `syscall_handler` 在 `syscall.asm` 中用 `global` 导出，用 `extern` 在 C 代码中声明。

**问题3：编译错误，找不到 GDT 常量**

可能原因：
- `interrupt/gdt.h` 没有包含
- GDT 常量定义在不同的文件中

解决方法：检查 `syscall.c` 的 include 语句，确认 GDT 头文件路径正确。

---

## 接下来

到这里我们已经完成了 MSR 寄存器的配置，syscall 指令的"基础设施"已经就绪。但如果你现在尝试执行 syscall 指令，系统还是会崩溃，因为我们还没有实现汇编入口 `syscall_handler`。

在下一篇文章中，我们会实现完整的汇编入口代码，包括寄存器保存、栈对齐、构建系统调用帧，以及调用 C 分发器。这部分代码是系统调用的"胶水"，连接用户态和内核态。

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 搭建syscall脚手架](03_搭建syscall脚手架.md)  | [汇编入口与栈对齐 →](05_汇编入口与栈对齐.md)

</div>
