# 08 - RTC 周期性中断与闹钟功能

上一节我们实现了 RTC 时间的读取，说实话，光是能读时间还远远不够。RTC 真正强大的地方在于它能够产生周期性中断——从 2Hz 一直到 8192Hz，这相当于一个免费的硬件定时器，而且不用我们自己去维护计数。这一节我们就来把这个功能用好，顺便把闹钟功能也加上。

---

## 为什么需要周期性中断

你可能会问，我们不是已经有 PIT 定时器了吗，为什么还要用 RTC 的周期性中断？原因其实挺实在的：PIT 固定连在 IRQ 0 上，频率相对固定，而且它的精度虽然高，但如果我们只需要一个每秒触发一次的定时任务，用 PIT 未免有点大材小用。RTC 的周期性中断频率可调，最低可以到 0.25Hz，最高 8192Hz，灵活性相当不错。

更重要的是，RTC 的周期性中断是由独立的硬件时钟源驱动的，即使 CPU 处于某些低功耗状态，它依然能够正常工作。这在将来我们要实现电源管理的时候会非常有用。

---

## RTC 中断机制速览

RTC 通过 IRQ 8 触发中断，这个中断线连接在 PIC2 上。有意思的是，RTC 支持三种不同的中断源，它们可以同时工作，共享同一个 IRQ 线：

第一种是周期性中断，以我们设定的固定频率触发，非常适合用来做定时任务调度；第二种是闹钟中断，当当前时间与我们预设的闹钟时间匹配时触发；第三种是更新结束中断，这个我们暂时用不到。这三种中断的状态都存储在寄存器 C 中，我们可以在中断处理器中读取这个寄存器来判断具体是哪种中断触发了。

---

## 寄存器深挖

### 寄存器 A 的速率选择器

寄存器 A 的低四位叫做 RS 位，它们决定了周期性中断的频率。这个设计挺巧妙的，通过一个简单的 4 位值就能覆盖从 0.25Hz 到 8192Hz 的范围。比如我们设置 RS 为 0x0D，就会得到 1Hz 的中断频率，正好每秒触发一次。设置为 0x0A 就是 8Hz，每 125 毫秒触发一次。

这里有个坑要注意，修改 RS 位之前最好先禁用 IRQ 8，否则在配置过程中可能会触发意外的中断。修改完成后再重新启用 IRQ，这样比较稳妥。

### 寄存器 B 的中断使能

寄存器 B 有几个关键的使能位，其中 PIE（bit 6）控制周期性中断，AIE（bit 5）控制闹钟中断。这两个位是独立的，我们可以只启用其中一个，也可以两个都启用。在实际代码中，我们通常会先禁用所有中断，配置完成后再按需启用。

寄存器 C 是只读的，读取后会自动清零，这个特性很方便。我们在中断处理器中首先读取这个寄存器，然后检查 PF 位（周期中断标志）和 AF 位（闹钟中断标志），根据不同的标志执行相应的回调函数。

---

## 实现周期性中断

### 先定义好常量

代码写起来其实不复杂，但先把常量定义清楚能让后续的逻辑更清晰。我们为常用的频率定义一些宏，这样使用的时候不用记住每个频率对应的十六进制值。

```c
// 寄存器 A 速率值对应不同的频率
#define RTC_RATE_8192Hz   0x00
#define RTC_RATE_4096Hz   0x01
#define RTC_RATE_2048Hz   0x02
#define RTC_RATE_1024Hz   0x03
#define RTC_RATE_512Hz    0x04
#define RTC_RATE_256Hz    0x05
#define RTC_RATE_128Hz    0x06
#define RTC_RATE_64Hz     0x07
#define RTC_RATE_32Hz     0x08
#define RTC_RATE_16Hz     0x09
#define RTC_RATE_8Hz      0x0A
#define RTC_RATE_4Hz      0x0B
#define RTC_RATE_2Hz      0x0C
#define RTC_RATE_1Hz      0x0D

// 寄存器 B 的中断使能位
#define RTC_REG_B_PIE      (1 << 6)  // 周期中断使能
#define RTC_REG_B_AIE      (1 << 5)  // 闹钟中断使能
```

### 启用周期中断的核心逻辑

