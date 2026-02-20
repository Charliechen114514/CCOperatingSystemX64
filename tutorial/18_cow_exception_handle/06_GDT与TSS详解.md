# 06 - GDT 与 TSS 详解

说实话，第一次理解 x86_64 下 GDT 和 TSS 的作用时，我挺困惑的——不是说长模式下分段几乎没用了吗？

---

## GDT 在 x86_64 中的作用

在 32 位保护模式下，GDT（Global Descriptor Table）是分段机制的核心。但在 x86_64 长模式下，分段功能被大大削弱，GDT 的作用也发生了变化。

### 长模式下分段的变化

在 x86_64 长模式下：

- **代码段**：基址必须是 0，限制必须是最大（除了 FS/GS）
- **数据段**：完全被忽略，实际上不复存在
- **FS/GS 段**：仍然可以用于线程局部存储（TLS）

这意味着分段机制在长模式下几乎不存在了，但 GDT 仍然是必需的。

### GDT 在长模式下的用途

既然分段没什么用了，为什么还需要 GDT？

**原因一：代码段选择器仍然必需**

虽然代码段的基址必须是 0，但 CPU 仍然需要从 GDT 中加载代码段描述符来验证：

- 段存在位（P）
- 特权级（DPL）
- 类型（代码段 vs 数据段）
- 长模式标志（L 位）

```
CS 寄存器 → 选择器 → GDT 索引 → 描述符 → 验证
```

**原因二：TSS 需要通过 GDT 加载**

TSS（Task State Segment）在 x86_64 中不再用于任务切换，但仍然用于：

- 存储 IST（Interrupt Stack Table）栈地址
- 存储特权级栈指针（虽然在长模式下不太相关）

TSS 必须通过 GDT 加载，使用 `ltr` 指令。

**原因三：用户态代码需要用户段选择器**

虽然用户代码段的基址也是 0，但 DPL 不同。我们需要为用户代码和数据创建描述符，让 CPU 能够正确处理特权级切换。

### 典型的 x86_64 GDT 布局

```
索引  | 偏移 | 描述符
------|------|------------------
0     | 0x00 | Null 描述符
1     | 0x08 | 内核 64 位代码段
2     | 0x10 | 内核数据段
3     | 0x18 | 用户 64 位代码段
4     | 0x20 | 用户数据段
5-6   | 0x28 | TSS 描述符（16 字节）
```

⚠️ 注意：选择器值是索引乘以 8，因为每个描述符 8 字节。

---

## TSS 的演变

TSS 在 x86 历史中经历了很大的变化。

### 32 位保护模式下的 TSS

在 32 位时代，TSS 用于：

- **任务切换**：硬件支持的多任务切换
- **特权级栈**：存储不同特权级的栈指针
- **寄存器状态**：保存任务的寄存器

TSS 非常复杂，包含大量字段。

### x86_64 下的 TSS

在 x86_64 长模式下：

- **任务切换被移除**：不再有硬件任务切换
- **TSS 简化**：只保留必要字段
- **IST（中断栈表）**：新增的功能，用于异常栈隔离

### x86_64 TSS 结构

```c
typedef struct PACKED {
    uint32_t reserved0;       /* Reserved, must be 0 */
    uint32_t rsp0;            /* Privilege level 0 stack pointer */
    uint32_t rsp1;            /* Privilege level 1 stack pointer */
    uint32_t reserved1;       /* Reserved, must be 0 */
    uint32_t rsp2;            /* Privilege level 2 stack pointer */
    uint32_t reserved2;       /* Reserved, must be 0 */
    uint64_t ist1;            /* IST1 stack pointer (for Double Fault) */
    uint64_t ist2;            /* IST2 stack pointer */
    uint64_t ist3;            /* IST3 stack pointer */
    uint64_t ist4;            /* IST4 stack pointer (for Stack Fault) */
    uint64_t ist5;            /* IST5 stack pointer */
    uint64_t ist6;            /* IST6 stack pointer */
    uint64_t ist7;            /* IST7 stack pointer */
    uint64_t reserved3;       /* Reserved, must be 0 */
    uint16_t reserved4;       /* Reserved, must be 0 */
    uint16_t iomap_base;      /* I/O map base address */
} __attribute__((packed)) tss_t;
```

⚠️ 注意：这个结构是简化版，实际的 TSS 还有其他字段。

---

## IST（中断栈表）机制

IST 是 x86_64 引入的重要机制，用于解决异常处理中的栈问题。

### 为什么需要 IST

想象这个场景：

```
1. 内核栈已经接近溢出
    ↓
2. 发生异常（如 #PF）
    ↓
3. CPU 尝试把帧压入内核栈
    ↓
4. 触发 #SS（栈错误）
    ↓
5. CPU 再次尝试压入栈
    ↓
6. 又触发栈错误
    ↓
7. #DF（Double Fault）
```

如果 #DF 也使用同一个内核栈，就会无限循环，最终 Triple Fault（系统重启）。

