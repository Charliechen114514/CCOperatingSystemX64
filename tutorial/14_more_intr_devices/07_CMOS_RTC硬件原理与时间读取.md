# 07 - CMOS RTC硬件原理与时间读取

说实话，操作系统不知道时间这种感觉挺奇妙的。你启动内核，屏幕上显示 "System started"，但你不知道这是什么时候发生的。日志没有时间戳，调度器无法知道过去了多久，一切都基于"相对时间"（滴答数），而不是"绝对时间"。

CMOS RTC 芯片就是用来解决这个问题的。它从 1980 年代就存在了，至今仍然在每一台 PC 主板上。

---

## CMOS RAM 历史和地位

CMOS（Complementary Metal-Oxide-Semiconductor）RAM 最早出现在 IBM PC/AT（1984 年）上。它是一小块由电池供电的内存，用来存储系统配置（如硬盘参数、启动顺序）和实时时钟。

即使电脑关机，CMOS 仍然由主板电池供电，所以时间和配置不会丢失。这就是为什么你可以长时间不插电，重新开机后时间仍然正确的原因。

### 硬件接口

CMOS RTC 通过两个端口访问：

| 端口 | 名称 | 描述 |
|------|------|------|
| 0x70 | 地址寄存器 | 选择要访问的 CMOS 寄存器 |
| 0x71 | 数据寄存器 | 读/写选中的寄存器数据 |

访问流程：
1. 向 0x70 写入寄存器索引
2. 从/向 0x71 读/写数据

```c
// 读取
outb(0x70, index);
uint8_t data = inb(0x71);

// 写入
outb(0x70, index);
outb(0x71, data);
```

---

## NMI 禁用技巧

0x70 端口的 bit 7 是 NMI（不可屏蔽中断）禁用位：

```
┌──┬──┬──┬──┬──┬──┬──┬──┐
│ 7│ 6│ 5│ 4│ 3│ 2│ 1│ 0│
└──┴──┴──┴──┴──┴──┴──┴──┘
  │
  └── NMI Disable (1=禁用 NMI)
```

在访问 CMOS 时，通常会禁用 NMI，避免某些敏感操作被打断：

```c
#define CMOS_NMI_DISABLE 0x80

uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg | CMOS_NMI_DISABLE);  // 禁用 NMI
    return inb(0x71);
}

void cmos_write(uint8_t reg, uint8_t value) {
    outb(0x70, reg | CMOS_NMI_DISABLE);
    outb(0x71, value);
}
```

---

## RTC 寄存器布局

RTC 时间数据存储在寄存器 0x00-0x09：

| 索引 | 名称 | 范围 | 描述 |
|------|------|------|------|
| 0x00 | 秒 | 0-59 | 秒（BCD） |
| 0x02 | 分 | 0-59 | 分（BCD） |
| 0x04 | 时 | 0-23 | 时（BCD，24小时模式） |
| 0x06 | 星期 | 1-7 | 星期几（1=周日） |
| 0x07 | 日 | 1-31 | 月中的日 |
| 0x08 | 月 | 1-12 | 月 |
| 0x09 | 年 | 0-99 | 年（世纪需推断） |

注意索引是跳跃的（0x00, 0x02, 0x04...），这是因为某些寄存器保留用于报警功能。

---

## BCD 编码

RTC 默认使用 BCD（Binary-Coded Decimal）编码。BCD 把每个十进制 digit 存储在一个 nibble（4 位）中：

```
十进制 45:
- 二进制: 0010 1101 (0x2D)
- BCD:    0100 0101 (0x45)

BCD 的 0x45 = 0100(4) 0101(5) = 45
```

### BCD 转二进制

```c
static inline uint8_t bcd_to_binary(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}
```

### 二进制转 BCD

```c
static inline uint8_t binary_to_bcd(uint8_t binary) {
    return ((binary / 10) << 4) | (binary % 10);
}
```

### 自动检测模式

RTC 可以工作在 BCD 或二进制模式，由寄存器 B 的 bit 2（DM）决定：

```c
bool rtc_is_binary_mode(void) {
    uint8_t reg_b = cmos_read(0x0B);
    return (reg_b & (1 << 2)) != 0;
}
```

在读取时间前，先检查模式，然后相应转换：

```c
uint8_t rtc_read_field(uint8_t reg) {
    uint8_t value = cmos_read(reg);
    if (!rtc_is_binary_mode()) {
        value = bcd_to_binary(value);
    }
    return value;
}
```

