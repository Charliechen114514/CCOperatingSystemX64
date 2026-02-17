# CCOS 更多中断设备 文档中心

本目录包含 CCOS Stage 14 - 更多中断设备开发的完整文档体系。

---

## 阶段概述

**Stage 14: 更多中断设备**

本阶段在 Stage 13 中断基础之上，扩展了多种硬件设备的中断驱动支持，实现了完整的交互式 Shell 系统，为操作系统提供了丰富的人机交互能力。

### 核心成果

- **PS/2 键盘驱动** ([`kernel/driver/keyboard/keyboard.h`](../../kernel/driver/keyboard/keyboard.h))
  - 中断驱动的 PS/2 键盘支持 (IRQ 1)
  - 扫描码集 1 到 ASCII 转换
  - Shift/Caps Lock 修饰键处理
  - 256 字节环形缓冲区

- **CMOS RTC 驱动** ([`kernel/driver/rtc/rtc.h`](../../kernel/driver/rtc/rtc.h))
  - 实时时钟读取与设置
  - 周期性中断 (2Hz ~ 8192Hz)
  - 闹钟功能
  - BCD/二进制模式自动转换

- **PIT 定时器驱动** ([`kernel/driver/timer/timer.h`](../../kernel/driver/timer/timer.h))
  - 8253/8254 PIT 可配置频率驱动
  - 回调机制支持
  - 毫秒级延时函数

- **中断驱动串口** ([`kernel/driver/serial/serial_intr.h`](../../kernel/driver/serial/serial_intr.h))
  - UART 异步发送/接收
  - FIFO 中断阈值支持
  - 双向环形缓冲区

- **通用 Shell 系统** ([`kernel/shell/shell.h`](../../kernel/shell/shell.h))
  - 后端抽象接口设计
  - VGA Shell 后端
  - Serial Shell 后端
  - 命令注册机制

- **VGA 增强功能** ([`kernel/driver/vga/vga.h`](../../kernel/driver/vga/vga.h))
  - 软件光标实现
  - 光标闪烁效果
  - 滚动优化

- **kprintf 优化** ([`kernel/klogs/ksnprintf.h`](../../kernel/klogs/ksnprintf.h))
  - ksnprintf 独立实现
  - format 模块解耦

---

## 目录结构

```
kernel/
├── driver/
│   ├── keyboard/
│   │   ├── keyboard.h          # 键盘驱动接口
│   │   ├── keyboard.c          # 键盘驱动实现
│   │   └── keyboard_config.h   # 键盘配置常量
│   ├── rtc/
│   │   ├── rtc.h               # RTC 驱动接口
│   │   ├── rtc.c               # RTC 驱动实现
│   │   └── rtc_constants.h     # RTC 常量定义
│   ├── timer/
│   │   ├── timer.h             # 定时器驱动接口
│   │   ├── timer.c             # 定时器驱动实现
│   │   └── timer_constants.h   # 定时器常量定义
│   ├── serial/
│   │   ├── serial_intr.h       # 串口中断接口
│   │   ├── serial_intr.c       # 串口中断实现
│   │   └── serial_config.h     # 串口配置
│   └── vga/
│       ├── vga.h               # VGA 驱动接口
│       ├── vga.c               # VGA 驱动实现（含软件光标）
│       └── vga_config.h        # VGA 配置
├── shell/
│   ├── shell.h                 # Shell 核心接口
│   ├── shell.c                 # Shell 核心实现
│   └── backends/
│       ├── vga_shell.h/c       # VGA Shell 后端
│       └── serial_shell.h/c    # Serial Shell 后端
└── klogs/
    ├── ksnprintf.h             # snprintf 实现
    └── private/
        ├── format.h            # 格式化模块
        └── format.c            # 格式化实现
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要这些中断设备驱动
- 设计决策（环形缓冲区、后端抽象、扫描码处理）
- 架构设计与模块协作
- 实现细节与关键技术
- 常见陷阱与注意事项
- 未来改进方向

**适合**:
- 理解设计思路
- 学习系统架构
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- PS/2 键盘协议详解
- CMOS RTC 技术参考（寄存器、中断）
- PIT 定时器技术参考
- UART 中断机制
- Shell API 完整参考
- 数据结构定义
- 常量定义

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 问题诊断

**内容**:
- 键盘输入无响应问题
- RTC 时间读取异常
- 串口中断不触发
- Shell 显示异常
- 系统稳定性问题

每个问题按"症状-原因-解决方案"格式组织。

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

### 4. [调试工具指南](./调试工具指南.md) 工具使用

**内容**:
- QEMU Monitor 调试技巧
- GDB 中断调试方法
- 串口日志分析
- 驱动状态查看工具
- 性能分析技巧

**适合**:
- 学习调试工具
- 查询具体命令
- 提升调试效率

---

## 快速开始

### 查看代码结构

1. **键盘驱动接口** → 查看 [`kernel/driver/keyboard/keyboard.h`](../../kernel/driver/keyboard/keyboard.h)
2. **RTC 驱动接口** → 查看 [`kernel/driver/rtc/rtc.h`](../../kernel/driver/rtc/rtc.h)
3. **定时器驱动接口** → 查看 [`kernel/driver/timer/timer.h`](../../kernel/driver/timer/timer.h)
4. **Shell 接口** → 查看 [`kernel/shell/shell.h`](../../kernel/shell/shell.h)

### 使用示例

```c
#include "driver/keyboard/keyboard.h"
#include "driver/rtc/rtc.h"
#include "driver/timer/timer.h"
#include "shell/shell.h"

