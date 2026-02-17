# PIC 初始化序列实现 —— Stage 13 中断基础实战指南

## 前言

上一篇文章我们搭建了 PIC 驱动的框架，现在到了实现核心逻辑的时候了。说实话，8259 PIC 的初始化序列是我觉得整个中断系统中最容易出错的部分。为什么这么说？因为初始化有一套固定的顺序，错一步 PIC 就没法工作，而且问题还很难排查。

所以这篇文章我们会非常详细地讲解每一步在做什么，为什么要这样做。我们按顺序来，一步一步完成 PIC 的初始化。

---

## 第一步：理解初始化的必要性

在开始写代码之前，我们需要明白为什么需要初始化 PIC。

### 上电后的状态

计算机上电后，PIC 处于默认配置状态：
- IRQ 0-7 映射到向量 0x08-0x0F
- IRQ 8-15 映射到向量 0x70-0x77

### 问题所在

x86 CPU 规定向量 0-31 保留给异常：
- 向量 0：Divide Error
- 向量 1：Debug
- ...
- 向量 8：Double Fault
- ...

如果 PIC 不重新映射，IRQ 0（定时器）会映射到向量 8，这与 Double Fault 冲突！结果就是定时器中断会被误认为是双故障，系统完全无法工作。

### 解决方案

我们需要重新配置 PIC，将 IRQ 映射到向量 32-47：
```
IRQ 0-7  → 向量 32-39 (0x20-0x27)
IRQ 8-15 → 向量 40-47 (0x28-0x2F)
```

---

## 第二步：理解 ICW 初始化序列

8259 PIC 的初始化通过发送四个初始化命令字（ICW）完成：

```
ICW1 → ICW2 → ICW3 → ICW4
```

每一步都有特定的作用，顺序不能错。

### ICW1：开始初始化

告诉 PIC "我要开始初始化了"，并发送一些配置：

```
发送到：命令端口 (PIC1_CMD = 0x20, PIC2_CMD = 0xA0)
值：   0x11
       │  │
       │  └─ Bit 0 = 1: 初始化开始
       └─ Bit 4 = 1: 需要 ICW4
```

### ICW2：设置向量偏移

告诉 PIC "IRQ 0 对应哪个向量号"：

```
发送到：数据端口 (PIC1_DATA = 0x21, PIC2_DATA = 0xA1)
值：   Master: 32 (0x20) - IRQ 0-7 对应向量 32-39
       Slave:  40 (0x28) - IRQ 8-15 对应向量 40-47
```

### ICW3：配置级联

告诉 Master "Slave 连接在哪个 IRQ 上"：

```
发送到：数据端口
值：   Master: 0x04 - Slave 连接在 IR2
       Slave:  0x02 - 级联标识码
```

### ICW4：设置 8086 模式

告诉 PIC "我们使用的是 8086 CPU"：

```
发送到：数据端口
值：   0x01
       │
       └─ Bit 0 = 1: 8086 模式
```

---

## 第三步：实现 pic_init() 函数

现在我们来实现 `pic_init()` 函数。这个函数会按照 ICW1→ICW2→ICW3→ICW4 的顺序初始化两个 PIC。

打开 `kernel/driver/pic/pic.c`，修改 `pic_init()` 函数：

```c
void pic_init(uint8_t offset1, uint8_t offset2) {
    // 第一步：保存当前的屏蔽状态
    uint8_t a1 = pic_read_data(PIC1_DATA);
    uint8_t a2 = pic_read_data(PIC2_DATA);

    // 第二步：发送 ICW1 - 开始初始化
    pic_send_command(PIC_INIT, PIC1_CMD);
    pic_send_command(PIC_INIT, PIC2_CMD);

    // 第三步：发送 ICW2 - 设置向量偏移
    pic_send_data(offset1, PIC1_DATA);  // Master: IRQ 0-7 → 32-39
    pic_send_data(offset2, PIC2_DATA);  // Slave:  IRQ 8-15 → 40-47

    // 第四步：发送 ICW3 - 配置级联
    pic_send_data(0x04, PIC1_DATA);     // Master: IR2 连接 slave
    pic_send_data(0x02, PIC2_DATA);     // Slave: 级联标识

    // 第五步：发送 ICW4 - 设置 8086 模式
    pic_send_data(PIC_ICW4_8086, PIC1_DATA);
    pic_send_data(PIC_ICW4_8086, PIC2_DATA);

    // 第六步：恢复屏蔽状态
    pic_send_data(a1, PIC1_DATA);
    pic_send_data(a2, PIC2_DATA);
}
```

每一步的作用我已经用注释标清楚了。这里有几个细节需要注意：

**为什么要保存和恢复屏蔽状态？**

初始化过程会重置 PIC 的内部状态，包括中断屏蔽寄存器（IMR）。如果我们不保存原来的值，初始化后所有 IRQ 都会被启用，这可能不是我们想要的。保存后恢复，可以保持原来的屏蔽配置。

**为什么两个 PIC 都要初始化？**

因为这是双片级联结构，Master 和 Slave 是两个独立的芯片，都需要初始化。

---

## 第四步：添加辅助函数

