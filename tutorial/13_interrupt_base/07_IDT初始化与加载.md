# IDT 初始化与加载 —— Stage 13 中断基础实战指南

## 前言

上一篇文章我们定义了 IDT 的数据结构和常量，现在到了真正实现 IDT 初始化的时候了。说实话，这部分代码虽然不难，但涉及很多细节。我们需要设置 256 个 IDT 条目，为每个 CPU 异常配置默认处理程序，为每个 IRQ 配置占位处理程序，最后用 `lidt` 指令加载到 CPU。

---

## 第一步：创建 IDT 数组

首先我们需要定义 IDT 数组。在 `kernel/interrupt/idt.c` 中：

```c
/**
 * @file idt.c
 * @brief IDT 实现
 * @date 2026-02-17
 */

#include "idt.h"
#include "idt_constants.h"
#include "klogs/kprintf.h"

/* ============================================================================
 * IDT Array
 * ============================================================================ */

// 256 个 IDT 条目（对齐到 16 字节边界）
static idt_entry_t g_idt[IDT_ENTRIES] __attribute__((aligned(16)));
```

`__attribute__((aligned(16))` 确保 IDT 数组 16 字节对齐，这是 x86_64 的要求。

---

## 第二步：实现 idt_set_gate() 函数

这个函数设置单个 IDT 条目：

```c
void idt_set_gate(uint8_t vector, uint64_t handler,
                  uint8_t type_attr, uint16_t segment_selector) {
    if (vector >= IDT_ENTRIES) {
        return;  // 无效向量
    }

    g_idt[vector].offset_low       = handler & 0xFFFF;
    g_idt[vector].offset_middle    = (handler >> 16) & 0xFFFF;
    g_idt[vector].offset_high      = (handler >> 32) & 0xFFFFFFFF;
    g_idt[vector].segment_selector = segment_selector;
    g_idt[vector].ist              = 0;
    g_idt[vector].type_attr        = type_attr;
    g_idt[vector].reserved         = 0;
}
```

这里用位操作把 64 位地址拆分成三部分：
- 低 16 位
- 中 16 位
- 高 32 位

---

## 第三步：实现异常名称查找

创建一个异常名称查找表：

```c
static const char* g_exception_names[] = {
    "Divide Error (#DE)",              // 0
    "Debug (#DB)",                     // 1
    "Non-Maskable Interrupt",          // 2
    "Breakpoint (#BP)",                // 3
    "Overflow (#OF)",                  // 4
    "BOUND Range Exceeded (#BR)",      // 5
    "Invalid Opcode (#UD)",            // 6
    "Device Not Available (#NM)",      // 7
    "Double Fault (#DF)",              // 8
    "Coprocessor Segment Overrun",     // 9
    "Invalid TSS (#TS)",               // 10
    "Segment Not Present (#NP)",       // 11
    "Stack-Segment Fault (#SS)",       // 12
    "General Protection Fault (#GP)",  // 13
    "Page Fault (#PF)",                // 14
    "x87 FPU Error (#MF)",             // 15
    "Alignment Check (#AC)",           // 16
    "Machine Check (#MC)",             // 17
    "SIMD FP Exception (#XM)",         // 18
    "Virtualization Exception (#VE)",  // 19
    "Control Protection (#CP)",        // 20
    "Reserved",                        // 21
    "Reserved",                        // 22
    "Reserved",                        // 23
    "Reserved",                        // 24
    "Reserved",                        // 25
    "Reserved",                        // 26
    "Reserved",                        // 27
    "Reserved",                        // 28
    "SSE Exception (#XF)",             // 29
    "Reserved",                        // 30
    "Reserved",                        // 31
};

const char* idt_get_exception_name(uint8_t vector) {
    if (vector < 32) {
        return g_exception_names[vector];
    }
    return "Unknown";
}
```

---

## 第四步：声明汇编处理函数

我们的汇编 stub 会导出一组处理函数，需要在这里声明：

```c
/* ============================================================================
 * Assembly ISR/IRQ Stubs (defined in interrupt.asm)
 * ============================================================================ */

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
// ... isr3 - isr31 ...

extern void irq0(void);   // 即 isr32
extern void irq1(void);   // 即 isr33
// ... irq2 - irq15 ...
```

但这些函数太多了，手动声明太麻烦。我们可以在 `idt.h` 中定义一个宏来自动生成。

---

## 第五步：实现 idt_init() 函数

现在实现完整的 IDT 初始化：

```c
void idt_init(void) {
    klog_trace("Initializing IDT...\n");

    // 清空 IDT
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // 设置 CPU 异常处理程序（向量 0-31）
    // 这里假设我们有 isr0-isr31 的汇编 stub
    for (int i = 0; i < 32; i++) {
        // TODO: 设置异常处理函数
        // idt_set_gate(i, (uint64_t)isrX, IDT_KERNEL_INTERRUPT_GATE, 0x08);
    }

    // 设置 IRQ 处理程序（向量 32-47）
    // 这里假设我们有 irq0-irq15 的汇编 stub
    for (int i = 0; i < 16; i++) {
        // TODO: 设置 IRQ 处理函数
        // idt_set_gate(IDT_IRQ_BASE + i, (uint64_t)irqX, IDT_KERNEL_INTERRUPT_GATE, 0x08);
    }

    // 加载 IDT
    idt_ptr_t idt_ptr = {
        .limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1,
        .base = (uint64_t)g_idt
    };

    idt_load(&idt_ptr);

    klog_trace("IDT initialized and loaded\n");
}
```

---

## 第六步：实现 idt_load() 汇编函数

创建 `kernel/interrupt/interrupt.asm`，添加 `idt_load` 函数：

```asm
; interrupt.asm
section .text

; void idt_load(idt_ptr_t* idt_ptr)
; RDI contains pointer to idt_ptr_t
global idt_load
idt_load:
    lidt [rdi]    ; 加载 IDTR
    ret
```

`lidt` 指令加载 IDTR 寄存器，它告诉 CPU IDT 在哪里以及有多大。

---

## 第七步：配置 CMake

创建 `kernel/interrupt/CMakeLists.txt`：

```cmake
add_library(interrupt STATIC
    idt.c
    # interrupt.asm 会在后面添加
)

target_include_directories(interrupt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_options(interrupt PUBLIC ${CCOS_FREESTANDING_FLAGS})
```

还需要添加汇编文件支持（后面会详细讲解）。

---

## 到这里我们完成了什么

这篇文章我们实现了 IDT 的初始化和加载：

- 创建了 IDT 数组
- 实现了 `idt_set_gate()` 函数
- 实现了异常名称查找
- 实现了 `idt_init()` 函数
- 实现了 `idt_load()` 汇编函数

下一篇文章我们会实现汇编 stub，这是中断处理的入口。

---

## 接下来

在下一篇文章中，我们会：
1. 创建中断汇编 stub
2. 实现 ISR 宏定义
3. **重点：栈对齐问题的发现与修复**
4. 生成所有 48 个处理函数

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← IDT结构与常量定义](06_IDT结构与常量定义.md)  | [汇编Stub入门与踩坑 →](08_汇编Stub入门与踩坑.md)

</div>
