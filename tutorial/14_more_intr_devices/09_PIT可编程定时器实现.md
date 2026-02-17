# 09 - PIT 可编程定时器实现

8254 PIT（Programmable Interval Timer）是 PC 上最经典的定时器芯片，虽然现在听起来有点年代感了。说实话，现代系统确实有更多高级定时器可用——比如 APIC Timer、HPET 之类的——但 PIT 仍然是最基础、最可靠的选择，而且它默认就连接在 IRQ 0 上，用起来很方便。这一节我们就来实现一个完整的 PIT 驱动，让系统有一个稳定的滴答源。

---

## 为什么需要 PIT

你可能会问，我们不是已经有了 RTC 的周期性中断吗，为什么还要再搞一个 PIT？原因其实很简单：RTC 的周期性中断虽然灵活，但它的频率范围有限，而且它是为"实时时钟"这个用途设计的，不是用来做系统滴答的。PIT 则不同，它从一开始就是为系统定时而生的。

PIT 的输入时钟频率大约是 1.19318 MHz，通过编程可以得到从 18 Hz 到 1193180 Hz 范围内的任何频率。而且 PIT 的中断直接连在 IRQ 0 上，这是最高优先级的可屏蔽中断，能保证定时器中断的及时响应。对于操作系统来说，一个稳定的滴答源是必不可少的基础设施。

---

## PIT 架构速览

PIT 芯片内部有三个独立的计数通道，每个通道都可以独立编程。不过在我们这个场景下，只需要关注通道 0 就够了。通道 0 历史上就是用来做系统定时器的，它连接到 IRQ 0，每次计数归零就会触发一次中断。

通道 1 在早期的 PC 上是用来做内存刷新的，现在基本上不用了。通道 2 连接到 PC 扬声器，可以用来产生简单的蜂鸣声，不过我们暂时也不需要。所以重点就是通道 0，把它配置好，整个系统的定时机制就有了基础。

---

## 命令寄存器详解

### 端口布局

PIT 的 I/O 端口布局很直观，0x40 到 0x42 分别对应三个通道的数据端口，0x43 是命令寄存器。我们操作的主要是 0x40 和 0x43 这两个端口。

### 命令寄存器的位结构

命令寄存器虽然只有一个字节，但里面包含的信息可不少。高两位用来选择通道，接下来两位控制读写模式，中间三位是工作模式，最低一位选择 BCD 还是二进制。

对于我们的系统定时器，配置是这样的：选择通道 0，使用先低后高的 16 位访问模式，工作在模式 3（方波发生器），使用二进制计数。把这些设置组合起来，命令字节就是 0x36，这个值值得记住。

---

## 频率计算的内幕

### 基础频率的由来

PIT 的输入频率是 1193180 Hz，这个数字看起来很奇怪，但其实是有些历史原因的。它源自 NTSC 彩色电视副载波频率的一半，具体到 IBM PC 时代，选择这个频率是为了兼顾成本和精度。不管怎样，这是一个固定值，我们只需要用它来计算除数。

### 除数计算公式

要得到想要的频率，我们用基础频率除以目标频率，得到的就是要写入 PIT 的除数值。比如我们想要 1000 Hz 的中断频率，除数就是 1193180 / 1000 = 1193。

这里有个边界情况需要注意，PIT 的除数是 16 位的，最大值是 65535，这意味着我们能得到的最低频率大约是 18 Hz。反过来，除数最小是 1，对应最高频率 1193180 Hz。不过实际使用中，我们通常设置在 100 Hz 到 1000 Hz 之间，这个范围对操作系统来说比较合适。

---

## 定时器驱动的实现

### 先定义好常量

把硬件相关的常量定义清楚是好习惯，这样以后如果需要移植或者修改，只需要改一个地方。

```c
// timer_constants.h
#define PIT_BASE_FREQUENCY      1193180
#define TIMER_DEFAULT_FREQUENCY 1000

#define PIT_COMMAND_REG      0x43
#define PIT_CHANNEL0_DATA    0x40
#define PIT_CHANNEL1_DATA    0x41
#define PIT_CHANNEL2_DATA    0x42
```

### 核心数据结构

我们需要维护一个全局的滴答计数器，每次中断就加一。另外还需要一个回调函数指针，这样用户可以注册自己的定时处理逻辑。

```c
static volatile uint64_t timer_ticks = 0;
static void (*timer_callback)(void) = NULL;
```

### 设置频率的内部函数

这个函数是整个驱动的核心，它负责把命令字节和除数写入 PIT。先发送命令字，然后分别发送除数的低字节和高字节，顺序不能乱。

```c
static void pit_set_frequency(uint32_t frequency) {
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency;

    // 发送控制字：二进制模式，模式 3（方波），先低后高访问
    outb(PIT_CMD_PORT, PIT_CHANNEL_0 | PIT_MODE_3 | PIT_ACCESS_WORD);

    // 发送除数（低字节然后高字节）
    outb(PIT_CHANNEL0_DATA, divisor & 0xFF);
    outb(PIT_CHANNEL0_DATA, (divisor >> 8) & 0xFF);
}
```

