# PIO 模式读写实现 —— Stage 24 ATA 驱动实战指南

## 前言

上一篇文章我们实现了设备检测和 IDENTIFY 命令，现在驱动已经能够"看到"磁盘了。但光看到还不够，我们需要真正能够读写数据。

PIO（Programmed I/O）模式是最简单的数据传输方式，CPU 直接通过 IN/OUT 指令与磁盘交换数据。虽然效率不如 DMA，但对于操作系统开发初期来说完全够用，而且实现简单、易于调试。

说实话，PIO 读写是整个驱动最容易出问题的地方。你需要精确遵守 ATA 协议的时序要求，任何一个步骤出错都可能导致数据损坏或者系统卡死。所以这一篇文章我们会非常详细地讲解每一个步骤。

---

## LBA 寻址原理

在实现读写之前，我们需要先理解 LBA 寻址是怎么工作的。

### LBA vs CHS

早期磁盘使用 CHS（柱面-磁头-扇区）寻址：
```
地址 = (cylinder * heads_per_cylinder + head) * sectors_per_track + sector
```

这种方式直接对应磁盘物理结构，但计算复杂且容量受限。

LBA（Logical Block Addressing）把整个磁盘看作一个线性数组：
```
扇区 0, 扇区 1, 扇区 2, ..., 扇区 N
```

### LBA28 地址分解

LBA28 使用 28 位地址，支持最大 2^28 个扇区（约 128GB）。

地址需要分解到 4 个寄存器：

```
LBA_LO  (0x1F3): bits 7:0
LBA_MID (0x1F4): bits 15:8
LBA_HI  (0x1F5): bits 23:16
DEVICE (0x1F6): bits 27:24 (高4位)
```

示例代码：
```c
void setup_lba(uint16_t io_base, uint32_t lba, ata_device_t device) {
    // 设置 LBA 低、中、高字节
    outb(io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);

    // 设置 Device 寄存器（包含 LBA 高4位和设备选择）
    uint8_t dev_byte = ATA_DEV_LBA;  // 启用 LBA 模式
    if (device == ATA_DEVICE_SLAVE) {
        dev_byte |= ATA_DEV_SLAVE;
    }
    dev_byte |= (lba >> 24) & 0x0F;  // LBA27:24
    outb(io_base + ATA_REG_DEVICE, dev_byte);
}
```

---

## PIO 读取流程

现在我们来实现核心的 PIO 读取功能。

### 完整流程图

```
1. 选择设备
   ↓
2. 等待 BSY 清除
   ↓
3. 设置 LBA 地址
   ↓
4. 设置扇区数
   ↓
5. 发送 READ 命令
   ↓
6. 等待 BSY 清除
   ↓
7. 等待 DRQ 置位
   ↓
8. 读取 256 字数据
   ↓ (如果还有扇区)
9. 返回步骤 6
```

### ata_read_pio() 实现

```c
ata_result_t ata_read_pio(ata_controller_t* ctrl, ata_device_t device,
                          uint32_t lba, void* buffer, uint16_t sectors) {
    // 第一步：选择设备
    uint8_t dev_byte = ATA_DEV_LBA;
    if (device == ATA_DEVICE_SLAVE) {
        dev_byte |= ATA_DEV_SLAVE;
    }
    dev_byte |= (lba >> 24) & 0x0F;  // LBA 高位
    outb(ctrl->io_base + ATA_REG_DEVICE, dev_byte);

    // 等待 400ns
    ata_delay();

    // 第二步：等待 BSY 清除
    ata_result_t result = ata_wait_bsy(ctrl->io_base);
    if (result != ATA_OK) return result;

    // 第三步：设置 LBA 地址
    outb(ctrl->io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(ctrl->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(ctrl->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);

    // Device 寄存器已经设置过了

    // 第四步：设置扇区数（注意 256 的坑）
    uint8_t sec_count = (sectors == 256) ? 0 : sectors;
    outb(ctrl->io_base + ATA_REG_SECCOUNT, sec_count);

    // 第五步：发送读命令
    outb(ctrl->io_base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    // 第六步：读取每个扇区
    uint16_t* buf16 = (uint16_t*)buffer;

    for (uint16_t sec = 0; sec < sectors; sec++) {
        // 等待 BSY 清除
        result = ata_wait_bsy(ctrl->io_base);
        if (result != ATA_OK) return result;

        // 等待 DRQ 置位
        result = ata_wait_drq(ctrl->io_base);
        if (result != ATA_OK) return result;

        // 读取 256 字（512 字节）
        for (int i = 0; i < 256; i++) {
            buf16[sec * 256 + i] = inw(ctrl->io_base + ATA_REG_DATA);
        }
    }

    return ATA_OK;
}
```

