# ATA 硬件基础与 I/O 端口 —— Stage 24 ATA 驱动实战指南

## 前言

上一篇文章我们讲了为什么要实现磁盘驱动，以及一些设计决策。现在到了真正深入理解 ATA 硬件的时候了。

说实话，ATA 协议这块儿的内容比我们之前接触的任何东西都要底层。我们不再是在操作抽象的数据结构，而是直接与硬件寄存器打交道。你写的每一个 `outb()` 都会直接影响到磁盘控制器的行为，这种感觉很奇妙，但也意味着更多的责任。

理解 ATA 硬件基础是实现驱动的前提。如果你不知道 I/O 端口是怎么工作的，不清楚状态寄存器的每一位含义，那写出来的代码大概率会出问题。所以这篇文章虽然不涉及写太多代码，但非常重要。

---

## 环境说明

在开始之前，我们需要准备一些测试环境。

### QEMU 磁盘镜像

首先创建一个测试用的磁盘镜像：

```bash
# 创建一个 128MB 的原始磁盘镜像
dd if=/dev/zero of=disk.img bs=1M count=128

# 或者用 qemu-img 创建 qcow2 格式（更节省空间）
qemu-img create -f qcow2 disk.qcow2 128M
```

### QEMU 启动参数

启动 QEMU 时需要添加磁盘参数：

```bash
qemu-system-x86_64 \
    -drive file=disk.img,format=raw,index=0,media=disk \
    -kernel kernel.elf \
    -serial stdio \
    -m 128M
```

关键参数说明：
- `-drive` 添加磁盘驱动器
- `file=disk.img` 磁盘镜像文件
- `format=raw` 镜像格式（raw 或 qcow2）
- `index=0` 驱动器编号（0 = Primary Master）
- `media=disk` 介质类型（disk 或 cdrom）

### 开发工具

确保你有这些工具：
- `dd` - 创建磁盘镜像
- `qemu-img` - QEMU 镜像管理工具
- `hexdump` - 查看磁盘内容
- `gdb` - 调试驱动代码

---

## ATA 控制器架构详解

让我们先从整体上理解 ATA 控制器是怎么组织的。

### 通道与设备

x86 系统中的 ATA 布局：

```
┌─────────────────────────────────────────────────────────┐
│                      x86 CPU                             │
│                  (I/O 端口访问)                          │
└────────────┬────────────────────────────────────────────┘
             │
    ┌────────┴────────┐
    │                 │
┌───▼────┐      ┌────▼───┐
│Primary │      │Secondary│
│Channel │      │Channel  │
├────────┤      ├─────────┤
│I/O:0x1F0│    │I/O:0x170│
│IRQ: 14 │      │IRQ: 15  │
├────────┤      ├─────────┤
│Master  │      │Master   │
│Slave   │      │Slave    │
└────────┘      └─────────┘
 0/1             2/3  <- 设备编号
```

**Primary Channel** (主通道)
- I/O 基地址：0x1F0
- 控制端口：0x3F6
- IRQ：14

**Secondary Channel** (从通道)
- I/O 基地址：0x170
- 控制端口：0x376
- IRQ：15

**设备编号**：
- 0 = Primary Master
- 1 = Primary Slave
- 2 = Secondary Master
- 3 = Secondary Slave

### 为什么需要两个通道

早期的 PC 有两个 IDE 通道是因为：
1. **支持更多设备** - 每个通道两个设备，总共四个
2. **并行操作** - 两个通道可以同时工作（理论上）
3. **兼容性** - CD-ROM 通常放在 Secondary 通道

在现代系统中，SATA 接口继承了这种编号方式。

---

## I/O 端口映射

现在我们来看具体的 I/O 端口。这是我们与磁盘控制器通信的唯一途径。

### Primary 通道端口表

