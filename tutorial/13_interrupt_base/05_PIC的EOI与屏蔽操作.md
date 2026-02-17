# PIC 的 EOI 与屏蔽操作 —— Stage 13 中断基础实战指南

## 前言

上一篇文章我们实现了 PIC 的初始化，现在 PIC 已经知道要把 IRQ 映射到哪些向量了。但初始化只是第一步，中断系统要正常工作，还需要两个关键机制：EOI（中断结束）和 IRQ 屏蔽控制。

这两个功能非常重要。EOI 如果忘记发送，后续中断就不会触发；屏蔽控制如果用不好，要么中断一直触发导致系统崩溃，要么需要的中断被屏蔽掉导致设备不工作。所以这篇文章我们会非常仔细地讲解每个函数的实现。

---

## 第一步：理解 EOI 机制

EOI（End of Interrupt）是"中断结束"信号。当 CPU 处理完一个中断后，必须向 PIC 发送 EOI，告诉 PIC "这个中断处理完了，可以发送下一个了"。

### 为什么需要 EOI？

PIC 是一个"状态机"：当它发送一个中断给 CPU 后，会进入"服务中"状态，直到收到 EOI 才会恢复。如果 CPU 不发送 EOI，PIC 就认为中断还在处理中，不会发送后续的中断。

### 双 PIC 的 EOI 规则

对于双片级联结构，EOI 的发送规则是：
- **IRQ 0-7**（Master PIC）：只需向 Master 发送 EOI
- **IRQ 8-15**（Slave PIC）：需要向 Slave 和 Master 都发送 EOI

这是因为 Slave 的中断是通过 Master 的 IRQ 2 传递的，所以两个 PIC 都需要确认中断结束。

---

## 第二步：实现 pic_send_eoi() 函数

现在我们来实现 EOI 发送函数。

```c
void pic_send_eoi(uint8_t irq) {
    // 如果 IRQ 来自 Slave PIC (IRQ >= 8)，需要向 Slave 也发送 EOI
    if (irq >= 8) {
        pic_send_command(PIC_EOI, PIC2_CMD);
    }
    // 总是向 Master PIC 发送 EOI
    pic_send_command(PIC_EOI, PIC1_CMD);
}
```

这个函数的逻辑很简单：
1. 如果 IRQ 大于等于 8，说明是 Slave PIC 的中断，先向 Slave 发送 EOI
2. 无论如何都向 Master 发送 EOI

**为什么不直接用 `if (irq >= 8) else`？**

因为 Slave 的中断是通过 Master 传递的，即使 IRQ 在 Slave 上，Master 也需要知道中断结束了。

---

## 第三步：理解 IRQ 屏蔽机制

PIC 的每个 IRQ 都可以单独屏蔽（禁用）或启用。这是通过操作 IMR（中断屏蔽寄存器）实现的。

### IMR 位操作

```
IMR 寄存器（每个 PIC 有一个）：
Bit 7 6 5 4 3 2 1 0
     │ │ │ │ │ │ │ └─ IRQ 0 (定时器)
     │ │ │ │ │ │ └─── IRQ 1 (键盘)
     │ │ │ │ │ └───── IRQ 2 (级联)
     ...

Bit = 1: 屏蔽（禁用）该 IRQ
Bit = 0: 启用该 IRQ
```

要屏蔽某个 IRQ，就把对应的位设置为 1；要启用，就设置为 0。

### 两个 PIC 的 IMR

- Master PIC 的 IMR（端口 0x21）：控制 IRQ 0-7
- Slave PIC 的 IMR（端口 0xA1）：控制 IRQ 8-15

---

## 第四步：实现 pic_disable_irq() 函数

这个函数屏蔽指定的 IRQ：

```c
void pic_disable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t irq_bit;

    // 确定操作哪个 PIC
    if (irq < 8) {
        port = PIC1_DATA;   // Master PIC
        irq_bit = irq;
    } else {
        port = PIC2_DATA;   // Slave PIC
        irq_bit = irq - 8;  // Slave 的 IRQ 0 对应系统的 IRQ 8
    }

    // 读取当前 IMR
    uint8_t mask = pic_read_data(port);

    // 设置对应的位（屏蔽）
    mask |= (1 << irq_bit);

    // 写回 IMR
    pic_send_data(mask, port);
}
```

这里用了位操作 `|=` 来设置特定位。比如要屏蔽 IRQ 3：
```
mask |= (1 << 3)  // 即 mask |= 0x08
```

---

## 第五步：实现 pic_enable_irq() 函数

这个函数启用指定的 IRQ：

```c
void pic_enable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t irq_bit;

    // 确定操作哪个 PIC
    if (irq < 8) {
        port = PIC1_DATA;
        irq_bit = irq;
    } else {
        port = PIC2_DATA;
        irq_bit = irq - 8;
    }

    // 读取当前 IMR
    uint8_t mask = pic_read_data(port);

    // 清除对应的位（启用）
    mask &= ~(1 << irq_bit);

    // 写回 IMR
    pic_send_data(mask, port);
}
```