### 为什么要逐扇区等待

你可能想问：为什么不一次等待然后读取所有数据？

这是因为 ATA 协议规定每个扇区的传输是独立的。设备会在每个扇区传输后：
1. 清除 BSY 位
2. 置位 DRQ 位
3. 等待 CPU 读取

如果你不等 DRQ 就读取，会读到垃圾数据或者卡死。

---

## PIO 写入流程

写入流程与读取类似，但有一些关键差异。

### 写入流程图

```
1. 选择设备并设置地址
   ↓
2. 等待 BSY 清除
   ↓
3. 发送 WRITE 命令
   ↓
4. 等待 DRQ 置位
   ↓
5. 写入 256 字数据
   ↓ (如果还有扇区)
6. 返回步骤 4
   ↓
7. 刷新缓存
```

### ata_write_pio() 实现

```c
ata_result_t ata_write_pio(ata_controller_t* ctrl, ata_device_t device,
                           uint32_t lba, const void* buffer, uint16_t sectors) {
    // 选择设备并设置 LBA
    uint8_t dev_byte = ATA_DEV_LBA;
    if (device == ATA_DEVICE_SLAVE) {
        dev_byte |= ATA_DEV_SLAVE;
    }
    dev_byte |= (lba >> 24) & 0x0F;
    outb(ctrl->io_base + ATA_REG_DEVICE, dev_byte);

    ata_delay();

    // 等待 BSY 清除
    ata_result_t result = ata_wait_bsy(ctrl->io_base);
    if (result != ATA_OK) return result;

    // 设置 LBA 地址
    outb(ctrl->io_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(ctrl->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(ctrl->io_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);

    // 设置扇区数
    uint8_t sec_count = (sectors == 256) ? 0 : sectors;
    outb(ctrl->io_base + ATA_REG_SECCOUNT, sec_count);

    // 发送写命令
    outb(ctrl->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

    // 写入每个扇区
    const uint16_t* buf16 = (const uint16_t*)buffer;

    for (uint16_t sec = 0; sec < sectors; sec++) {
        // 等待 DRQ 置位
        result = ata_wait_drq(ctrl->io_base);
        if (result != ATA_OK) return result;

        // 写入 256 字
        for (int i = 0; i < 256; i++) {
            outw(ctrl->io_base + ATA_REG_DATA, buf16[sec * 256 + i]);
        }
    }

    // 刷新缓存 - 重要！
    ata_flush_cache(ctrl);

    return ATA_OK;
}
```

### 写入与读取的差异

1. **命令不同** - 写入使用 `ATA_CMD_WRITE_SECTORS (0x30)`
2. **数据方向** - 写入是 CPU 到设备
3. **需要刷新** - 写入后必须刷新缓存

### 缓存刷新函数

```c
ata_result_t ata_flush_cache(ata_controller_t* ctrl) {
    // 发送 FLUSH CACHE 命令
    outb(ctrl->io_base + ATA_REG_COMMAND, ATA_CMD_FLUSH_CACHE);

    // 等待完成
    return ata_wait_bsy(ctrl->io_base);
}
```

---

## 256 扇区边界问题

这是一个经典的 ATA 坑点。

### 问题

ATA 命令中扇区数是 8 位的，但 256 个扇区编码为 0：

