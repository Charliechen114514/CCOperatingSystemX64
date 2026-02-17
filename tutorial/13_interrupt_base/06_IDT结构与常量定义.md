# IDT 结构与常量定义 —— Stage 13 中断基础实战指南

## 前言

上一篇文章我们完成了 PIC 驱动的实现，现在硬件中断控制器已经搞定了。接下来我们需要处理 CPU 那边的事情 —— IDT（中断描述符表）。

IDT 是 CPU 用来查找中断处理函数的数据结构。每个中断向量对应一个 IDT 条目，条目里包含处理函数的地址、特权级、类型等信息。这篇文章我们会定义 IDT 相关的数据结构和常量。

---

## 第一步：创建中断子系统目录

先创建目录结构：

```bash
mkdir -p kernel/interrupt
tree kernel/interrupt -L 1
```

---

## 第二步：定义 CPU 异常常量

创建 `kernel/interrupt/idt_constants.h`，首先定义 CPU 异常向量：

```c
/**
 * @file idt_constants.h
 * @brief IDT 常量定义
 * @date 2026-02-17
 */

#pragma once

#define IDT_ENTRIES 256

/* ============================================================================
 * CPU Exception Vectors (0-31)
 * ============================================================================ */

#define IDT_DE   0  // Divide Error
#define IDT_DB   1  // Debug
#define IDT_NMI  2  // Non-Maskable Interrupt
#define IDT_BP   3  // Breakpoint
#define IDT_OF   4  // Overflow
#define IDT_BR   5  // BOUND Range Exceeded
#define IDT_UD   6  // Invalid Opcode
#define IDT_NM   7  // Device Not Available
#define IDT_DF   8  // Double Fault
#define IDT_CSO  9  // Coprocessor Segment Overrun
#define IDT_TS  10  // Invalid TSS
#define IDT_NP  11  // Segment Not Present
#define IDT_SS  12  // Stack-Segment Fault
#define IDT_GP  13  // General Protection Fault
#define IDT_PF  14  // Page Fault
#define IDT_MF  15  // x87 FPU Error
#define IDT_AC  16  // Alignment Check
#define IDT_MC  17  // Machine Check
#define IDT_XM  18  // SIMD FP Exception
#define IDT_VE  19  // Virtualization Exception
#define IDT_CP  20  // Control Protection
// 21-31 Reserved
```

这些常量对应 CPU 的 32 个异常向量。虽然我们不会全部用到，但定义出来便于代码阅读。

---

## 第三步：定义 IRQ 向量常量

继续在 `idt_constants.h` 中添加 IRQ 向量定义：

```c
/* ============================================================================
 * IRQ Vectors (32-47) - After PIC Remapping
 * ============================================================================ */

#define IDT_IRQ_BASE 32

#define IDT_IRQ0  32  // Timer (PIC1)
#define IDT_IRQ1  33  // Keyboard
#define IDT_IRQ2  34  // Cascade (internal)
#define IDT_IRQ3  35  // COM2
#define IDT_IRQ4  36  // COM1
#define IDT_IRQ5  37  // LPT2
#define IDT_IRQ6  38  // Floppy
#define IDT_IRQ7  39  // LPT1
#define IDT_IRQ8  40  // RTC (PIC2)
#define IDT_IRQ9  41  // Free
#define IDT_IRQ10 42  // Free
#define IDT_IRQ11 43  // Free
#define IDT_IRQ12 44  // PS/2 Mouse
#define IDT_IRQ13 45  // FPU
#define IDT_IRQ14 46  // Primary ATA
#define IDT_IRQ15 47  // Secondary ATA
```

---

## 第四步：定义 IDT 门类型属性

继续添加门类型和属性的常量：

```c
/* ============================================================================
 * IDT Type Attributes
 * ============================================================================ */

// Type field values (bits 0-3)
#define IDT_TYPE_TASK_GATE       0x5
#define IDT_TYPE_INTERRUPT_GATE  0xE
#define IDT_TYPE_TRAP_GATE       0xF

// Storage segment bit (bit 4)
#define IDT_STORAGE_SEGMENT 0x0

// Descriptor Privilege Level (DPL, bits 5-6)
#define IDT_DPL_KERNEL 0x0
#define IDT_DPL_USER   0x3

// Present bit (bit 7)
#define IDT_PRESENT     0x80
#define IDT_NOT_PRESENT 0x00

/* ============================================================================
 * Common Attribute Macros
 * ============================================================================ */

#define IDT_KERNEL_INTERRUPT_GATE \
    (IDT_PRESENT | IDT_DPL_KERNEL | IDT_STORAGE_SEGMENT | IDT_TYPE_INTERRUPT_GATE)

#define IDT_USER_INTERRUPT_GATE \
    (IDT_PRESENT | IDT_DPL_USER | IDT_STORAGE_SEGMENT | IDT_TYPE_INTERRUPT_GATE)

#define IDT_KERNEL_TRAP_GATE \
    (IDT_PRESENT | IDT_DPL_KERNEL | IDT_STORAGE_SEGMENT | IDT_TYPE_TRAP_GATE)
```

