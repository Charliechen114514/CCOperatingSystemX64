# 汇编 Stub 入门与踩坑 —— Stage 13 中断基础实战指南

## 前言

前面几篇文章我们实现了 PIC 驱动和 IDT 管理，现在到了整个中断系统最关键的部分：汇编 stub。这是 CPU 中断发生后的第一站，负责保存 CPU 状态、准备调用 C 语言处理函数。

说实话，这部分代码是我调试时间最长的。不是因为它多复杂，而是因为有一个非常隐蔽的坑 —— **栈对齐问题**。这个问题会导致各种奇怪的异常，而且很难定位。这篇文章我会详细讲解这个问题，以及我们是如何发现并修复的。

---

## 第一步：理解汇编 Stub 的作用

当中断发生时，CPU 会：
1. 自动推送一些信息到栈上（SS, RSP, RFLAGS, CS, RIP）
2. 跳转到 IDT 条目指定的地址

但 CPU 不会自动保存通用寄存器（RAX, RBX, ...）。这部分工作需要我们的汇编 stub 来完成。

```
CPU 推送的栈帧：
    +------------------+
    |   SS (可选)      |
    +------------------+
    |   RSP (可选)     |
    +------------------+
    |   RFLAGS         │
    +------------------+
    |   CS             │
    +------------------+
    |   RIP            │
    +------------------+
    |   Error Code     │  (部分异常)
    +------------------+  ← RSP 指向这里

我们需要做的：
1. 推送额外的信息（中断向量号、对齐 dummy）
2. 保存所有通用寄存器
3. 调用 C 处理函数
4. 恢复所有寄存器
5. 使用 iretq 返回
```

---

## 第二步：编写第一个 ISR Stub

我们使用 NASM 宏来生成 32 个 ISR stub。先写一个基础的版本：

在 `interrupt.asm` 中：

```asm
; 宏：不带错误码的 ISR
%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    push qword 0         ; 虚拟错误码（CPU 没有推送）
    push qword %1        ; 中断向量号
    jmp interrupt_common
%endmacro

; 生成 32 个异常 stub
%assign i 0
%rep 32
  ISR_NOERRCODE i
%assign i i+1
%endrep
```

这个宏接收一个参数（中断向量号），生成对应的 stub 函数。`%rep` 是重复指令，会生成 32 个函数（isr0 到 isr31）。

---

## 第三步：第一次测试 —— 发现问题

现在我们来编译测试一下。在设置好 IDT 后启用中断：

```c
void kernel_init(void) {
    // ... 初始化 PIC 和 IDT ...

    interrupt_enable();  // 启用中断

    // 主循环
    while (1) {
        __asm__ volatile("hlt");
    }
}
```

编译运行，然后观察串口输出：

```
[ERROR] === EXCEPTION OCCURRED ===
[ERROR] Vector: 0 - Divide Error (#DE)
[ERROR] RIP: 0x00000000000100a1
...
```

**等等，我们没有除零操作，为什么会触发 Divide Error？**

而且这个 RIP 地址也不是我们的中断处理函数。这说明我们的中断 stub 有问题。

---

## 第四步：分析问题 —— 栈对齐

我花了很长时间才定位到问题所在。让我详细解释一下。

### x86-64 ABI 栈对齐要求

x86-64 System V ABI 规定：
> 当调用 C 函数时，栈指针（RSP）必须 16 字节对齐。

这个要求是为了保证 SSE/AVX 指令能正确访问内存。

### 我们的栈布局分析

让我们计算一下进入 `interrupt_common` 时的栈偏移：

```
ISR stub 推送：
    push qword 0         ; 8 字节
    push qword %1        ; 8 字节
    总共：16 字节

CPU 自动推送（内核态）：
    RIP                  ; 8 字节
    CS                   ; 8 字节
    RFLAGS               ; 8 字节
    总共：24 字节

总计：16 + 24 = 40 字节

40 % 16 = 8  ← 没有对齐！
```

当我们调用 C 函数时，`call` 指令会再推送 8 字节的返回地址：

```
40 + 8 = 48

48 % 16 = 0  ← 现在对齐了？
```

等等，48 确实是 16 的倍数。但问题在于，C 函数内部可能会调用其他函数，而那些函数的 `call` 会破坏对齐。

更严重的问题是，GCC 生成的代码假设栈在入口时已经 16 字节对齐，如果不对齐，编译器生成的 SSE 指令可能会访问错误的内存地址。

---

## 第五步：修复 —— 添加对齐 Dummy

解决方案是在 ISR stub 中额外推送 8 字节：