// 内核初始化时调用
void kernel_init(void) {
    // 初始化键盘驱动
    keyboard_init();

    // 初始化 RTC
    rtc_init();

    // 初始化定时器
    timer_init(1000);  // 1000 Hz

    // 启用 RTC 周期性中断
    rtc_enable_periodic(RTC_RATE_8Hz, my_rtc_callback, NULL);
}

// 读取键盘输入
void read_keyboard_input(void) {
    if (keyboard_haschar()) {
        char c = keyboard_getchar();
        // 处理字符...
    }
}

// 读取 RTC 时间
void read_rtc_time(void) {
    rtc_time_t time;
    if (rtc_get_time(&time) == 0) {
        char buffer[32];
        rtc_format_time(&time, buffer, sizeof(buffer));
        klog_info("Current time: %s\n", buffer);
    }
}
```

### Shell 使用示例

```c
#include "shell/shell.h"
#include "shell/backends/vga_shell.h"

// 在主循环中启动 VGA Shell
void kernel_main(void) {
    // 注册自定义命令
    shell_register_command("hello", "Print hello message", cmd_hello);

    // 运行 VGA Shell
    shell_run(vga_shell_backend());
}
```

---

## 与前一阶段对比

| 特性 | Stage 13 (中断基础) | Stage 14 (更多中断设备) |
|------|---------------------|-------------------------|
| 键盘支持 | 无 | PS/2 键盘中断驱动 |
| 时钟支持 | 基础滴答 | RTC + 可配置 PIT |
| 串口支持 | 轮询模式 | 中断驱动 + FIFO |
| 用户交互 | 无 | 完整 Shell 系统 |
| VGA 显示 | 基础输出 | 软件光标 + 滚动优化 |
| 格式化输出 | 基础 kprintf | ksnprintf + 模块化 |
| 新增文件 | 13 个 | 25+ 个 |

---

## 技术亮点

### 1. 环形缓冲区设计

键盘和串口驱动采用环形缓冲区实现异步 I/O：

```c
typedef struct {
    char buffer[256];
    volatile uint32_t head;
    volatile uint32_t tail;
} ring_buffer_t;
```

**优点**:
- 中断安全
- 无内存分配
- 固定延迟

### 2. 后端抽象设计

Shell 采用后端抽象模式，支持多种输出设备：

```c
typedef struct shell_backend {
    const char* name;
    void (*puts)(const char* str);
    void (*putc)(char c);
    bool (*haschar)(void);
    char (*getchar)(void);
    void (*clear)(void);
} shell_backend_t;
```

### 3. 扫描码状态机

键盘驱动实现了完整的修饰键状态跟踪：

```c
static volatile bool kb_shift_pressed = false;
static volatile bool kb_caps_lock = false;

// Caps Lock 只影响字母
if (kb_caps_lock && is_letter_scancode(scancode)) {
    use_shift = !use_shift;
}
```

### 4. RTC BCD 转换

自动处理 BCD 和二进制模式：

```c
static inline uint8_t bcd_to_binary(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}
```

---

## 文档关系图

```
                    ┌──────────────────┐
                    │   项目根目录    │
                    │  (PROGRESS.md)   │
                    └────────┬─────────┘
                             │
                    ┌────────▼─────────┐
                    │  document/       │
                    └────────┬─────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
┌───────▼──────┐   ┌─────▼──────┐   ┌─────▼──────┐
│ 开发笔记     │   │ 技术参考   │   │ 故障排查   │
└──────────────┘   └─────────────┘   └─────────────┘
        │                    │
        └──────────────────┬─────────┘
                           │
                    ┌──────▼──────┐
                    │ 调试工具指南 │
                    └─────────────┘
```

---

## 版本信息

- **阶段**: Stage 14
- **分支**: `stage/14_more_intr_devices`
- **提交**: `a4a2a67` - VGA/Serial, Keyboard drivers
- **日期**: 2026-02-17
- **作者**: Charliechen114514

---

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../13_interrupt_base/README.md](../13_interrupt_base/) - 上一阶段文档

### 源码文件
- [`kernel/driver/keyboard/keyboard.h`](../../kernel/driver/keyboard/keyboard.h)
- [`kernel/driver/rtc/rtc.h`](../../kernel/driver/rtc/rtc.h)
- [`kernel/driver/timer/timer.h`](../../kernel/driver/timer/timer.h)
- [`kernel/shell/shell.h`](../../kernel/shell/shell.h)
- [`kernel/driver/serial/serial_intr.h`](../../kernel/driver/serial/serial_intr.h)

### 外部参考
- [PS/2 Keyboard Protocol](https://wiki.osdev.org/PS/2_Keyboard)
- [CMOS RTC](https://wiki.osdev.org/RTC)
- [8254 PIT](https://wiki.osdev.org/Programmable_Interval_Timer)
- [UART 16550](https://wiki.osdev.org/UART)
- [OSDev.org](https://wiki.osdev.org)

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-17
