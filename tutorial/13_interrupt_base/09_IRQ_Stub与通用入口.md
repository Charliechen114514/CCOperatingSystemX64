# IRQ Stub 与通用入口 —— Stage 13 中断基础实战指南

## 前言

上一篇文章我们实现了异常 ISR stub，现在轮到 IRQ stub 了。IRQ stub 的结构与 ISR 类似，但有一点不同 —— 所有 IRQ 都没有错误码。我们还需要实现 `interrupt_common` 通用入口，这是所有中断的汇合点。

---

## 第一步：实现 IRQ Stub 宏

IRQ stub 比 ISR 简单，因为所有 IRQ 都不带错误码：

```asm
; 宏：IRQ stub
%macro IRQ 2
  global irq%1
  irq%1:
    push qword 0         ; 虚拟错误码
    push qword %2        ; 中断向量号
    push qword 0         ; 对齐 dummy
    jmp interrupt_common
%endmacro
```

这个宏接收两个参数：
1. IRQ 号（0-15）
2. 中断向量号（32-47）

---

## 第二步：生成所有 16 个 IRQ

```asm
section .text

; IRQ 0-15，映射到向量 32-47
IRQ 0,  32    ; Timer
IRQ 1,  33    ; Keyboard
IRQ 2,  34    ; Cascade
IRQ 3,  35    ; COM2
IRQ 4,  36    ; COM1
IRQ 5,  37    ; LPT2
IRQ 6,  38    ; Floppy
IRQ 7,  39    ; LPT1
IRQ 8,  40    ; RTC
IRQ 9,  41    ; Free
IRQ 10, 42    ; Free
IRQ 11, 43    ; Free
IRQ 12, 44    ; PS/2 Mouse
IRQ 13, 45    ; FPU
IRQ 14, 46    ; Primary ATA
IRQ 15, 47    ; Secondary ATA
```

注意 IRQ 2 是级联用的，不会产生实际中断，但我们还是生成 stub 以防万一。

---

## 第三步：实现 interrupt_common 入口

这是所有中断的汇合点，负责保存 CPU 状态并调用 C 处理函数。

```asm
; ============================================================================
; Common Interrupt Handler
; ============================================================================

extern interrupt_handler  ; C 函数

interrupt_common:
    ; --- 保存通用寄存器 ---
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; --- 保存段寄存器 ---
    mov ax, ds
    push rax
    mov ax, es
    push rax
    mov ax, fs
    push rax
    mov ax, gs
    push rax

    ; --- 加载内核数据段 ---
    mov ax, 0x10  ; 内核数据段选择子
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; --- 准备 C 函数参数 ---
    ; void interrupt_handler(uint64_t vector, uint64_t error_code, interrupt_frame_t* frame)
    ;
    ; 栈布局（此时 RSP 指向 GS）：
    ;   [RSP + 152] = error_code / dummy
    ;   [RSP + 160] = vector_number
    ;   [RSP + 168] = alignment dummy
    ;   [RSP + 176] = RIP (CPU push)
    ;   [RSP + 184] = CS (CPU push)
    ;   [RSP + 192] = RFLAGS (CPU push)
    ;   [RSP + 200] = RSP (CPU push)
    ;   [RSP + 208] = SS (CPU push)

    mov rdi, [rsp + 160]  ; RDI = vector
    mov rsi, [rsp + 152]  ; RSI = error_code
    lea rdx, [rsp + 152]  ; RDX = pointer to frame

    ; --- 调用 C 处理函数 ---
    cld                     ; 清除方向标志
    call interrupt_handler

    ; --- 恢复段寄存器 ---
    pop rax
    mov gs, ax
    pop rax
    mov fs, ax
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    ; --- 恢复通用寄存器 ---
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; --- 清理栈 ---
    add rsp, 24  ; 移除 error_code, vector, alignment dummy

    ; --- 中断返回 ---
    iretq
```

---

## 第四步：导出处理函数表

为了方便 C 代码使用，我们导出处理函数地址数组：

```asm
section .rodata

; ISR 处理函数表
global isr_handler_table
isr_handler_table:
    %assign i 0
    %rep 32
        dq isr %+ i
        %assign i i+1
    %endrep

; IRQ 处理函数表
global irq_handler_table
irq_handler_table:
    %assign i 0
    %rep 16
        dq irq %+ i
        %assign i i+1
    %endrep
```

这样 C 代码可以用循环来设置 IDT，而不需要一个一个手动设置。

---

## 第五步：更新 CMake 添加汇编文件

修改 `kernel/interrupt/CMakeLists.txt`：

```cmake
add_library(interrupt STATIC
    idt.c
)

# 添加汇编文件
target_sources(interrupt PRIVATE
    interrupt.asm
)

# 设置汇编编译选项
set_target_properties(interrupt PROPERTIES
    LINKER_LANGUAGE C
)
```

---

## 到这里我们完成了什么

这篇文章我们实现了：

- 16 个 IRQ stub
- `interrupt_common` 通用入口
- 完整的寄存器保存/恢复
- 处理函数表导出

下一篇文章我们会实现 C 语言的中断分发器。

---

## 接下来

在下一篇文章中，我们会：
1. 实现异常显示函数
2. 实现 IRQ 路由逻辑
3. 实现定时器中断处理
4. 完成中断分发器

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 汇编Stub入门与踩坑](08_汇编Stub入门与踩坑.md)  | [C语言中断分发器 →](10_C语言中断分发器.md)

</div>