| 端口 | 读操作 | 写操作 | 数据位 | 描述 |
|------|--------|--------|--------|------|
| 0x1F0 | Data | Data | 16-bit | 数据寄存器 |
| 0x1F1 | Error | Features | 8-bit | 错误/特性寄存器 |
| 0x1F2 | SecCount | SecCount | 8-bit | 扇区计数 |
| 0x1F3 | LBA Lo | LBA Lo | 8-bit | LBA 低字节 |
| 0x1F4 | LBA Mid | LBA Mid | 8-bit | LBA 中字节 |
| 0x1F5 | LBA Hi | LBA Hi | 8-bit | LBA 高字节 |
| 0x1F6 | - | Device | 8-bit | 设备/头寄存器 |
| 0x1F7 | Status | Command | 8-bit | 状态/命令寄存器 |

### Secondary 通道端口表

| 端口 | 读操作 | 写操作 | 数据位 | 描述 |
|------|--------|--------|--------|------|
| 0x170 | Data | Data | 16-bit | 数据寄存器 |
| 0x171 | Error | Features | 8-bit | 错误/特性寄存器 |
| 0x172 | SecCount | SecCount | 8-bit | 扇区计数 |
| 0x173 | LBA Lo | LBA Lo | 8-bit | LBA 低字节 |
| 0x174 | LBA Mid | LBA Mid | 8-bit | LBA 中字节 |
| 0x175 | LBA Hi | LBA Hi | 8-bit | LBA 高字节 |
| 0x176 | - | Device | 8-bit | 设备/头寄存器 |
| 0x177 | Status | Command | 8-bit | 状态/命令寄存器 |

### 控制端口

| 端口 | 读操作 | 写操作 | 描述 |
|------|--------|--------|------|
| 0x3F6/0x376 | AltStatus | DeviceCtl | 交替状态/设备控制 |

**AltStatus** 和 **Status** 的值相同，但读取 AltStatus 不会影响中断状态。

---

## 寄存器详解

让我们详细看看每个寄存器的用途。

### Data 寄存器 (0x1F0/0x170)

这是数据传输的门户，用于读写扇区数据。

**重要特性**：
- 16 位寄存器，必须使用 `inw()`/`outw()` 操作
- 每次读写 2 字节，一个扇区需要读写 256 次
- 读取时等待 DRQ 位置位，写入时等待 BSY 清除

```c
// 读取一个扇区的数据（512 字节）
uint16_t* buffer = (uint16_t*)sector_buffer;
for (int i = 0; i < 256; i++) {
    buffer[i] = inw(io_base + ATA_REG_DATA);
}
```

### Error 寄存器 (0x1F1/0x171)

当 Status 寄存器的 ERR 位置位时，这个寄存器包含错误详情。

| 位 | 名称 | 描述 |
|----|------|------|
| 7 | BBK | 坏块检测 |
| 6 | UNC | 无法纠正的数据错误 |
| 5 | MC | 介质更换 |
| 4 | IDNF | ID 未找到 |
| 3 | MCR | 介质更换请求 |
| 2 | ABRT | 命令中止 |
| 1 | TK0NF | 磁道 0 未找到 |
| 0 | AMNF | 地址标记未找到 |

### SecCount 寄存器 (0x1F2/0x172)

指定要读写的扇区数量。

**重要特性**：
- 值为 0 表示 256 个扇区（这是个坑！）
- LBA48 模式下需要写入两次
- 实际传输的扇区数可能少于请求值

```c
// 设置扇区数时的坑
uint8_t sec_count = (sectors == 256) ? 0 : sectors;
outb(io_base + ATA_REG_SECCOUNT, sec_count);
```

### LBA 寄存器 (0x1F3-0x1F5/0x173-0x175)

这三个寄存器存储 LBA 地址的低 24 位：

```
LBA_LO:  bits 7:0
LBA_MID: bits 15:8
LBA_HI:  bits 23:16
```

LBA 的高 4 位（bits 27:24）存储在 Device 寄存器中。

### Device 寄存器 (0x1F6/0x176)