启用周期性中断的函数需要接收几个参数：速率选择器、回调函数和上下文指针。回调函数的设计是为了让使用者可以在中断发生时执行自己的逻辑，上下文指针则允许我们传递一些自定义数据给回调函数。

实现上，我们首先要保存回调函数和上下文，这样在中断发生时才能调用正确的处理逻辑。然后禁用 IRQ 8 防止配置期间触发中断，接着修改寄存器 A 的 RS 位设置频率，修改寄存器 B 的 PIE 位启用周期中断，最后读取寄存器 C 清除可能存在的待处理中断，重新启用 IRQ 8，整个流程就完成了。

```c
typedef void (*rtc_periodic_callback_fn)(void* context);

static rtc_periodic_callback_fn periodic_callback = NULL;
static void* periodic_context = NULL;

int rtc_enable_periodic(uint8_t rate_div, rtc_periodic_callback_fn callback, void* context) {
    // 先把中断禁掉，避免配置过程中触发
    pic_disable_irq(8);

    // 保存回调函数和上下文
    periodic_callback = callback;
    periodic_context = context;

    // 设置频率（寄存器 A）
    uint8_t reg_a = cmos_read(0x0A);
    reg_a = (reg_a & ~0x0F) | (rate_div & 0x0F);  // 只修改 RS 位
    cmos_write(0x0A, reg_a);

    // 启用周期中断（寄存器 B）
    uint8_t reg_b = cmos_read(0x0B);
    reg_b |= RTC_REG_B_PIE;
    cmos_write(0x0B, reg_b);

    // 清除待处理中断（寄存器 C）
    cmos_read(0x0C);

    // 重新启用 IRQ 8
    pic_enable_irq(8);

    klog_info("RTC periodic interrupt enabled at rate code 0x%02X\n", rate_div);
    return 0;
}
```

### 中断处理器

中断处理器的逻辑很直接，读取寄存器 C，然后检查 PF 位和 AF 位，分别调用对应的回调函数。这里有个小细节，寄存器 C 读取后会自动清零，所以我们在处理完中断后不需要手动清除标志。

```c
static irq_descriptor_t rtc_irq_desc = {
    .name = "CMOS RTC",
    .handler = NULL,
    .context = NULL,
    .flags = IRQ_FLAG_NONE,
    .invocation_count = 0
};

static void rtc_irq_handler(interrupt_frame_t* frame, void* context) {
    (void)frame;
    (void)context;

    // 读取寄存器 C 清除中断
    uint8_t reg_c = cmos_read(0x0C);

    // 检查周期中断标志
    if (reg_c & (1 << 6)) {  // PF 位
        if (periodic_callback != NULL) {
            periodic_callback(periodic_context);
        }
    }

    // 检查闹钟中断标志
    if (reg_c & (1 << 5)) {  // AF 位
        if (alarm_callback != NULL) {
            alarm_callback(alarm_context);
        }
    }

    rtc_irq_desc.invocation_count++;
    pic_send_eoi(8);
}
```

---

## 闹钟功能的实现

RTC 的闹钟功能比我们想象的要灵活一些。它可以设置为在特定的时、分、秒触发，而且支持"忽略"某个字段。具体来说，如果我们把某个字段设置为 0xC0，这个字段就会被忽略，闹钟就会只匹配其他字段。

### 设置闹钟

设置闹钟的函数接收时、分、秒三个参数，以及回调函数和上下文。如果某个参数传入 0xC0，就表示忽略这个字段。比如我们想每天早上 8 点都触发闹钟，就可以把秒和分都设置为 0xC0，小时设置为 8。

```c
typedef void (*rtc_alarm_callback_fn)(void* context);

static rtc_alarm_callback_fn alarm_callback = NULL;
static void* alarm_context = NULL;

int rtc_set_alarm(uint8_t hours, uint8_t minutes, uint8_t seconds,
                  rtc_alarm_callback_fn callback, void* context) {
    // 禁用 IRQ 8
    pic_disable_irq(8);

    // 保存回调
    alarm_callback = callback;
    alarm_context = context;

    // 设置闹钟时间
    cmos_write(0x01, seconds | 0xC0);  // 0xC0 表示忽略秒
    cmos_write(0x03, minutes | 0xC0);  // 0xC0 表示忽略分
    cmos_write(0x05, hours);           // 每天的这个时间触发

    // 启用闹钟中断
    uint8_t reg_b = cmos_read(0x0B);
    reg_b |= RTC_REG_B_AIE;
    cmos_write(0x0B, reg_b);

    // 清除待处理中断
    cmos_read(0x0C);

    // 启用 IRQ 8
    pic_enable_irq(8);

    klog_info("RTC alarm set for %02d:%02d:%02d\n", hours, minutes, seconds);
    return 0;
}
```

