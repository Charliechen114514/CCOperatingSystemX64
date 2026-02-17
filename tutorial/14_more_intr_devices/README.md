# Stage 14 更多中断设备 - 教程

本目录包含 CCOS Stage 14（更多中断设备）的完整教程文档，通过 15 个循序渐进的手把手教程，带你从零实现 PS/2 键盘驱动、CMOS RTC 驱动、PIT 定时器、中断驱动串口和通用 Shell 系统。

---

## 阶段概述

**Stage 14: 更多中断设备**

本阶段在 Stage 13 中断基础之上，扩展了多种硬件设备的中断驱动支持，实现了完整的交互式 Shell 系统。

### 核心成果

- **PS/2 键盘驱动** - 中断驱动、扫描码转 ASCII、修饰键支持
- **CMOS RTC 驱动** - 时间读取、周期中断、闹钟功能
- **PIT 定时器驱动** - 可配置频率、回调机制
- **中断驱动串口** - FIFO、异步 I/O
- **通用 Shell 系统** - 后端抽象、VGA/Serial 支持
- **VGA 增强** - 软件光标、闪烁效果
- **kprintf 优化** - format 模块、ksnprintf

---

## 教程列表

按照开发顺序阅读，每个教程都是在上一个教程的基础上继续：

| 序号 | 教程 | 内容 |
|------|------|------|
| 01 | [为什么需要更多中断设备驱动](./01_为什么需要更多中断设备驱动.md) | 阶段背景、痛点分析、目标概览 |
| 02 | [中断系统重构——统一IRQ注册机制](./02_中断系统重构——统一IRQ注册机制.md) | 新 IRQ 注册 API、描述符结构 |
| 03 | [数据结构基础——环形缓冲区从零实现](./03_数据结构基础——环形缓冲区从零实现.md) | 环形缓冲区原理、实现细节 |
| 04 | [PS/2协议与扫描码详解](./04_PS/2协议与扫描码详解.md) | PS/2 接口、扫描码集、Make/Break 代码 |
| 05 | [键盘驱动实现——扫描码转ASCII](./05_键盘驱动实现——扫描码转ASCII.md) | 查找表、修饰键处理、状态机 |
| 06 | [键盘中断处理与集成测试](./06_键盘中断处理与集成测试.md) | IRQ 1 处理器、API 封装 |
| 07 | [CMOS RTC硬件原理与时间读取](./07_CMOS_RTC硬件原理与时间读取.md) | CMOS 访问、BCD 转换、时间读取 |
| 08 | [RTC周期性中断与闹钟功能](./08_RTC周期性中断与闹钟功能.md) | 寄存器配置、周期中断、闹钟 |
| 09 | [PIT可编程定时器实现](./09_PIT可编程定时器实现.md) | 8254 PIT 原理、频率计算 |
| 10 | [串口中断驱动原理与FIFO](./10_串口中断驱动原理与FIFO.md) | UART 16550 FIFO、中断机制 |
| 11 | [串口中断处理实现与优化](./11_串口中断处理实现与优化.md) | TX/RX 处理、缓冲区管理 |
| 12 | [Shell系统设计——后端抽象架构](./12_Shell系统设计——后端抽象架构.md) | 后端接口设计、命令解析 |
| 13 | [VGA与Serial Shell后端实现](./13_VGA与Serial_Shell后端实现.md) | 软件光标、回显控制 |
| 14 | [kprintf优化与snprintf实现](./14_kprintf优化与snprintf实现.md) | format 模块、ksnprintf |
| 15 | [完整集成与系统验证](./15_完整集成与系统验证.md) | 初始化流程、功能测试 |

---

## 前置要求

在开始本阶段教程之前，请确保你已经完成：

1. **Stage 13（中断基础）**：理解 IDT、PIC、中断处理流程
2. **基础开发环境**：GCC、NASM、CMake、QEMU
3. **C 语言基础**：指针、结构体、位操作
4. **x86 汇编基础**：端口 I/O、中断处理

---

## 环境确认

```bash
# 检查工具版本
gcc --version    # >= 9.0
nasm -v          # >= 2.14
cmake --version  # >= 3.15
qemu-system-x86_64 --version  # >= 4.0

# 切换到 stage/14 分支
git checkout stage/14_more_intr_devices

# 构建
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# 运行
qemu-system-x86_64 -drive format=raw,file=build/boot.img
```

---

## 快速开始

### 查看代码结构

```bash
# 查看键盘驱动
ls kernel/driver/keyboard/

# 查看串口中断驱动
ls kernel/driver/serial/

# 查看 Shell 系统
ls kernel/shell/
```

### 运行演示

编译并运行内核后，你应该能够：

1. 在 VGA 屏幕上看到启动信息
2. 通过键盘输入命令
3. 看到当前时间显示
4. 使用串口连接到另一个 Shell

---

## 技术亮点

### 1. 环形缓冲区设计

中断安全的 FIFO 实现，支持：
- 固定大小，无动态内存分配
- 原子操作优化
- 满/空快速判断

### 2. 后端抽象接口

Shell 采用后端抽象模式：
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

### 3. 中断驱动 I/O

所有设备采用中断驱动模式：
- 键盘：IRQ 1
- RTC：IRQ 8
- 定时器：IRQ 0
- 串口：IRQ 4

### 4. 软件光标

VGA 软件光标实现，支持：
- 多种样式（实心块、下划线）
- 闪烁效果
- 字符保存/恢复

---

## 与前一阶段对比

| 特性 | Stage 13 | Stage 14 |
|------|----------|----------|
| 键盘支持 | 无 | PS/2 键盘中断驱动 |
| 时钟支持 | 基础滴答 | RTC + 可配置 PIT |
| 串口支持 | 轮询模式 | 中断驱动 + FIFO |
| 用户交互 | 无 | 完整 Shell 系统 |
| VGA 显示 | 基础输出 | 软件光标 + 滚动优化 |
| 格式化输出 | 基础 kprintf | ksnprintf + 模块化 |
| 新增文件 | 13 个 | 25+ 个 |

---

## 学习路径建议

1. **快速预览**：阅读文档 01 了解阶段背景
2. **核心理解**：阅读文档 02-03 理解基础设施
3. **设备驱动**：阅读文档 04-09 学习各设备驱动
4. **高级功能**：阅读文档 10-15 学习串口、Shell、优化

---

## 调试技巧

### 查看 IRQ 状态

```c
// 在代码中或 GDB 中
debug_irq_status();
```

### 打印驱动状态

```c
debug_driver_status();
```

### QEMU Monitor

```bash
# 在 QEMU Monitor 中
info registers
info pic
x /10x $eax  # 查看寄存器
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
- [../../document/14_more_intr_devices/](../../document/14_more_intr_devices/) - 技术文档

### 源码文件
- [`kernel/driver/keyboard/`](../../kernel/driver/keyboard/) - 键盘驱动
- [`kernel/driver/rtc/`](../../kernel/driver/rtc/) - RTC 驱动
- [`kernel/driver/timer/`](../../kernel/driver/timer/) - 定时器驱动
- [`kernel/shell/`](../../kernel/shell/) - Shell 系统

### 外部参考
- [OSDev.org PS/2 Keyboard](https://wiki.osdev.org/PS/2_Keyboard)
- [OSDev.org RTC](https://wiki.osdev.org/RTC)
- [OSDev.org PIT](https://wiki.osdev.org/Programmable_Interval_Timer)
- [OSDev.org UART](https://wiki.osdev.org/UART)

---

**开始学习**: [01 - 为什么需要更多中断设备驱动](./01_为什么需要更多中断设备驱动.md)