这个寄存器做两件事：选择设备和存储 LBA 高位。

| 位 | 名称 | 描述 |
|----|------|------|
| 7 | Reserved | 保留（必须为 1） |
| 6 | nIEN | 禁用中断（1 = 禁用） |
| 5 | SRST | 软件复位 |
| 4 | LBA | 使用 LBA 模式 |
| 3:0 | DEV | LBA27:24 或 设备号 |

**设备选择**：
```c
// 选择 Master，使用 LBA 模式
outb(io_base + ATA_REG_DEVICE, 0xE0);  // 11100000b

// 选择 Slave，使用 LBA 模式
outb(io_base + ATA_REG_DEVICE, 0xF0);  // 11110000b
```

常量定义：
```c
#define ATA_DEV_MASTER        0x00    // Master 设备
#define ATA_DEV_SLAVE         0x10    // Slave 设备
#define ATA_DEV_LBA           0x40    // LBA 模式
```

### Status 寄存器 (0x1F7/0x177) - 读

这是最重要的寄存器，告诉我们设备的当前状态。

| 位 | 名称 | 描述 |
|----|------|------|
| 7 | BSY | 设备忙 |
| 6 | DRDY | 设备就绪 |
| 5 | DF | 设备故障 |
| 4 | DSC | 寻道完成 |
| 3 | DRQ | 数据请求（可传输） |
| 2 | CORR | 已纠正数据 |
| 1 | IDX | 索引信号 |
| 0 | ERR | 错误发生 |

### Command 寄存器 (0x1F7/0x177) - 写

向这个寄存器写入命令码来执行操作。

常用命令：
```c
#define ATA_CMD_IDENTIFY          0xEC    // 设备识别
#define ATA_CMD_READ_SECTORS      0x20    // 读扇区
#define ATA_CMD_WRITE_SECTORS     0x30    // 写扇区
#define ATA_CMD_FLUSH_CACHE       0xE7    // 刷新缓存
#define ATA_CMD_READ_SECTORS_EXT  0x24    // 读扇区扩展
#define ATA_CMD_WRITE_SECTORS_EXT 0x34    // 写扇区扩展
```

---

## 状态检查的正确顺序

与 ATA 设备通信时，状态检查的顺序非常重要。这是很多新手踩坑的地方。

### 正确的状态轮询流程

```
1. 等待 BSY 清除
   ↓
2. 检查 ERR 位
   ↓
3. 等待 DRQ 置位（对于数据传输命令）
   ↓
4. 传输数据
```

### 为什么不能直接检查 DRQ

因为 BSY 位置位时，其他状态位都是无效的。你必须先等 BSY 清除。

```c
// 错误的做法
uint8_t status = inb(io_base + ATA_REG_STATUS);
if (status & ATA_STATUS_DRQ) {  // 可能收到错误的 DRQ！
    // 传输数据
}

// 正确的做法
while (inb(io_base + ATA_REG_STATUS) & ATA_STATUS_BSY) {
    // 等待 BSY 清除
}
uint8_t status = inb(io_base + ATA_REG_STATUS);
if (status & ATA_STATUS_ERR) {
    // 处理错误
}
if (status & ATA_STATUS_DRQ) {
    // 传输数据
}
```

---

## 400ns 延迟的必要性

ATA 规范规定，在某些操作后必须等待至少 400ns。这是硬件特性，不能跳过。

### 需要延迟的操作

1. 选择设备后
2. 写入某些寄存器后

### 实现方式

最简单的方法是读取未使用的端口 0x80：

```c
static inline void ata_delay(void) {
    // 每次读取约 100ns，读 4 次 = 400ns
    for (int i = 0; i < 4; i++) {
        inb(0x80);
    }
}
```

### 为什么选择端口 0x80

端口 0x80 是传统上的"延迟端口"，用于实现精确的短延迟。读取它没有任何副作用，但会消耗一定的 CPU 周期。

---

## 创建项目目录结构