这里用了位操作 `&= ~` 来清除特定位。比如要启用 IRQ 3：
```
mask &= ~(1 << 3)  // 即 mask &= ~0x08
```

`~` 是按位取反，`~0x08 = 0xF7`，所以 `mask &= 0xF7` 会清除 bit 3。

---

## 第六步：实现 pic_is_irq_masked() 函数

这个函数查询某个 IRQ 是否被屏蔽：

```c
bool pic_is_irq_masked(uint8_t irq) {
    uint16_t port;
    uint8_t irq_bit;

    // 确定操作哪个 PIC
    if (irq < 8) {
        port = PIC1_DATA;
        irq_bit = irq;
    } else {
        port = PIC2_DATA;
        irq_bit = irq - 8;
    }

    // 读取 IMR
    uint8_t mask = pic_read_data(port);

    // 检查对应的位
    return (mask & (1 << irq_bit)) != 0;
}
```

如果对应的位是 1，返回 `true`（被屏蔽）；如果是 0，返回 `false`（已启用）。

---

## 第七步：实现 pic_disable_all() 和 pic_enable_all()

这两个函数控制所有 IRQ 的屏蔽状态：

```c
void pic_disable_all(void) {
    // 0xFF 表示所有位都是 1，即屏蔽所有 IRQ
    pic_send_data(0xFF, PIC1_DATA);
    pic_send_data(0xFF, PIC2_DATA);
}

void pic_enable_all(void) {
    // 0x00 表示所有位都是 0，即启用所有 IRQ
    pic_send_data(0x00, PIC1_DATA);
    pic_send_data(0x00, PIC2_DATA);
}
```

---

## 第八步：验证实现

现在我们可以测试一下这些函数是否正常工作。在内核初始化代码中添加测试：

```c
void kernel_init(void) {
    // ... 其他初始化 ...

    // 初始化 PIC
    pic_init(0x20, 0x28);

    // 测试屏蔽操作
    pic_disable_irq(0);  // 屏蔽定时器
    klog_info("IRQ 0 masked: %d\n", pic_is_irq_masked(0));

    pic_enable_irq(0);   // 启用定时器
    klog_info("IRQ 0 masked: %d\n", pic_is_irq_masked(0));

    // 屏蔽所有 IRQ
    pic_disable_all();
    klog_info("All IRQs disabled\n");

    // ... 其他代码 ...
}
```

编译运行：

```bash
cd build
cmake ..
make
./run.sh
```

你应该看到类似的输出：
```
[INFO] IRQ 0 masked: 1
[INFO] IRQ 0 masked: 0
[INFO] All IRQs disabled
```

---

## 踩坑预警

### 忘记发送 EOI

这是最常见的问题。中断处理函数结束后如果不发送 EOI，后续中断就不会触发。

```c
// 错误示例
void timer_handler(interrupt_frame_t* frame) {
    timer_ticks++;
    // 忘记 pic_send_eoi(0);
    // 后续定时器中断不会触发！
}

// 正确示例
void timer_handler(interrupt_frame_t* frame) {
    timer_ticks++;
    pic_send_eoi(0);  // 必须调用！
}
```

### Slave IRQ 只向一个 PIC 发送 EOI

处理 Slave IRQ（8-15）时，必须向两个 PIC 都发送 EOI。

```c
// 错误示例
void irq9_handler(interrupt_frame_t* frame) {
    // ...
    pic_send_eoi(PIC2_CMD);  // 错误！Master 也需要 EOI
}

// 正确示例
void irq9_handler(interrupt_frame_t* frame) {
    // ...
    pic_send_eoi(9);  // 函数内部会处理两个 PIC
}
```

### 在错误的时机修改屏蔽状态

中断处理过程中修改 IRQ 屏蔽状态要小心。如果在处理 IRQ A 时禁用了 IRQ A，然后发送 EOI，可能会导致问题。

---

## 到这里我们完成了什么

这篇文章我们完成了 PIC 驱动的所有功能：

- 实现了 EOI 发送机制
- 实现了 IRQ 屏蔽/启用控制
- 实现了屏蔽状态查询
- 实现了全部禁用/启用功能

现在 PIC 驱动已经完整了。下一篇文章我们会开始实现 IDT（中断描述符表）相关的代码。

---

## 接下来

在下一篇文章中，我们会：
1. 创建中断子系统目录
2. 定义 IDT 常量和异常名称
3. 定义中断帧结构体
4. 定义 IDT 条目结构体

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← PIC初始化序列实现](04_PIC初始化序列实现.md)  | [IDT结构与常量定义 →](06_IDT结构与常量定义.md)

</div>
