# 07 - 实现 GDT 和 TSS 管理

说实话，第一次成功加载自己的 GDT 时，我盯着屏幕看了好久——这么复杂的东西，居然真的能跑起来。

---

## 实现概述

本节我们将实现：
1. **GDT 管理**：创建内核 GDT，加载 GDT
2. **TSS 管理**：创建和初始化 TSS，配置 IST 栈
3. **汇编辅助函数**：使用汇编指令加载 GDT 和 TSS

---

## 创建 kernel/interrupt/gdt.h

首先创建 GDT 头文件：

```c
/* ==============================================================================
 * CCOS - Global Descriptor Table (GDT) for x86_64
 * ==============================================================================
 * This module provides GDT management including TSS loading.
 * In x86_64, most segmentation is disabled, but we still need GDT for:
 * - Code segment selectors (required by hardware)
 * - TSS segment for IST stacks
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"
```

### 数据结构定义

```c
/* ============================================================================
 * GDT Entry Structures
 * ============================================================================ */

/**
 * @brief GDT entry (normal segment descriptor)
 */
typedef struct PACKED {
    uint16_t limit_low;       /* Limit bits 0-15 */
    uint16_t base_low;        /* Base bits 0-15 */
    uint8_t  base_middle;     /* Base bits 16-23 */
    uint8_t  access;          /* Access byte */
    uint8_t  granularity;     /* Granularity flags */
    uint8_t  base_high;       /* Base bits 24-31 */
} __attribute__((packed)) gdt_entry_t;

/**
 * @brief GDT TSS entry (64-bit TSS descriptor)
 */
typedef struct PACKED {
    uint16_t limit_low;       /* Limit bits 0-15 */
    uint16_t base_low;        /* Base bits 0-15 */
    uint8_t  base_middle;     /* Base bits 16-23 */
    uint8_t  access;          /* Access byte (0x89 for available 64-bit TSS) */
    uint8_t  granularity;     /* Limit bits 16-19 and flags */
    uint8_t  base_high;       /* Base bits 24-31 */
    uint32_t base_upper;      /* Base bits 32-63 */
    uint32_t reserved;        /* Reserved, must be 0 */
} __attribute__((packed)) gdt_tss_entry_t;

/**
 * @brief GDT pointer structure (for lgdt instruction)
 */
typedef struct PACKED {
    uint16_t limit;           /* GDT size - 1 */
    uint64_t base;            /* GDT base address */
} __attribute__((packed)) gdt_ptr_t;
```

### 选择器值定义

```c
/* ============================================================================
 * GDT Selector Values
 * ============================================================================ */

#define GDT_NULL        0x00  /* Null descriptor */
#define GDT_KERNEL_CODE 0x08  /* Kernel 64-bit code */
#define GDT_KERNEL_DATA 0x10  /* Kernel data */
#define GDT_USER_CODE   0x18  /* User 64-bit code */
#define GDT_USER_DATA   0x20  /* User data */
#define GDT_TSS         0x28  /* TSS */
```

### 访问字节和标志位

```c
/* Access byte values */
#define GDT_ACCESS_PRESENT    (1 << 7)   /* Present bit */
#define GDT_ACCESS_DPL0       (0 << 5)   /* DPL 0 */
#define GDT_ACCESS_DPL3       (3 << 5)   /* DPL 3 */
#define GDT_ACCESS_SYSTEM     (1 << 4)   /* System flag (0 for system segments) */
#define GDT_ACCESS_TYPE_CODE  (0xA)      /* Code segment, execute/read */
#define GDT_ACCESS_TYPE_DATA  (0x2)      /* Data segment, read/write */
#define GDT_ACCESS_TYPE_TSS   (0x9)      /* 64-bit TSS (available) */

/* Granularity byte values */
#define GDT_GRANULARITY_4K    (1 << 7)   /* 4KB granularity */
#define GDT_GRANULARITY_32BIT (1 << 6)   /* 32-bit protected mode (ignored in long mode) */
#define GDT_GRANULARITY_64BIT (1 << 5)   /* 64-bit code segment */
```

### API 声明

```c
/* ============================================================================
 * GDT API
 * ============================================================================ */

void gdt_init(void);
void gdt_load(void);
const gdt_ptr_t* gdt_get_ptr(void);
void gdt_dump(void);
```