---

## 更新等待（UIP）

RTC 内部会在每秒更新时间，这个过程需要一点时间。如果我们在更新期间读取，可能会读到不一致的数据（比如秒数变了但分钟没变）。

寄存器 A 的 bit 7（UIP，Update In Progress）指示是否正在更新：

```c
bool rtc_is_updating(void) {
    return (cmos_read(0x0A) & 0x80) != 0;
}
```

读取前等待更新完成：

```c
void rtc_wait_update(void) {
    while (rtc_is_updating()) {
        __asm__ volatile("pause");
    }
}
```

---

## 完整时间读取

### 时间结构体

```c
typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day_of_week;
    uint8_t day_of_month;
    uint8_t month;
    uint16_t year;  // 完整年份，如 2024
} rtc_time_t;
```

### 读取函数

```c
int rtc_get_time(rtc_time_t* time) {
    if (time == NULL) {
        return -1;
    }

    // 等待更新完成
    rtc_wait_update();

    // 读取各字段
    time->seconds = rtc_read_field(0x00);
    time->minutes = rtc_read_field(0x02);
    time->hours = rtc_read_field(0x04);
    time->day_of_week = rtc_read_field(0x06);
    time->day_of_month = rtc_read_field(0x07);
    time->month = rtc_read_field(0x08);

    // 年份需要处理世纪
    uint8_t year = rtc_read_field(0x09);
    // 假设 2000-2099 范围
    time->year = 2000 + year;

    return 0;
}
```

### 世纪推断

RTC 只存储两位年份（0-99），我们需要推断世纪。简单假设是 2000-2099：

```c
// 更精确的做法（考虑 1980-2079 范围）
uint16_t infer_century(uint8_t year) {
    // 假设 RTC 从 1980 年开始有效
    // 如果 year >= 80，认为是 19xx
    // 否则是 20xx
    if (year >= 80) {
        return 1900 + year;
    } else {
        return 2000 + year;
    }
}
```

但对于现代系统，直接假设 2000+ 就够了。

---

## 时间设置

```c
int rtc_set_time(const rtc_time_t* time) {
    if (time == NULL) {
        return -1;
    }

    // 禁用更新（寄存器 B bit 7）
    uint8_t reg_b = cmos_read(0x0B);
    cmos_write(0x0B, reg_b | 0x80);

    // 写入各字段（假设 BCD 模式）
    uint8_t year = time->year % 100;
    cmos_write(0x00, binary_to_bcd(time->seconds));
    cmos_write(0x02, binary_to_bcd(time->minutes));
    cmos_write(0x04, binary_to_bcd(time->hours));
    cmos_write(0x06, time->day_of_week);
    cmos_write(0x07, binary_to_bcd(time->day_of_month));
    cmos_write(0x08, binary_to_bcd(time->month));
    cmos_write(0x09, binary_to_bcd(year));

    // 恢复更新
    reg_b = cmos_read(0x0B);
    cmos_write(0x0B, reg_b & ~0x80);

    return 0;
}
```

---

## 验证电池状态

RTC 由电池供电，如果电池没电，时间会丢失。寄存器 D 的 bit 7 指示电池状态：

```c
bool rtc_battery_ok(void) {
    uint8_t reg_d = cmos_read(0x0D);
    return (reg_d & 0x80) != 0;  // 1 = 电池正常
}
```

---

## 测试

```c
void test_rtc(void) {
    rtc_time_t time;

    if (rtc_get_time(&time) == 0) {
        klog_info("Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  time.year, time.month, time.day_of_month,
                  time.hours, time.minutes, time.seconds);
    } else {
        klog_error("Failed to read RTC time\n");
    }

    if (!rtc_battery_ok()) {
        klog_warn("RTC battery is low or dead!\n");
    }
}
```

---

## 接下来

现在我们可以读取 RTC 时间了。下一节，我们会实现 RTC 的周期性中断功能，让系统能够定期执行某些任务。

→ [下一篇：RTC周期性中断与闹钟功能](./08_RTC周期性中断与闹钟功能.md)

---

<div align="center">

## 文档导航

[← 键盘中断处理与集成测试](./06_键盘中断处理与集成测试.md) | [RTC周期性中断与闹钟功能 →](./08_RTC周期性中断与闹钟功能.md)

</div>
