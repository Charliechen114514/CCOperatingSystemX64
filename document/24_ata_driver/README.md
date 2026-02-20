# CCOS ATA 磁盘驱动 文档中心

本目录包含 CCOS Stage 24 - ATA 磁盘驱动开发的完整文档体系。

---

## 阶段概述

**Stage 24: ATA 磁盘驱动**

本阶段实现了 CCOS 的 ATA/IDE 磁盘驱动程序，为操作系统提供持久化存储能力。实现了基于 PIO 模式的磁盘读写、设备检测、LBA/LBA48 寻址以及中断驱动的异步 I/O。

### 核心成果

- **ATA 驱动核心** ([`kernel/driver/ata/ata.c`](../../kernel/driver/ata/ata.c))
  - PIO 模式读写操作
  - 设备检测与 IDENTIFY 解析
  - LBA/LBA48 寻址支持
  - 双通道支持（Primary/Secondary）

- **异步 I/O 支持** ([`kernel/driver/ata/ata.c`](../../kernel/driver/ata/ata.c))
  - 中断驱动模式
  - 操作队列管理
  - 回调机制

- **演示程序** ([`kernel/demo/ata/ata_demo.c`](../../kernel/demo/ata/ata_demo.c))
  - MBR 读取与验证
  - 设备信息显示
  - 读写性能测试

- **Bootloader 增强** ([`boot/bootloader.asm`](../../boot/bootloader.asm))
  - 页表映射扩展至 40MB
  - 支持更大内核 BSS 段

---

## 目录结构

```
kernel/
├── driver/
│   └── ata/
│       ├── ata.h              # 公共 API 接口
│       ├── ata.c              # 驱动核心实现
│       ├── ata_constants.h    # 硬件常量定义
│       └── ata_internal.h     # 内部结构体
└── demo/
    └── ata/
        ├── ata_demo.h         # 演示接口
        └── ata_demo.c         # 演示程序
boot/
└── bootloader.asm             # 页表映射扩展
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要磁盘驱动
- 设计决策（PIO vs DMA、同步 vs 异步）
- 架构设计与模块协作
- 实现细节（端口 I/O、状态轮询、IDENTIFY 解析）
- 常见陷阱与注意事项
- 未来改进方向

**适合**:
- 理解设计思路
- 学习系统架构
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- ATA 硬件基础（I/O 端口、寄存器、命令）
- ATA 子系统完整 API 参考
- 数据结构定义（ata_controller_t、ata_device_info_t）
- 常量定义（命令码、状态位、错误码）
- 异步 I/O 机制

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 问题诊断

**内容**:
- 设备未检测到问题
- 读写超时问题
- MBR 签名验证失败
- 中断驱动模式问题
- QEMU 磁盘镜像调试

每个问题按"症状-原因-解决方案"格式组织。

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

### 4. [调试工具指南](./调试工具指南.md) 工具使用

**内容**:
- QEMU 磁盘调试命令
- GDB 磁盘 I/O 调试
- ATA 端口监控技巧
- 演示程序使用

**适合**:
- 学习调试工具
- 查询具体命令
- 提升调试效率

---

## 快速开始

### 查看代码结构

1. **驱动接口** → 查看 [`kernel/driver/ata/ata.h`](../../kernel/driver/ata/ata.h)
2. **硬件常量** → 查看 [`kernel/driver/ata/ata_constants.h`](../../kernel/driver/ata/ata_constants.h)
3. **演示程序** → 查看 [`kernel/demo/ata/ata_demo.c`](../../kernel/demo/ata/ata_demo.c)

### 使用示例

```c
#include "driver/ata/ata.h"

// 内核初始化时调用
void kernel_init(void) {
    // 初始化 ATA 驱动（同步模式）
    ata_init();

    // 检查设备
    if (ata_device_exists(ATA_PRIMARY_MASTER)) {
        klog_info("ATA Primary Master detected\n");

        // 读取 MBR
        uint8_t mbr[512];
        ata_result_t result = ata_read(ATA_PRIMARY_MASTER, 0, mbr, 1);

        if (result == ATA_OK) {
            klog_info("MBR read successfully\n");
        }
    }
}

// 在其他地方使用
void read_disk_data(void) {
    uint8_t buffer[4096];  // 8 个扇区

    // 读取 8 个扇区
    ata_result_t result = ata_read(ATA_PRIMARY_MASTER, 1, buffer, 8);

    if (result != ATA_OK) {
        klog_error("Read failed: %s\n", ata_error_string(result));
    }
}
```

### 异步 I/O 示例

```c
// 异步读取回调
void my_read_callback(int device, uint64_t lba, uint16_t sectors,
                      void* buffer, ata_result_t result, void* context) {
    if (result == ATA_OK) {
        klog_info("Async read completed\n");
    }
    // 处理数据...
}