---

## 创建 kernel/interrupt/tss.h

```c
/* ==============================================================================
 * CCOS - Task State Segment (TSS) for x86_64
 * ==============================================================================
 * This module provides TSS management including IST stack configuration.
 * In x86_64, TSS is used for:
 * - IST (Interrupt Stack Table) stacks
 * - Privilege level stack pointers (not heavily used in long mode)
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"

/* ============================================================================
 * TSS Structure
 * ============================================================================ */

#define IST_DF       1    /* Double Fault uses IST1 */
#define IST_SS       4    /* Stack Fault uses IST4 */
#define IST_STACK_SIZE  (16 * 1024)  /* 16KB per IST stack */

typedef struct PACKED {
    uint32_t reserved0;       /* Reserved, must be 0 */
    uint32_t rsp0;            /* Privilege level 0 stack pointer */
    uint32_t rsp1;            /* Privilege level 1 stack pointer */
    uint32_t reserved1;       /* Reserved, must be 0 */
    uint32_t rsp2;            /* Privilege level 2 stack pointer */
    uint32_t reserved2;       /* Reserved, must be 0 */
    uint64_t ist1;            /* IST1 stack pointer */
    uint64_t ist2;            /* IST2 stack pointer */
    uint64_t ist3;            /* IST3 stack pointer */
    uint64_t ist4;            /* IST4 stack pointer */
    uint64_t ist5;            /* IST5 stack pointer */
    uint64_t ist6;            /* IST6 stack pointer */
    uint64_t ist7;            /* IST7 stack pointer */
    uint64_t reserved3;       /* Reserved, must be 0 */
    uint16_t reserved4;       /* Reserved, must be 0 */
    uint16_t iomap_base;      /* I/O map base address */
} __attribute__((packed)) tss_t;

/* ============================================================================
 * TSS API
 * ============================================================================ */

void tss_init(void);
tss_t* tss_get(void);
void tss_set_ist_stack(uint8_t ist_index, virtual_addr_t stack_top);
```

---

## 创建 kernel/interrupt/gdt.c

实现 GDT 管理：

