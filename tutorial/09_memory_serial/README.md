# CCOS 内存操作与串口驱动 - 教程

本阶段将实现标准 C 内存操作函数和 UART 16550 串口驱动，大幅提升内核的调试能力。

---

## 阶段概述

### 你将学到什么

| 技能 | 描述 |
|------|------|
| 标准库实现 | 如何从零实现 memset/memcpy/memmove/memcmp |
| 主机端测试 | 在宿主机上测试内核代码的方法 |
| x86 端口 I/O | inb/outb 指令和内联汇编 |
| UART 驱动开发 | UART 16550 寄存器和初始化流程 |
| 多模式编程 | 在 16/32/64 位模式下实现相同功能 |
| 调试技巧 | QEMU 串口配置和 GDB 调试 |

### 与前一个阶段的对比

| 特性 | stage/08 | stage/09 (本阶段) |
|-----|----------|------------------|
| 内存函数 | ❌ 无 | ✅ memset/memcpy/memmove/memcmp |
| 串口驱动 | ❌ 无 | ✅ UART 16550 |
| 端口 I/O | ❌ 无 | ✅ inb/outb |
| 调试输出 | 仅 VGA | VGA + 串口双输出 |
| Bootloader 串口 | ❌ 无 | ✅ 全模式支持 |
| 单元测试 | ❌ 无 | ✅ 主机端测试框架 |

---

## 文档导航

本教程包含 7 篇文档，按开发顺序排列：

### 1. [为什么需要内存操作和串口驱动](./01_为什么需要内存操作和串口驱动.md)
**动机和环境准备**

- 当前开发的痛点
- 内存函数和串口的价值
- 环境验证

### 2. [从零实现标准 C 内存函数](./02_从零实现标准C内存函数.md)
**内存函数完整实现**

- memset 实现（从后向前填充）
- memcpy 实现
- memmove 实现（重叠检测）
- memcmp 实现
- **主机端测试框架搭建**

### 3. [端口 I/O 操作封装](./03_端口IO操作封装.md)
**x86 硬件访问基础**

- 端口 I/O vs 内存映射 I/O
- inb/outb 内联汇编实现
- volatile 关键字的重要性

### 4. [UART 16550 串口驱动实现](./04_UART16550串口驱动实现.md)
**完整的串口驱动**

- UART 16550 寄存器映射
- 波特率计算（115200）
- 串口初始化六步骤
- sync_serial_putc/sync_serial_puts 实现

### 5. [让内核支持串口输出](./05_让内核支持串口输出.md)
**内核集成**

- 修改 kernel_init.c
- 修改 kernel_main.c
- CMakeLists.txt 配置
- 编译验证

### 6. [Bootloader 串口支持](./06_Bootloader串口支持.md)
**三种模式的串口实现**

- **实模式串口**（16-bit）完整实现
- **保护模式串口**（32-bit）完整实现
- **长模式串口**（64-bit）完整实现
- bootloader.asm 修改流程

### 7. [上板测试与验证](./07_上板测试与验证.md)
**测试和调试**

- QEMU 串口重定向配置
- 运行完整测试
- 常见问题排查
- 串口日志保存
- GDB 调试技巧

---

## 快速开始

### 环境要求

```bash
# 检查工具
nasm -v          # NASM version 2.x.x
gcc --version    # gcc (Ubuntu xx.x.x.x) xx.x.x
cmake --version  # cmake version x.x.x
qemu-system-x86_64 --version  # QEMU emulator version x.x.x
```

### 构建和运行

```bash
# 配置构建
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 构建
cmake --build build

# 运行内存测试
./build/test/test_memory

# 运行 QEMU（串口重定向）
qemu-system-x86_64 -drive format=raw,file=build/boot.img \
    -nographic -serial mon:stdio
```

### 预期输出

```
=== CCOS Bootloader v1.0 ===
[Serial] Bootloader initialized
[Serial] Entering protected mode...
[Serial] Entering long mode...
[Serial] Loading kernel...
=== CCOS Kernel ===
Serial port initialized at 115200 8N1
[KERNEL] Hello from CCOS kernel!
[KERNEL] VGA + Serial dual output is working!
```

---

## 关键代码文件

```
kernel/base/
├── memory.c         # 内存操作实现
└── memory.h         # 内存操作声明

kernel/driver/io/
├── io.c             # 端口 I/O 实现
└── io.h             # 端口 I/O 声明

kernel/driver/serial/
├── serial.c         # 串口驱动实现
├── serial.h         # 串口驱动声明
└── serial_config.h  # 串口寄存器定义

kernel/
├── kernel_init.c    # 内核初始化（添加串口初始化）
└── kernel_main.c    # 主函数（添加串口输出）

boot/
└── bootloader.asm   # Bootloader（添加串口支持）

test/
├── test_memory.c    # 内存函数测试
└── assert_stub.c    # 主机端断言桩
```

---

## 技术要点

### 内存函数设计

| 函数 | 关键点 |
|------|--------|
| memset | 从后向前填充，使用 n-1 索引 |
| memcpy | 从低地址向高地址拷贝，不支持重叠 |
| memmove | 检测重叠，决定拷贝方向 |
| memcmp | 使用 unsigned char 比较 |

### 串口初始化流程

```
1. 禁用中断 (IER = 0x00)
       ↓
2. 启用 DLAB (LCR = 0x80)
       ↓
3. 设置波特率除数 (DLL=1, DLM=0 → 115200)
       ↓
4. 配置 8N1 (LCR = 0x03)
       ↓
5. 启用 FIFO (FCR = 0xC7)
       ↓
6. 设置调制解调器 (MCR = 0x0B)
```

### 三种模式差异

| 特性 | 实模式 (16-bit) | 保护模式 (32-bit) | 长模式 (64-bit) |
|------|----------------|------------------|-----------------|
| 寄存器 | AX, DX, SI | EAX, EDX, ESI | RAX, RDX, RSI |
| 指令前缀 | `bits 16` | `bits 32` | `bits 64` |
| 地址大小 | 16 位 | 32 位 | 64 位 |
| 串口实现 | 独立实现 | 独立实现 | 独立实现 |

---

## 常见问题

### Q: 为什么 memmove 需要检查重叠？

A: 当目标和源内存重叠时，从前向后拷贝会覆盖尚未拷贝的数据。需要检测重叠并从后向前拷贝。

### Q: 为什么串口初始化要六步？

A: UART 16550 的初始化必须按照特定顺序：禁用中断 → 设置波特率（需 DLAB）→ 配置格式 → 启用 FIFO → 设置调制解调器。

### Q: 为什么 Bootloader 需要三种模式的串口？

A: Bootloader 经历实模式、保护模式、长模式三个阶段。每个模式的寄存器大小和调用约定不同，需要独立实现。

### Q: 串口输出乱码怎么办？

A: 检查波特率配置（应该是 115200，除数为 1）和 LCR 配置（应该是 0x03 = 8N1）。

---

## 下一步

完成本阶段后，你将掌握：

- ✅ 标准库函数的实现方法
- ✅ 硬件驱动开发的基础
- ✅ 多模式汇编编程
- ✅ 主机端测试技巧

下一阶段（`stage/10`）我们将实现**格式化日志输出（printf）**，让调试更加方便。

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-16