```c
// 错误示例
uint8_t sec_count = sectors;  // 256 会变成 0！
outb(io_base + ATA_REG_SECCOUNT, sec_count);  // 实际上是 256
```

### 解决方案

```c
// 正确做法
uint8_t sec_count = (sectors == 256) ? 0 : sectors;
outb(io_base + ATA_REG_SECCOUNT, sec_count);
```

### 为什么要这样设计

这是 ATA 协议的历史遗留问题。早期设计者觉得：
- 0 表示 256 比增加一个 9 位寄存器更节省
- 256 是一个"足够大"的值

所以我们在代码中必须特殊处理这个边界情况。

---

## 公共 API 实现

现在我们需要把内部函数包装成公共 API。

### ata_read() 函数

```c
ata_result_t ata_read(int device, uint64_t lba,
                      void* buffer, uint16_t sectors) {
    // 参数验证
    if (device < 0 || device > 3) {
        return ATA_ERR_INVALID_PARAM;
    }
    if (buffer == NULL) {
        return ATA_ERR_INVALID_PARAM;
    }
    if (sectors == 0 || sectors > 256) {
        return ATA_ERR_INVALID_PARAM;
    }

    // 获取控制器和设备
    int ctrl_idx = ATA_DEV_TO_CONTROLLER(device);
    ata_device_t drive = ATA_DEV_TO_DRIVE(device);

    ata_controller_t* ctrl = &g_ata_controllers[ctrl_idx];
    if (!ctrl->initialized) {
        return ATA_ERR_NOT_INIT;
    }

    // 检查设备存在
    bool present = (drive == ATA_DEVICE_MASTER) ?
                   ctrl->master_present : ctrl->slave_present;
    if (!present) {
        return ATA_ERR_NO_DEVICE;
    }

    // 检查 LBA 范围
    if (lba >= ata_get_sector_count(device)) {
        return ATA_ERR_INVALID_PARAM;
    }

    // 调用内部函数
    ata_result_t result = ata_read_pio(ctrl, drive, (uint32_t)lba, buffer, sectors);

    // 更新统计
    if (result == ATA_OK) {
        ctrl->read_count++;
    } else {
        ctrl->error_count++;
    }

    return result;
}
```

### ata_write() 函数

```c
ata_result_t ata_write(int device, uint64_t lba,
                       const void* buffer, uint16_t sectors) {
    // 参数验证
    if (device < 0 || device > 3) {
        return ATA_ERR_INVALID_PARAM;
    }
    if (buffer == NULL) {
        return ATA_ERR_INVALID_PARAM;
    }
    if (sectors == 0 || sectors > 256) {
        return ATA_ERR_INVALID_PARAM;
    }

    // 获取控制器和设备
    int ctrl_idx = ATA_DEV_TO_CONTROLLER(device);
    ata_device_t drive = ATA_DEV_TO_DRIVE(device);

    ata_controller_t* ctrl = &g_ata_controllers[ctrl_idx];
    if (!ctrl->initialized) {
        return ATA_ERR_NOT_INIT;
    }

    // 检查设备存在
    bool present = (drive == ATA_DEVICE_MASTER) ?
                   ctrl->master_present : ctrl->slave_present;
    if (!present) {
        return ATA_ERR_NO_DEVICE;
    }

    // 检查 LBA 范围
    if (lba >= ata_get_sector_count(device)) {
        return ATA_ERR_INVALID_PARAM;
    }

    // 调用内部函数
    ata_result_t result = ata_write_pio(ctrl, drive, (uint32_t)lba, buffer, sectors);

    // 更新统计
    if (result == ATA_OK) {
        ctrl->write_count++;
    } else {
        ctrl->error_count++;
    }

    return result;
}
```

---

## 辅助函数

### ata_get_sector_count()

```c
uint64_t ata_get_sector_count(int device) {
    if (device < 0 || device > 3) {
        return 0;
    }

    // 这里需要返回之前检测的设备信息
    // 实际实现需要存储设备信息
    // 简化版：
    return 262144;  // QEMU 默认 128MB = 262144 扇区
}
```