```asm
; 修复后的版本
%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    push qword 0         ; 虚拟错误码（CPU 没有推送）
    push qword %1        ; 中断向量号
    push qword 0         ; ← 添加：对齐 dummy
    jmp interrupt_common
%endmacro
```

现在重新计算栈偏移：

```
ISR stub 推送：
    push qword 0         ; 8 字节
    push qword %1        ; 8 字节
    push qword 0         ; 8 字节（新增）
    总共：24 字节

CPU 自动推送：24 字节

总计：24 + 24 = 48 字节

48 % 16 = 0  ← 对齐了！
```

---

## 第六步：处理带错误码的异常

某些异常（如 #PF, #GP）会自动推送错误码。对于这些异常，CPU 已经推送了错误码，我们不需要再推送虚拟的 0：

```asm
; 宏：带错误码的 ISR
%macro ISR_ERRCODE 1
  global isr%1
  isr%1:
    ; CPU 已经推送了错误码，只推送向量号
    push qword %1        ; 中断向量号
    push qword 0         ; 对齐 dummy
    jmp interrupt_common
%endmacro
```

需要错误码的异常：8, 10, 11, 12, 13, 14, 16, 17, 20

---

## 第七步：生成所有 32 个 ISR

现在我们用正确的宏生成所有 32 个异常 stub：

```asm
section .text

; 不带错误码的异常
ISR_NOERRCODE 0   ; Divide Error
ISR_NOERRCODE 1   ; Debug
ISR_NOERRCODE 2   ; NMI
ISR_NOERRCODE 3   ; Breakpoint
ISR_NOERRCODE 4   ; Overflow
ISR_NOERRCODE 5   ; BOUND
ISR_NOERRCODE 6   ; Invalid Opcode
ISR_NOERRCODE 7   ; Device Not Available

; 带错误码的异常
ISR_ERRCODE   8   ; Double Fault

ISR_NOERRCODE 9   ; Coprocessor Segment Overrun

ISR_ERRCODE   10  ; Invalid TSS
ISR_ERRCODE   11  ; Segment Not Present
ISR_ERRCODE   12  ; Stack Fault
ISR_ERRCODE   13  ; General Protection
ISR_ERRCODE   14  ; Page Fault

ISR_NOERRCODE 15  ; x87 FPU Error
ISR_NOERRCODE 16  ; Alignment Check
ISR_NOERRCODE 17  ; Machine Check
ISR_NOERRCODE 18  ; SIMD FP Exception
ISR_NOERRCODE 19  ; Virtualization Exception
ISR_NOERRCODE 20  ; Control Protection
ISR_NOERRCODE 21  ; Reserved
ISR_NOERRCODE 22  ; Reserved
ISR_NOERRCODE 23  ; Reserved
ISR_NOERRCODE 24  ; Reserved
ISR_NOERRCODE 25  ; Reserved
ISR_NOERRCODE 26  ; Reserved
ISR_NOERRCODE 27  ; Reserved
ISR_NOERRCODE 28  ; Reserved
ISR_NOERRCODE 29  ; SSE Exception
ISR_NOERRCODE 30  ; Reserved
ISR_NOERRCODE 31  ; Reserved
```

---

## 第八步：验证修复

重新编译运行，这次应该不会触发 Divide Error 了。如果我们的中断处理程序正确，应该能看到定时器中断的输出。

```
[INFO] Interrupts enabled
[INFO] [TIMER] Active! Ticks: 100
[INFO] [TIMER] Active! Ticks: 200
```

---

## 踩坑总结

栈对齐问题是一个非常隐蔽但致命的 bug。如果你遇到：

1. 莫名其妙的异常（如 Divide Error）
2. 异常发生在循环内部
3. RIP 指向的指令看起来没有问题

那很可能是栈对齐问题。解决方法：
1. 检查 ISR stub 是否推送了对齐 dummy
2. 计算栈偏移确保 16 字节对齐
3. 用 GDB 检查 RSP 的值

---

## 到这里我们完成了什么

这篇文章我们实现了异常 ISR stub：

- 理解了汇编 stub 的作用
- 实现了 ISR 宏定义
- **发现并修复了栈对齐问题**
- 生成了所有 32 个异常 stub

下一篇文章我们会实现 IRQ stub 和通用入口。

---

## 接下来

在下一篇文章中，我们会：
1. 实现 16 个 IRQ stub
2. 实现 `interrupt_common` 通用入口
3. 实现完整的寄存器保存/恢复
4. 导出处理函数表

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← IDT初始化与加载](07_IDT初始化与加载.md)  | [IRQ Stub与通用入口 →](09_IRQ_Stub与通用入口.md)

</div>