// 启动异步读取
void read_async(void) {
    uint8_t buffer[4096];

    // 启动异步操作
    ata_read_async(ATA_PRIMARY_MASTER, 1, buffer, 8,
                   my_read_callback, NULL);

    // 操作在后台进行...
}
```

---

## 与前一阶段对比

| 特性 | Stage 23 (用户进程) | Stage 24 (ATA 驱动) |
|------|---------------------|---------------------|
| 存储能力 | 仅内存 | + 磁盘持久化 |
| I/O 模式 | 仅字符设备 | + 块设备驱动 |
| 进程容量 | 受内存限制 | 可扩展到磁盘 |
| 异步 I/O | 无 | 中断驱动队列 |
| 页表映射 | 2MB | 40MB |
| 新增文件 | - | 6 个核心文件 |

---

## 技术亮点

### 1. PIO 模式实现

```
┌─────────────────────────────────────────────────────────┐
│                    ATA PIO 读写流程                      │
├─────────────────────────────────────────────────────────┤
│  1. 选择设备      → 写入 Device 寄存器                   │
│  2. 等待 400ns    → ATA 规范要求的延迟                  │
│  3. 设置 LBA      → 写入 LBA_LO/MID/HI 寄存器            │
│  4. 设置扇区数    → 写入 Sector Count 寄存器            │
│  5. 发送命令      → 写入 Command 寄存器                  │
│  6. 等待 BSY 清除  → 轮询 Status 寄存器                 │
│  7. 等待 DRQ 置位  → 轮询 Status 寄存器                 │
│  8. 传输数据      → 读写 Data 端口 (256 字)             │
└─────────────────────────────────────────────────────────┘
```

### 2. 模块化架构

```
┌─────────────────────────────────────────────────────────┐
│                 ATA 驱动子系统                          │
│  - ata_init()                                           │
│  - ata_read() / ata_write()                             │
│  - ata_read_async() / ata_write_async()                 │
└──────────────────────┬──────────────────────────────────┘
                       │
             ┌─────────┴─────────┐
             │                   │
        ┌────▼────┐         ┌────▼────┐
        │  PIO 层  │         │ 异步队列 │
        │  读写实现 │         │  中断驱动 │
        └──────────┘         └─────────┘
             │
        ┌────▼────┐
        │ 硬件抽象 │
        │ I/O 端口 │
        └─────────┘
```

### 3. 设备信息解析

```c
typedef struct ata_device_info {
    bool exists;                // 设备存在
    bool lba_supported;         // LBA 寻址
    bool lba48_supported;       // LBA48 扩展
    bool dma_supported;         // DMA 模式

    char model[41];             // 型号
    char serial[21];            // 序列号
    char firmware[9];           // 固件版本

    uint64_t lba_sectors;       // LBA28 扇区数
    uint64_t lba48_sectors;     // LBA48 扇区数
    uint32_t cylinders;         // CHS 参数
    uint32_t heads;
    uint32_t sectors_per_track;
} ata_device_info_t;
```

### 4. 双通道支持

```
┌─────────────────────────────────────────────────────────┐
│                   ATA 控制器架构                         │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌──────────────────┐        ┌──────────────────┐      │
│  │ Primary Channel  │        │ Secondary Channel│      │
│  │  I/O: 0x1F0      │        │  I/O: 0x170      │      │
│  │  IRQ: 14         │        │  IRQ: 15         │      │
│  ├──────────────────┤        ├──────────────────┤      │
│  │  Master (dev 0)  │        │  Master (dev 2)  │      │
│  │  Slave  (dev 1)  │        │  Slave  (dev 3)  │      │
│  └──────────────────┘        └──────────────────┘      │
│                                                         │
└─────────────────────────────────────────────────────────┘
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

- **阶段**: Stage 24
- **分支**: `stage/24_ata_driver`
- **提交**: `6197a48` - ata driver ok
- **日期**: 2026-02-19
- **作者**: Charliechen114514

---

## 相关资源

### 项目文档
- [../PROGRESS.md](../PROGRESS.md) - 项目进度
- [../23_usr_proc/README.md](../23_usr_proc/) - 上一阶段文档

### 源码文件
- [`kernel/driver/ata/ata.h`](../../kernel/driver/ata/ata.h)
- [`kernel/driver/ata/ata_constants.h`](../../kernel/driver/ata/ata_constants.h)
- [`kernel/driver/ata/ata_internal.h`](../../kernel/driver/ata/ata_internal.h)
- [`kernel/driver/ata/ata.c`](../../kernel/driver/ata/ata.c)
- [`kernel/demo/ata/ata_demo.c`](../../kernel/demo/ata/ata_demo.c)

### 外部参考
- [ATA/ATAPI 规范](https://www.t13.org/)
- [OSDev.org - ATA](https://wiki.osdev.org/ATA)
- [OSDev.org - ATA_PIO_Mode](https://wiki.osdev.org/ATA_PIO_Mode)
- [Intel IDE Controller Specification](https://www.intel.com/content/dam/www/public/us/en/documents/specification-updates/ide-ctrl-spec-update.pdf)

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-20