### 禁用闹钟

禁用闹钟的逻辑很简单，只需要清除寄存器 B 的 AIE 位，然后把回调函数指针置空即可。不过这里有个细节需要注意，如果周期性中断还在使用，我们不应该把 IRQ 8 完全禁用，只禁用闹钟中断就好。

```c
void rtc_disable_alarm(void) {
    uint8_t reg_b = cmos_read(0x0B);
    reg_b &= ~RTC_REG_B_AIE;
    cmos_write(0x0B, reg_b);

    alarm_callback = NULL;
    alarm_context = NULL;

    klog_info("RTC alarm disabled\n");
}
```

---

## 初始化流程

RTC 的初始化需要设置一些基本的配置。我们通常会在系统启动时调用这个函数，确保 RTC 处于一个已知的状态。首先检查并设置 24 小时模式，然后注册 IRQ 8 的中断处理器，这样之后的中断才能被正确处理。

```c
int rtc_init(void) {
    // 设置 24 小时模式
    uint8_t reg_b = cmos_read(0x0B);
    reg_b |= (1 << 1);  // 24HR
    cmos_write(0x0B, reg_b);

    // 注册 IRQ 8 处理器
    rtc_irq_desc.handler = rtc_irq_handler;
    irq_register_handler(8, &rtc_irq_desc);

    klog_info("RTC driver initialized\n");
    return 0;
}
```

---

## 测试验证

写完了代码，当然要测试一下。对于周期性中断，我们可以设置一个 8Hz 的频率，然后在中断回调中增加计数器，每 8 次中断打印一次信息，这样就能验证中断是否按预期每秒触发一次。

```c
static uint32_t rtc_tick_count = 0;

static void rtc_periodic_handler(void* context) {
    (void)context;
    rtc_tick_count++;

    if (rtc_tick_count % 8 == 0) {  // 每秒（假设 8Hz）
        klog_trace("RTC tick: %u\n", rtc_tick_count);
    }
}

void test_rtc_periodic(void) {
    rtc_init();
    rtc_enable_periodic(RTC_RATE_8Hz, rtc_periodic_handler, NULL);

    // 主循环
    while (true) {
        __asm__ volatile("hlt");
    }
}
```

对于闹钟功能，我们可以先读取当前时间，然后设置一个 10 秒后的闹钟，看看到时候中断是否触发。测试的时候记得先调用 rtc_init 初始化驱动。

```c
static void rtc_alarm_handler(void* context) {
    (void)context;
    klog_info("ALARM! Wake up!\n");
}

void test_rtc_alarm(void) {
    rtc_init();

    // 获取当前时间
    rtc_time_t now;
    rtc_get_time(&now);

    // 设置 10 秒后的闹钟
    uint8_t alarm_sec = (now.seconds + 10) % 60;
    uint8_t alarm_min = now.minutes + ((now.seconds + 10) / 60);
    uint8_t alarm_hour = now.hours;

    rtc_set_alarm(alarm_hour, alarm_min, alarm_sec, rtc_alarm_handler, NULL);

    klog_info("Alarm set for %02d:%02d:%02d\n", alarm_hour, alarm_min, alarm_sec);
}
```

---

## 接下来

RTC 的周期性中断和闹钟功能都实现完成了，我们现在有了两个独立的定时中断源。下一节我们会实现 PIT（可编程间隔定时器）驱动，它会提供更灵活的定时功能，而且是我们系统滴答的主要来源。

→ [下一篇：PIT可编程定时器实现](./09_PIT可编程定时器实现.md)

---

<div align="center">

## 文档导航

[← CMOS RTC硬件原理与时间读取](./07_CMOS_RTC硬件原理与时间读取.md) | [PIT可编程定时器实现 →](./09_PIT可编程定时器实现.md)

</div>