你可能注意到了代码中用到了 `pic_send_command()` 和 `pic_send_data()` 这些辅助函数。我们需要先实现它们。

在 `pic.c` 的开头添加这些辅助函数：

```c
/**
 * @brief Send a command to the PIC
 *
 * @param cmd  Command byte to send
 * @param port Command port (PIC1_CMD or PIC2_CMD)
 */
static void pic_send_command(uint8_t cmd, uint16_t port) {
    outb(port, cmd);
}

/**
 * @brief Send data to the PIC
 *
 * @param data Data byte to send
 * @param port Data port (PIC1_DATA or PIC2_DATA)
 */
static void pic_send_data(uint8_t data, uint16_t port) {
    outb(port, data);
}

/**
 * @brief Read data from the PIC
 *
 * @param port Data port to read from
 * @return uint8_t Data byte read
 */
static uint8_t pic_read_data(uint16_t port) {
    return inb(port);
}
```

这些函数是对 I/O 端口读写的简单封装。`outb()` 和 `inb()` 是内核提供的端口 I/O 函数，定义在 `io/io.h` 中。

---

## 第五步：理解 IMR（中断屏蔽寄存器）

PIC 的数据端口连接到中断屏蔽寄存器（IMR）。IMR 的每一位对应一个 IRQ：

```
IMR (Interrupt Mask Register):
Bit 7 6 5 4 3 2 1 0
     │ │ │ │ │ │ │ └─ IRQ 0
     │ │ │ │ │ │ └─── IRQ 1
     │ │ │ │ │ └───── IRQ 2
     │ │ │ │ └─────── IRQ 3
     │ │ │ └───────── IRQ 4
     │ │ └─────────── IRQ 5
     │ └───────────── IRQ 6
     └─────────────── IRQ 7

Bit = 1: IRQ 被屏蔽（禁用）
Bit = 0: IRQ 启用
```

所以：
- `outb(0xFF, PIC1_DATA)` 会屏蔽所有 IRQ 0-7
- `outb(0x00, PIC1_DATA)` 会启用所有 IRQ 0-7
- `outb(0x01, PIC1_DATA)` 只屏蔽 IRQ 0

这就是为什么我们后面可以用位操作来控制单个 IRQ 的屏蔽状态。

---

## 第六步：验证初始化

现在我们来验证一下初始化是否正确。我们可以在内核初始化时调用 `pic_init()`：

```c
// 在 kernel_init.c 中
#include "driver/pic/pic.h"

void kernel_init(void) {
    // ... 其他初始化代码 ...

    // 初始化 PIC
    pic_init(0x20, 0x28);  // offset1=32, offset2=40

    klog_info("PIC initialized: IRQs remapped to vectors 32-47\n");

    // ... 其他代码 ...
}
```

编译并运行：

```bash
cd build
cmake ..
make
./run.sh
```

如果你看到日志输出 `PIC initialized`，说明初始化函数被成功调用了。当然，现在中断还没有真正启用，我们只是配置好了 PIC。

---

## 踩坑预警

这里有几个常见的坑，你可能会遇到：

### 忘记恢复屏蔽状态

如果你不保存和恢复 IMR，初始化后所有 IRQ 都会被启用。这可能导致中断风暴 —— 大量中断同时触发，系统崩溃。

```c
// 错误示例
void pic_init_bad(uint8_t offset1, uint8_t offset2) {
    // 初始化 PIC
    // ...
    // 忘记恢复屏蔽状态！
    // 现在 IRQ 0-15 全部启用了
}
```

### ICW 顺序错误

ICW 必须按照 1→2→3→4 的顺序发送。如果顺序错了，PIC 可能进入未知状态。

### 向量偏移选择错误

偏移必须是 16 的倍数（对齐到 16 字节边界），而且必须避开 CPU 异常向量（0-31）。

```c
// 错误示例
pic_init(0x10, 0x18);  // 错误！与 CPU 异常冲突

// 正确示例
pic_init(0x20, 0x28);  // 正确！向量 32-47
```

### 级联配置错误

Master PIC 的 ICW3 必须是 `0x04`（IR2），Slave PIC 的 ICW3 必须是 `0x02`。如果配置错了，Slave PIC 的中断无法传递到 CPU。

---

## 到这里我们完成了什么

这篇文章我们实现了 PIC 的初始化序列：

- 理解了为什么需要 IRQ 重映射
- 掌握了 ICW1-ICW4 初始化命令
- 实现了 `pic_init()` 函数
- 添加了辅助函数
- 理解了 IMR 的工作原理

虽然现在 PIC 已经配置好了，但我们还缺少 EOI 和 IRQ 屏蔽控制。下一篇文章我们会实现这些功能，完善 PIC 驱动。

---

## 接下来

在下一篇文章中，我们会：
1. 实现 `pic_send_eoi()` 函数
2. 实现 `pic_disable_irq()` / `pic_enable_irq()` 函数
3. 实现屏蔽状态查询函数
4. 实现全部禁用/启用函数
5. 完成完整的 PIC 驱动

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 创建PIC驱动框架](03_创建PIC驱动框架.md)  | [PIC的EOI与屏蔽操作 →](05_PIC的EOI与屏蔽操作.md)

</div>