这些宏用于设置 IDT 条目的类型属性字节。最常用的是 `IDT_KERNEL_INTERRUPT_GATE`（值为 0x8E）。

---

## 第五步：定义中断帧结构体

现在我们需要定义 CPU 推送的栈帧结构。在 `idt_constants.h` 中添加：

```c
#include "defines/types.h"

/* ============================================================================
 * Interrupt Stack Frame
 * ============================================================================ */

/**
 * @brief Stack frame pushed by x86_64 CPU on interrupt/exception
 *
 * This is the exact layout pushed by CPU:
 * - If CPL changes: SS, RSP, RFLAGS, CS, RIP, Error Code
 * - If CPL doesn't change: RFLAGS, CS, RIP, Error Code (some)
 */
typedef struct PACKED {
    uint64_t error_code;  // Error code (pushed only for some exceptions)
    uint64_t rip;         // Instruction pointer
    uint64_t cs;          // Code segment
    uint64_t rflags;      // RFLAGS register
    uint64_t rsp;         // Stack pointer
    uint64_t ss;          // Stack segment
} interrupt_frame_t;
```

---

## 第六步：定义 IDT 条目结构体

继续在 `idt_constants.h` 中添加：

```c
/* ============================================================================
 * IDT Entry Structure
 * ============================================================================ */

/**
 * @brief IDT Entry (Gate Descriptor)
 *
 * x86_64 IDT entries are 16 bytes each:
 * Bytes 0-1:   Offset Low  (handler bits 0-15)
 * Bytes 2-3:   Segment Selector
 * Byte 4:      IST
 * Byte 5:      Type Attributes
 * Bytes 6-7:   Offset Middle (handler bits 16-31)
 * Bytes 8-11:  Offset High (handler bits 32-63)
 * Bytes 12-15: Reserved
 */
typedef struct PACKED {
    uint16_t offset_low;       // Handler address bits 0-15
    uint16_t segment_selector; // Code segment selector
    uint8_t  ist;              // Interrupt Stack Table offset
    uint8_t  type_attr;        // Type attributes
    uint16_t offset_middle;    // Handler address bits 16-31
    uint32_t offset_high;      // Handler address bits 32-63
    uint32_t reserved;         // Must be 0
} idt_entry_t;

/* ============================================================================
 * IDT Pointer Structure
 * ============================================================================ */

/**
 * @brief IDT Pointer for lidt instruction
 */
typedef struct PACKED {
    uint16_t limit;  // Size of IDT - 1
    uint64_t base;   // Base address of IDT
} idt_ptr_t;
```

---

## 第七步：定义处理函数类型

继续添加函数指针类型：

```c
/* ============================================================================
 * Interrupt Handler Function Types
 * ============================================================================ */

/**
 * @brief Type for interrupt/exception handler functions
 *
 * @param frame Pointer to interrupt stack frame
 */
typedef void (*interrupt_handler_fn)(interrupt_frame_t* frame);
```

---

## 第八步：创建 idt.h 头文件

现在创建 `kernel/interrupt/idt.h`，定义公共接口：

```c
/**
 * @file idt.h
 * @brief IDT 管理接口
 * @date 2026-02-17
 */

#pragma once

#include "idt_constants.h"

/* ============================================================================
 * IDT Management Functions
 * ============================================================================ */

/**
 * @brief Initialize the IDT
 *
 * Sets up all IDT entries and loads it into CPU.
 */
void idt_init(void);

/**
 * @brief Set an IDT entry
 *
 * @param vector Interrupt vector (0-255)
 * @param handler Handler function address
 * @param type_attr Type attributes
 * @param segment_selector Code segment selector
 */
void idt_set_gate(uint8_t vector, uint64_t handler,
                  uint8_t type_attr, uint16_t segment_selector);

/**
 * @brief Register a custom interrupt handler
 *
 * @param vector Interrupt vector
 * @param handler Handler function
 */
void idt_register_handler(uint8_t vector, interrupt_handler_fn handler);

/**
 * @brief Get exception name
 *
 * @param vector Exception vector
 * @return Exception name string
 */
const char* idt_get_exception_name(uint8_t vector);

/* ============================================================================
 * Architecture Functions (implemented in assembly)
 * ============================================================================ */

/**
 * @brief Load IDT (lidt instruction)
 *
 * @param idt_ptr Pointer to idt_ptr_t
 */
void idt_load(idt_ptr_t* idt_ptr);
```

---

## 到这里我们完成了什么

这篇文章我们定义了 IDT 相关的所有数据结构和常量：

- CPU 异常向量常量
- IRQ 向量常量
- IDT 门类型属性
- 中断帧结构体
- IDT 条目结构体
- IDT 指针结构体
- 公共接口声明

下一篇文章我们会实现 IDT 的初始化和加载功能。

---

## 接下来

在下一篇文章中，我们会：
1. 实现 `idt_init()` 函数
2. 实现异常名称查找
3. 实现 `idt_load()` 汇编函数
4. 配置 CMake 构建

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← PIC的EOI与屏蔽操作](05_PIC的EOI与屏蔽操作.md)  | [IDT初始化与加载 →](07_IDT初始化与加载.md)

</div>