```c
/* ==============================================================================
 * CCOS - Global Descriptor Table (GDT) Implementation
 * ==============================================================================
 */

#include "interrupt/gdt.h"
#include "interrupt/tss.h"
#include "base/memory.h"
#include "klogs/kprintf.h"

/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief Kernel GDT
 * 7 entries: null, kernel code, kernel data, user code, user data, TSS (2 entries)
 */
static struct {
    gdt_entry_t entries[5];       /* First 5 entries */
    gdt_tss_entry_t tss_entry;    /* TSS entry (16 bytes) */
} __attribute__((aligned(16))) s_gdt = {0};

static gdt_ptr_t s_gdt_ptr = {0};
static bool s_initialized = false;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static void gdt_set_entry(int index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t granularity) {
    if (index < 0 || index >= 5) {
        klog_error("[GDT] Invalid index: %d\n", index);
        return;
    }

    s_gdt.entries[index].limit_low = limit & 0xFFFF;
    s_gdt.entries[index].base_low = base & 0xFFFF;
    s_gdt.entries[index].base_middle = (base >> 16) & 0xFF;
    s_gdt.entries[index].access = access;
    s_gdt.entries[index].granularity = (limit >> 16) & 0x0F;
    s_gdt.entries[index].granularity |= granularity & 0xF0;
    s_gdt.entries[index].base_high = (base >> 24) & 0xFF;
}

static void gdt_set_tss(tss_t* tss) {
    uint64_t base = (uint64_t)tss;
    uint64_t limit = sizeof(tss_t) - 1;

    s_gdt.tss_entry.limit_low = limit & 0xFFFF;
    s_gdt.tss_entry.base_low = base & 0xFFFF;
    s_gdt.tss_entry.base_middle = (base >> 16) & 0xFF;
    s_gdt.tss_entry.access = 0x89;  /* Present, DPL0, Type=64-bit TSS */
    s_gdt.tss_entry.granularity = ((limit >> 16) & 0x0F) | 0x80;
    s_gdt.tss_entry.base_high = (base >> 24) & 0xFF;
    s_gdt.tss_entry.base_upper = (base >> 32) & 0xFFFFFFFF;
    s_gdt.tss_entry.reserved = 0;
}

/* External assembly functions */
extern void gdt_flush(uint64_t gdt_ptr);
extern void tss_load(void);

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void gdt_init(void) {
    if (s_initialized) {
        klog_warn("[GDT] Already initialized\n");
        return;
    }

    klog_info("[GDT] Setting up kernel GDT...\n");

    /* Clear GDT */
    memset(&s_gdt, 0, sizeof(s_gdt));

    /* Setup GDT pointer */
    s_gdt_ptr.limit = (sizeof(gdt_entry_t) * 5 + sizeof(gdt_tss_entry_t)) - 1;
    s_gdt_ptr.base = (uint64_t)&s_gdt;

    /* Entry 0: Null descriptor (already zeroed) */

    /* Entry 1: Kernel 64-bit code */
    gdt_set_entry(1, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_DPL0 |
                  GDT_ACCESS_SYSTEM | GDT_ACCESS_TYPE_CODE,
                  GDT_GRANULARITY_4K | GDT_GRANULARITY_64BIT);

    /* Entry 2: Kernel data */
    gdt_set_entry(2, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_DPL0 |
                  GDT_ACCESS_SYSTEM | GDT_ACCESS_TYPE_DATA,
                  GDT_GRANULARITY_4K | GDT_GRANULARITY_32BIT);

    /* Entry 3: User 64-bit code */
    gdt_set_entry(3, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_DPL3 |
                  GDT_ACCESS_SYSTEM | GDT_ACCESS_TYPE_CODE,
                  GDT_GRANULARITY_4K | GDT_GRANULARITY_64BIT);

    /* Entry 4: User data */
    gdt_set_entry(4, 0, 0xFFFFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_DPL3 |
                  GDT_ACCESS_SYSTEM | GDT_ACCESS_TYPE_DATA,
                  GDT_GRANULARITY_4K | GDT_GRANULARITY_32BIT);

    /* Entry 5: TSS */
    tss_t* tss = tss_get();
    if (tss == NULL) {
        klog_error("[GDT] TSS not initialized!\n");
        return;
    }
    gdt_set_tss(tss);

    /* Load GDT */
    gdt_load();

    s_initialized = true;
    klog_info("[GDT] GDT loaded at 0x%llX\n", s_gdt_ptr.base);
}

void gdt_load(void) {
    /* Call assembly function to load GDT */
    gdt_flush((uint64_t)&s_gdt_ptr);

    /* Load TSS */
    tss_load();
}

const gdt_ptr_t* gdt_get_ptr(void) {
    return &s_gdt_ptr;
}
```

---

## 创建 kernel/interrupt/tss.c

实现 TSS 管理：

```c
/* ==============================================================================
 * CCOS - Task State Segment (TSS) Implementation
 * ==============================================================================
 */

#include "interrupt/tss.h"
#include "mm/vmm/vmm.h"
#include "mm/heap/heap.h"
#include "base/memory.h"
#include "klogs/kprintf.h"

/* ============================================================================
 * Internal State
 * ============================================================================ */

static tss_t s_tss = {0};
static bool s_initialized = false;

/* IST stacks */
static uint8_t* s_ist1_stack = NULL;  /* Double Fault */
static uint8_t* s_ist4_stack = NULL;  /* Stack Fault */

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void tss_init(void) {
    if (s_initialized) {
        klog_warn("[TSS] Already initialized\n");
        return;
    }

    klog_info("[TSS] Initializing TSS...\n");

    /* Clear TSS */
    memset(&s_tss, 0, sizeof(tss_t));

    /* Allocate IST stacks */
    s_ist1_stack = (uint8_t*)kmalloc(IST_STACK_SIZE);
    s_ist4_stack = (uint8_t*)kmalloc(IST_STACK_SIZE);

    if (s_ist1_stack == NULL || s_ist4_stack == NULL) {
        klog_error("[TSS] Failed to allocate IST stacks\n");
        return;
    }

    /* Set IST stack pointers (stack grows down, so point to end) */
    s_tss.ist1 = (uint64_t)(s_ist1_stack + IST_STACK_SIZE);
    s_tss.ist4 = (uint64_t)(s_ist4_stack + IST_STACK_SIZE);

    /* Set kernel stack pointer (RSP0) */
    s_tss.rsp0 = (uint32_t)(s_ist1_stack + IST_STACK_SIZE);  /* Temporary */

    s_initialized = true;

    klog_info("[TSS] TSS initialized\n");
    klog_info("[TSS]   IST1 (DF): 0x%llX\n", s_tss.ist1);
    klog_info("[TSS]   IST4 (SS): 0x%llX\n", s_tss.ist4);
}

tss_t* tss_get(void) {
    if (!s_initialized) {
        return NULL;
    }
    return &s_tss;
}

void tss_set_ist_stack(uint8_t ist_index, virtual_addr_t stack_top) {
    if (!s_initialized) {
        return;
    }

    switch (ist_index) {
        case 1:
            s_tss.ist1 = stack_top;
            break;
        case 4:
            s_tss.ist4 = stack_top;
            break;
        default:
            klog_warn("[TSS] Invalid IST index: %d\n", ist_index);
            break;
    }
}
```