### ata_error_string()

```c
const char* ata_error_string(ata_result_t result) {
    switch (result) {
        case ATA_OK:               return "OK";
        case ATA_ERR_NOT_INIT:     return "Not initialized";
        case ATA_ERR_TIMEOUT:      return "Timeout";
        case ATA_ERR_NO_DEVICE:    return "No device";
        case ATA_ERR_IO_ERROR:     return "I/O error";
        case ATA_ERR_INVALID_PARAM: return "Invalid parameter";
        case ATA_ERR_UNSUPPORTED:  return "Unsupported";
        default:                   return "Unknown error";
    }
}
```

---

## MBR 读取验证

现在我们可以测试读取功能了。最简单的测试是读取 MBR（主引导记录）。

### MBR 位置和结构

MBR 位于 LBA 0，结构如下：

```
偏移      大小    描述
0x000     446     引导代码
0x1BE     16      分区表项 1
0x1CE     16      分区表项 2
0x1DE     16      分区表项 3
0x1EE     16      分区表项 4
0x1FE     2       签名 (0xAA55)
```

### 测试代码

```c
void test_read_mbr(void) {
    uint8_t mbr[512];

    // 读取 MBR
    ata_result_t result = ata_read(ATA_PRIMARY_MASTER, 0, mbr, 1);

    if (result != ATA_OK) {
        klog_error("Failed to read MBR: %s\n", ata_error_string(result));
        return;
    }

    // 检查签名
    uint16_t signature = *(uint16_t*)&mbr[510];
    klog_info("MBR Signature: 0x%04X\n", signature);

    if (signature == 0xAA55) {
        klog_info("MBR signature valid!\n");
    } else {
        klog_error("MBR signature invalid!\n");
    }

    // 打印分区表
    for (int i = 0; i < 4; i++) {
        uint8_t* partition = &mbr[0x1BE + i * 16];
        uint8_t status = partition[0];
        uint8_t type = partition[4];
        uint32_t lba = *(uint32_t*)&partition[8];
        uint32_t sectors = *(uint32_t*)&partition[12];

        if (type != 0) {
            klog_info("Partition %d: Type=0x%02X, LBA=%u, Sectors=%u\n",
                      i, type, lba, sectors);
        }
    }
}
```

---

## 常见问题排查

### 问题 1：读取超时

**症状**：`ata_wait_bsy()` 或 `ata_wait_drq()` 超时

**可能原因**：
1. LBA 地址超出范围
2. 设备未正确初始化
3. 命令发送错误

**调试方法**：
```c
// 添加详细日志
klog_debug("Reading LBA %u, %u sectors\n", lba, sectors);
klog_debug("Status before command: 0x%02X\n",
           inb(ctrl->io_base + ATA_REG_STATUS));
```

### 问题 2：数据全为零

**症状**：读取成功，但 buffer 全是 0

**可能原因**：
1. 磁盘镜像全是空的
2. DRQ 等待失败，读取了垃圾端口

**解决方案**：
```bash
# 在磁盘镜像中写入一些数据
dd if=/dev/urandom of=disk.img bs=512 count=1 conv=notrunc
```

### 问题 3：写入后读取失败

**症状**：写入成功，但读取返回旧数据

**原因**：忘记刷新缓存

**解决方案**：确保调用 `ata_flush_cache()`

---

## 到这里我们完成了什么

这篇文章我们实现了：
1. LBA 地址设置
2. PIO 模式读取扇区
3. PIO 模式写入扇区
4. 缓存刷新机制
5. 公共 API 封装
6. MBR 读取验证

现在我们的驱动可以读写磁盘数据了。下一篇文章我们会实现 LBA48 扩展，支持大容量磁盘。

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 设备检测与 IDENTIFY 命令](03_设备检测与IDENTIFY命令.md)  | [LBA48 扩展支持 →](05_LBA48扩展支持.md)

</div>