### 中断处理器的实现

中断处理器的逻辑非常简单：增加滴答计数，调用回调函数（如果注册了），然后发送 EOI。这里有个细节，回调函数的参数我们在接口设计时留了空，如果有需要可以传递滴答数作为参数。

```c
static void timer_irq_handler(interrupt_frame_t* frame, void* context) {
    (void)frame;
    (void)context;

    timer_ticks++;

    if (timer_callback != NULL) {
        timer_callback();
    }

    timer_irq_desc.invocation_count++;
    pic_send_eoi(0);
}
```

---

## 公共 API 的设计

### 初始化函数

初始化函数负责设置 PIT 的工作频率，注册中断处理器，然后启用 IRQ 0。如果传入的频率是 0，就使用默认的 1000 Hz。

```c
int timer_init(uint32_t frequency) {
    if (frequency == 0) {
        frequency = TIMER_DEFAULT_FREQUENCY;
    }

    // 保存频率
    timer_frequency = frequency;

    // 配置 PIT
    pit_set_frequency(frequency);

    // 注册 IRQ 0 处理器
    timer_irq_desc.handler = timer_irq_handler;
    irq_register_handler(0, &timer_irq_desc);

    // 启用 IRQ 0
    pic_enable_irq(0);

    klog_info("PIT timer initialized at %u Hz\n", frequency);
    return 0;
}
```

### 运行时修改频率

有时候我们需要在运行时改变定时器频率，这个函数就是为此设计的。它会检查频率是否在有效范围内，然后重新配置 PIT。

```c
int timer_set_frequency(uint32_t frequency) {
    // 频率必须在 18 到 1193180 之间
    if (frequency < 18 || frequency > PIT_BASE_FREQUENCY) {
        return -1;
    }

    timer_frequency = frequency;
    pit_set_frequency(frequency);

    return 0;
}
```

### 毫秒延时函数

这个函数在很多地方都会用到，它基于滴答计数来实现精确的毫秒级延时。需要注意的是，这个函数依赖中断正常工作，如果中断被禁用，滴答计数不会增加，延时就会不准确。

```c
void timer_mdelay(uint32_t milliseconds) {
    uint64_t start = timer_ticks;
    uint64_t target = start + (milliseconds * timer_frequency) / 1000;

    while (timer_ticks < target) {
        __asm__ volatile("pause");
    }
}
```

---

## 测试验证

### 基础滴答测试

最简单的测试就是初始化定时器，然后在主循环中不断打印滴答数。如果定时器工作正常，你会看到滴答数稳定增长，每次增加的间隔大约是 1 毫秒（假设 1000 Hz）。

```c
void test_timer(void) {
    timer_init(1000);  // 1000 Hz = 1ms

    while (true) {
        klog_info("Ticks: %llu\n", timer_get_ticks());
        timer_mdelay(1000);  // 延时 1 秒
    }
}
```

### 回调函数测试

这个测试验证回调机制是否正常工作。我们注册一个回调函数，让它每 1000 次调用打印一次信息。如果回调正常工作，你会看到每秒钟打印一条消息。

```c
static int callback_count = 0;

static void my_timer_callback(void) {
    callback_count++;
    if (callback_count % 1000 == 0) {  // 每秒
        klog_trace("Timer callback: %d\n", callback_count);
    }
}

void test_timer_callback(void) {
    timer_init(1000);
    timer_set_callback(my_timer_callback);

    // 启用中断
    interrupt_enable();

    while (true) {
        __asm__ volatile("hlt");
    }
}
```

---

## 常见问题排查

### 定时器频率不对

这个问题最常见的原因是基础频率记错了。有些人会记成 1193182 或者其他相近的值，但正确的值是 1193180。如果你发现定时器比实际快或慢，首先检查这个常量。

### 中断不触发

如果中断不触发，首先要确认 IRQ 0 已经被启用。可以通过 pic_enable_irq(0) 来启用。其次检查命令寄存器的值是否正确，应该是 0x36。如果这两个都没问题，那就需要检查 IDT 的设置是否正确。

### 延时函数不准确

timer_mdelay 不准确通常是因为在中断被禁用的情况下调用。滴答计数是在中断处理器中增加的，如果中断被禁用，计数就不会增长。确保在调用延时函数时中断是开启的。

---

## 接下来

PIT 定时器驱动已经完成了，现在我们有了一个稳定的系统滴答源。接下来我们要升级串口驱动，从轮询模式改为中断驱动模式，这样 CPU 就不用一直等待串口操作完成，效率会大大提升。

→ [下一篇：串口中断驱动原理与FIFO](./10_串口中断驱动原理与FIFO.md)

---

<div align="center">

## 文档导航

[← RTC周期性中断与闹钟功能](./08_RTC周期性中断与闹钟功能.md) | [串口中断驱动原理与FIFO →](./10_串口中断驱动原理与FIFO.md)

</div>