---

## 创建 kernel/interrupt/gdt.asm

汇编辅助函数：

```asm
; ==============================================================================
; CCOS - GDT/TSS Assembly Helper Functions
; ==============================================================================

section .text

; -----------------------------------------------------------------------------
; gdt_flush - Load the GDT
; Input:  RDI = pointer to gdt_ptr_t structure
; -----------------------------------------------------------------------------
global gdt_flush
gdt_flush:
    lgdt [rdi]              ; Load GDT pointer

    ; Reload segment registers
    mov ax, 0x10            ; Kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS
    push qword 0x08         ; Kernel code selector
    push qword .reload_cs
    retfq

.reload_cs:
    ret

; -----------------------------------------------------------------------------
; tss_load - Load the TSS
; Input:  None (uses GDT_TSS selector)
; -----------------------------------------------------------------------------
global tss_load
tss_load:
    mov ax, GDT_TSS         ; TSS selector
    ltr ax                  ; Load task register
    ret
```

---

## 更新 IDT 以使用 IST

在 IDT 初始化时，我们需要为 Double Fault 和 Stack Fault 设置 IST 索引：

```c
/* In idt.c or similar */

void idt_set_ist_index(uint8_t vector, uint8_t ist_index) {
    if (vector < 32) {
        /* IST index is stored in bits 0-2 of the offset_low field */
        idt_entries[vector].offset_low &= 0xFFFFFFF8;  /* Clear lower 3 bits */
        idt_entries[vector].offset_low |= (ist_index & 0x7);
    }
}

/* During IDT initialization */
void idt_init(void) {
    /* ... existing code ... */

    /* Set IST for critical exceptions */
    idt_set_ist_index(IDT_DF, IST_DF);   /* Double Fault → IST1 */
    idt_set_ist_index(IDT_SS, IST_SS);   /* Stack Fault → IST4 */
}
```

⚠️ 注意：IST 索引存储在 IDT 项的低 3 位中。这意味着处理函数地址必须 8 字节对齐。

---

## 更新 CMakeLists.txt

把新文件加入构建系统：

```cmake
# kernel/interrupt/CMakeLists.txt
target_sources(kernel_interrupt
    PRIVATE
        gdt.h
        gdt.c
        gdt.asm
        tss.h
        tss.c
        # ... other files
)
```

---

## 下一步

现在我们已经实现了 GDT 和 TSS 管理，内核可以：

1. 使用自己的 GDT 而不是 Bootloader 的
2. 为 Double Fault 和 Stack Fault 配置独立的 IST 栈
3. 正确加载 TSS

但 GDT 和 TSS 只是基础设施，它们本身不处理异常——我们需要实现异常处理器。

下一节我们将实现异常处理器，包括 Double Fault、Stack Fault 和 GPF 的处理逻辑。这些处理器将使用我们刚刚配置的 IST 栈。

在继续之前，请确保你理解了：
1. GDT 的结构和用途
2. TSS 在 x86_64 中的作用
3. IST 如何通过 TSS 和 IDT 配置
4. 为什么需要汇编辅助函数来加载 GDT 和 TSS