### IST 的解决方案

IST 允许为特定中断向量配置独立的栈：

```
正常中断 → 使用当前栈（RSP）
#DF (IST1) → 使用独立的 IST1 栈
#SS (IST4) → 使用独立的 IST4 栈
```

这样即使当前栈已损坏，关键异常处理器仍然有一个完好的栈可以使用。

### IST 配置方法

1. **在 TSS 中设置 IST 栈指针**

```c
tss->ist1 = ist1_stack_top;    // Double Fault 栈
tss->ist4 = ist4_stack_top;    // Stack Fault 栈
```

2. **在 IDT 项中指定 IST 索引**

```c
// IDT 项的位 0-2 存储 IST 索引
idt_entries[8].ist_index = 1;   // #DF 使用 IST1
idt_entries[12].ist_index = 4;  // #SS 使用 IST4
```

3. **异常发生时，CPU 自动切换到 IST 栈**

```
#DF 发生
    ↓
CPU 检查 IDT[8].ist_index = 1
    ↓
从 TSS.ist1 加载新 RSP
    ↓
压入栈帧到 IST1 栈
    ↓
调用 Double Fault 处理器
```

---

## GDT 描述符格式

在实现之前，我们需要理解 GDT 描述符的格式。

### 普通段描述符（8 字节）

```
┌────────────────────────────────────────────────────────┐
│                    段描述符格式                         │
├────────┬────────┬────────┬────────┬────────┬───────────┤
│ 基址   │              段限制              │ 标志位  │
│ 32位   │         位 19-0 及位 23-16               │ 位 15-0 │
└────────┴────────┴────────┴────────┴────────┴───────────┘
```

**详细分解**：

```
字节 0-1:  段限制[15:0]
字节 2-3:  基址[15:0]
字节 4:    基址[23:16]
字节 5:    访问字节
           - P (位 7): 存在位
           - DPL (位 6-5): 描述符特权级 (0-3)
           - S (位 4): 系统段 (0) 或代码/数据段 (1)
           - Type (位 3-0): 段类型
字节 6:    标志位
           - G (位 7): 粒度 (0=字节, 1=4KB)
           - DB (位 6): 16/32 位 (保护模式) 或 64 位标志
           - L (位 5): 64 位代码段标志
           - AVL (位 4): 可用位
           - 段限制[19:16] (位 3-0)
字节 7:    基址[31:24]
```

### TSS 描述符（16 字节）

x86_64 的 TSS 描述符是 16 字节：

```
字节 0-1:  TSS 限制[15:0]
字节 2-3:  TSS 基址[15:0]
字节 4:    TSS 基址[23:16]
字节 5:    访问字节 (0x89: 存在, DPL0, 系统, 可用 TSS)
字节 6:    标志位 (包含限制[19:16])
字节 7:    TSS 基址[31:24]
字节 8-11: TSS 基址[63:32]
字节 12-15: 保留 (必须为 0)
```

### 访问字节值

```c
#define GDT_ACCESS_PRESENT    (1 << 7)   /* P 位 */
#define GDT_ACCESS_DPL0       (0 << 5)   /* DPL 0 (内核) */
#define GDT_ACCESS_DPL3       (3 << 5)   /* DPL 3 (用户) */
#define GDT_ACCESS_SYSTEM     (1 << 4)   /* S 位：1=代码/数据段 */
#define GDT_ACCESS_TYPE_CODE  (0xA)      /* 代码段 (可读/执行) */
#define GDT_ACCESS_TYPE_DATA  (0x2)      /* 数据段 (可读写) */
#define GDT_ACCESS_TYPE_TSS   (0x9)      /* 64 位可用 TSS */
```

---

## 为什么内核需要自己的 GDT

Bootloader 已经创建了 GDT，为什么内核还要重新创建？

**原因一：可靠性**

Bootloader 的 GDT 可能不满足内核的需求：
- 可能没有设置 64 位代码段标志
- 可能没有为用户模式创建段
- 可能没有 TSS 描述符

**原因二：完整性**

内核需要完全控制系统的所有数据结构，包括 GDT。依赖 Bootloader 的结构不是一个好习惯。

**原因三：TSS 加载**

加载 TSS 需要 GDT 中有 TSS 描述符，Bootloader 通常不会创建 TSS。

---

## 下一步

现在我们已经理解了 GDT 和 TSS 的机制，包括：

- GDT 在 x86_64 中的作用
- TSS 结构和用途
- IST 机制如何工作
- GDT 描述符格式

下一节我们将实现 GDT 和 TSS 管理模块，包括创建内核 GDT、配置 IST 栈、加载 TSS 等。

这些实现将为异常处理器提供必要的支持，特别是为 Double Fault 和 Stack Fault 配置独立的 IST 栈。

在继续之前，请确保你理解了：
1. 为什么长模式下还需要 GDT
2. TSS 在 x86_64 中的用途
3. IST 如何解决栈损坏问题
4. GDT 描述符的格式