现在我们了解了硬件基础，可以开始创建代码结构了。

### 目录结构

```
kernel/
├── driver/
│   └── ata/
│       ├── ata.h              # 公共 API
│       ├── ata.c              # 核心实现
│       ├── ata_constants.h    # 硬件常量
│       └── ata_internal.h     # 内部结构
```

### 第一步：创建常量头文件

创建 `kernel/driver/ata/ata_constants.h`：

```c
#pragma once

/* I/O 端口定义 */
#define ATA_PRIMARY_IO        0x1F0
#define ATA_PRIMARY_CTRL      0x3F6
#define ATA_SECONDARY_IO      0x170
#define ATA_SECONDARY_CTRL    0x376

/* 寄存器偏移 */
#define ATA_REG_DATA          0x00
#define ATA_REG_ERROR         0x01
#define ATA_REG_FEATURES      0x01
#define ATA_REG_SECCOUNT      0x02
#define ATA_REG_LBA_LO        0x03
#define ATA_REG_LBA_MID       0x04
#define ATA_REG_LBA_HI        0x05
#define ATA_REG_DEVICE        0x06
#define ATA_REG_STATUS        0x07
#define ATA_REG_COMMAND       0x07

/* 状态位 */
#define ATA_STATUS_BSY        0x80
#define ATA_STATUS_DRDY       0x40
#define ATA_STATUS_DF         0x20
#define ATA_STATUS_DSC        0x10
#define ATA_STATUS_DRQ        0x08
#define ATA_STATUS_CORR       0x04
#define ATA_STATUS_IDX        0x02
#define ATA_STATUS_ERR        0x01

/* 设备位 */
#define ATA_DEV_MASTER        0x00
#define ATA_DEV_SLAVE         0x10
#define ATA_DEV_LBA           0x40

/* 命令 */
#define ATA_CMD_IDENTIFY      0xEC
#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30

/* 常量 */
#define ATA_SECTOR_SIZE       512
#define ATA_TIMEOUT_MS        5000
```

---

## 验证环境

在开始写驱动代码之前，我们先验证一下环境是否正确。

### 简单的端口测试

```c
// 测试代码：读取 Primary 通道的状态
void test_ata_ports(void) {
    uint16_t primary_base = ATA_PRIMARY_IO;

    // 读取状态寄存器
    uint8_t status = inb(primary_base + ATA_REG_STATUS);

    klog_info("ATA Primary Status: 0x%02X\n", status);
    klog_info("  BSY: %d\n", !!(status & ATA_STATUS_BSY));
    klog_info("  DRDY: %d\n", !!(status & ATA_STATUS_DRDY));
    klog_info("  DRQ: %d\n", !!(status & ATA_STATUS_DRQ));
    klog_info("  ERR: %d\n", !!(status & ATA_STATUS_ERR));
}
```

### 预期输出

如果 QEMU 正确配置了磁盘，你应该看到：

```
ATA Primary Status: 0x50
  BSY: 0
  DRDY: 1
  DRQ: 0
  ERR: 0
```

- DRDY = 1 表示设备就绪
- BSY = 0 表示设备不忙
- ERR = 0 表示没有错误

如果看到 0xFF 或者全是 1，可能是：
1. QEMU 没有添加磁盘
2. I/O 端口配置错误
3. 没有在 QEMU 中运行

---

## 到这里我们完成了什么

这篇文章我们讲解了：
- ATA 控制器的双通道架构
- 完整的 I/O 端口映射表
- 每个寄存器的详细用途
- 状态检查的正确顺序
- 400ns 延迟的必要性
- 项目目录结构创建
- 环境验证方法

现在我们对"什么"和"为什么"有了清晰的理解，下一篇文章我们会开始实现设备检测功能。

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 为什么要实现磁盘驱动](01_为什么要实现磁盘驱动.md)  | [设备检测与 IDENTIFY 命令 →](03_设备检测与IDENTIFY命令.md)

</div>
