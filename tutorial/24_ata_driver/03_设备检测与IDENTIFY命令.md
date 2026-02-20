# 设备检测与 IDENTIFY 命令 —— Stage 24 ATA 驱动实战指南

## 前言

上一篇文章我们学习了 ATA 硬件的基础知识，了解了 I/O 端口和寄存器的作用。现在到了真正开始写代码的时候了。

说实话，设备检测是整个 ATA 驱动中最关键的一步。如果检测失败，后面所有的读写操作都无法进行。而且设备检测涉及很多 ATA 协议的细节，稍有不慎就会导致程序卡死或者误判设备存在。

这一步我们不仅要实现设备存在检测，还要通过 IDENTIFY 命令获取设备的详细信息。这个过程就像是在和磁盘设备"打招呼"，问问它"你是谁"、"你能做什么"。准备好了吗？我们开始。

---

## 设备检测流程概述

在深入代码之前，先让我们理解一下设备检测的完整流程。

### 检测步骤

```
1. 选择设备
   ↓
2. 等待 400ns（必须！）
   ↓
3. 发送 IDENTIFY 命令
   ↓
4. 检查状态寄存器
   ↓
5. 如果设备存在，读取 IDENTIFY 数据
   ↓
6. 解析设备信息
```

### 关键点

1. **400ns 延迟** - 这是 ATA 规范的要求，不能跳过
2. **状态判断** - 根据状态寄存器的值判断设备是否存在
3. **IDENTIFY 数据** - 如果存在，读取 256 字的设备信息

---

## 设备存在检测原理

### IDENTIFY 命令的作用

IDENTIFY 命令（0xEC）是 ATA 协议中最重要的命令之一。它的作用是：
- 确认设备是否存在
- 获取设备能力信息
- 读取设备标识信息

### 设备响应判断

发送 IDENTIFY 命令后，设备可能有几种响应：

| Status 值 | 含义 | 处理方式 |
|-----------|------|----------|
| 0x00 | 无设备 | 设备不存在 |
| 有 ERR 位 | ATAPI 设备（如光驱） | 暂不支持 |
| BSY 位后被 DRQ | ATA 设备存在 | 读取 IDENTIFY 数据 |

---

## 实现设备选择

检测设备的第一步是选择我们要检测的设备。

### ata_select_device() 函数

```c
ata_result_t ata_select_device(ata_controller_t* ctrl, ata_device_t device) {
    // 构建设备选择字节
    uint8_t dev_byte = ATA_DEV_LBA;  // 使用 LBA 模式

    if (device == ATA_DEVICE_SLAVE) {
        dev_byte |= ATA_DEV_SLAVE;
    }

    // 写入 Device 寄存器
    outb(ctrl->io_base + ATA_REG_DEVICE, dev_byte);

    // 等待 400ns - ATA 规范要求
    ata_delay();

    return ATA_OK;
}
```

### 为什么要设置 LBA 位

虽然我们只是在选择设备，但设置 LBA 位是习惯做法。因为：
1. 现代磁盘都支持 LBA
2. 后续操作会使用 LBA 寻址
3. 不会造成任何问题

### 400ns 延迟实现

```c
static inline void ata_delay(void) {
    // 读取端口 0x80 实现约 400ns 延迟
    for (int i = 0; i < 4; i++) {
        inb(0x80);
    }
}
```

这个延迟必须存在！我当年就因为省掉这几行代码，调试了一整天才发现问题。

---

## 实现设备存在检测

现在我们来实现核心的设备检测函数。

### ata_device_present() 函数

```c
bool ata_device_present(ata_controller_t* ctrl, ata_device_t device) {
    // 第一步：选择设备
    uint8_t dev_byte = ATA_DEV_LBA;
    if (device == ATA_DEVICE_SLAVE) {
        dev_byte |= ATA_DEV_SLAVE;
    }
    outb(ctrl->io_base + ATA_REG_DEVICE, dev_byte);

    // 等待 400ns - 必须！
    ata_delay();

    // 第二步：发送 IDENTIFY 命令
    outb(ctrl->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    // 第三步：立即检查状态
    uint8_t status = inb(ctrl->io_base + ATA_REG_STATUS);

    // 如果状态为 0，表示无设备
    if (status == 0) {
        return false;
    }

    // 如果 ERR 位置位，可能是 ATAPI 设备（不支持）
    if (status & ATA_STATUS_ERR) {
        return false;
    }

    // 第四步：等待 BSY 清除
    ata_result_t result = ata_wait_bsy(ctrl->io_base);
    if (result != ATA_OK) {
        return false;  // 超时，设备可能不存在
    }

    // 第五步：再次检查状态
    status = inb(ctrl->io_base + ATA_REG_STATUS);

    // 检查 ERR 位
    if (status & ATA_STATUS_ERR) {
        return false;
    }

    // 检查 DRQ 位 - 如果置位说明有数据可读
    if (status & ATA_STATUS_DRQ) {
        return true;  // 设备存在！
    }

    return false;
}
```

### 为什么要立即检查状态

发送 IDENTIFY 命令后立即读取状态寄存器是一个技巧：
- 如果设备不存在，状态寄存器会返回 0
- 如果是 ATAPI 设备，ERR 位会被设置
- 这样可以快速判断设备类型

### ata_wait_bsy() 实现

```c
ata_result_t ata_wait_bsy(uint16_t io_base) {
    uint32_t timeout = 100000;  // 超时计数

    while (timeout-- > 0) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if (!(status & ATA_STATUS_BSY)) {
            return ATA_OK;  // BSY 清除，设备就绪
        }
        __asm__ volatile("pause");  // 减少功耗
    }

    return ATA_ERR_TIMEOUT;  // 超时
}
```

超时时间设置为 100000 次循环，大约是几毫秒。对于正常的设备，BSY 位会在很短时间内清除。

---

## 读取 IDENTIFY 数据

确认设备存在后，我们需要读取 IDENTIFY 数据。这是 256 个字（512 字节）的设备信息。

### ata_identify_device() 函数

```c
ata_result_t ata_identify_device(ata_controller_t* ctrl, ata_device_t device,
                                 ata_device_info_t* info) {
    // 清空 info 结构
    memset(info, 0, sizeof(ata_device_info_t));

    // 选择设备
    ata_select_device(ctrl, device);

    // 发送 IDENTIFY 命令
    outb(ctrl->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    // 等待设备响应
    uint8_t status = inb(ctrl->io_base + ATA_REG_STATUS);
    if (status == 0) return ATA_ERR_NO_DEVICE;
    if (status & ATA_STATUS_ERR) return ATA_ERR_IO_ERROR;

    // 等待 BSY 清除
    ata_result_t result = ata_wait_bsy(ctrl->io_base);
    if (result != ATA_OK) return result;

    // 等待 DRQ 置位
    result = ata_wait_drq(ctrl->io_base);
    if (result != ATA_OK) return result;

    // 读取 256 字数据
    uint16_t identify_data[256];
    uint16_t* buf16 = (uint16_t*)identify_data;

    for (int i = 0; i < 256; i++) {
        buf16[i] = inw(ctrl->io_base + ATA_REG_DATA);
    }

    // 解析数据（下一节详细讲解）
    // ...

    info->exists = true;
    return ATA_OK;
}
```

### ata_wait_drq() 实现

```c
ata_result_t ata_wait_drq(uint16_t io_base) {
    uint32_t timeout = 100000;

    while (timeout-- > 0) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);

        // 检查错误
        if (status & ATA_STATUS_ERR) {
            return ATA_ERR_IO_ERROR;
        }

        // 检查 BSY 和 DRQ
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) {
            return ATA_OK;  // 可以传输数据
        }

        __asm__ volatile("pause");
    }

    return ATA_ERR_TIMEOUT;
}
```

---

## 解析 IDENTIFY 数据

IDENTIFY 数据是以字（16-bit）为单位组织的，其中包含了设备的各种信息。

### IDENTIFY 数据结构

关键字的偏移：

```c
#define ATA_IDENT_CAPABILITIES  49  // 能力位
#define ATA_IDENT_SERIAL        10  // 序列号（字 10-19）
#define ATA_IDENT_FIRMWARE      23  // 固件版本（字 23-26）
#define ATA_IDENT_MODEL         27  // 型号（字 27-46）
#define ATA_IDENT_LBA_SECTORS   60  // LBA 扇区数（字 60-61）
#define ATA_IDENT_LBA48_SECTORS 100 // LBA48 扇区数（字 100-103）
```

### 解析能力位

```c
// 解析能力位
uint16_t caps = identify_data[ATA_IDENT_CAPABILITIES];

info->lba_supported = (caps & ATA_CAP_LBA) != 0;
info->dma_supported = (caps & ATA_CAP_DMA) != 0;
info->lba48_supported = (caps & ATA_CAP_LBA48) != 0;
```

能力位定义：
```c
#define ATA_CAP_LBA    0x0200  // Bit 9: LBA 支持
#define ATA_CAP_DMA    0x0100  // Bit 8: DMA 支持
#define ATA_CAP_LBA48  0x0400  // Bit 10: LBA48 支持
```

### 解析扇区数

```c
// LBA28 扇区数（字 60-61）
info->lba_sectors = ((uint64_t)identify_data[61] << 16) |
                    identify_data[60];

// LBA48 扇区数（字 100-103）
if (info->lba48_supported) {
    info->lba48_sectors = ((uint64_t)identify_data[103] << 48) |
                         ((uint64_t)identify_data[102] << 32) |
                         ((uint64_t)identify_data[101] << 16) |
                         identify_data[100];
} else {
    info->lba48_sectors = 0;
}
```

---

## 字符串转换的坑

IDENTIFY 数据中的字符串是以大端序字存储的，这是很多新手踩坑的地方。

### 问题示例

```
原始字符串: "QEMU HARDDISK"
存储格式（大端序字）:
  0x5145 0x5551 0x2048 0x5241 0x4444 0x4953 0x4B20

直接读取会得到:
  "EMQ HARDDISS"  ← 完全错了！
```

### 正确的转换函数

```c
void ata_identify_string_to_c(const uint16_t* src, char* dest, int words) {
    for (int i = 0; i < words; i++) {
        uint16_t word = src[i];
        // 高字节在前，低字节在后
        dest[i * 2] = (char)(word >> 8);
        dest[i * 2 + 1] = (char)(word & 0xFF);
    }
    dest[words * 2] = '\0';

    // 去除尾部空格
    int len = words * 2;
    while (len > 0 && dest[len - 1] == ' ') {
        dest[--len] = '\0';
    }
}
```

### 使用示例

```c
// 解析型号（20 个字 = 40 字节）
ata_identify_string_to_c(&identify_data[ATA_IDENT_MODEL],
                         info->model, 20);

// 解析序列号（10 个字 = 20 字节）
ata_identify_string_to_c(&identify_data[ATA_IDENT_SERIAL],
                         info->serial, 10);

// 解析固件版本（4 个字 = 8 字节）
ata_identify_string_to_c(&identify_data[ATA_IDENT_FIRMWARE],
                         info->firmware, 4);
```

---

## 完整的初始化流程

现在我们可以把设备检测集成到初始化流程中了。

### ata_init() 函数

```c
// 全局控制器数组
static ata_controller_t g_ata_controllers[2];

int ata_init(void) {
    // 初始化 Primary 控制器
    g_ata_controllers[0].io_base = ATA_PRIMARY_IO;
    g_ata_controllers[0].ctrl_base = ATA_PRIMARY_CTRL;
    g_ata_controllers[0].irq = 14;
    g_ata_controllers[0].initialized = false;

    // 初始化 Secondary 控制器
    g_ata_controllers[1].io_base = ATA_SECONDARY_IO;
    g_ata_controllers[1].ctrl_base = ATA_SECONDARY_CTRL;
    g_ata_controllers[1].irq = 15;
    g_ata_controllers[1].initialized = false;

    // 检测 Primary 通道
    ata_controller_t* primary = &g_ata_controllers[0];
    primary->master_present = ata_device_present(primary, ATA_DEVICE_MASTER);
    primary->slave_present = ata_device_present(primary, ATA_DEVICE_SLAVE);
    primary->initialized = true;

    klog_info("[ATA] Primary Master: %s\n",
              primary->master_present ? "Present" : "Not Present");
    klog_info("[ATA] Primary Slave: %s\n",
              primary->slave_present ? "Present" : "Not Present");

    // 检测 Secondary 通道
    ata_controller_t* secondary = &g_ata_controllers[1];
    secondary->master_present = ata_device_present(secondary, ATA_DEVICE_MASTER);
    secondary->slave_present = ata_device_present(secondary, ATA_DEVICE_SLAVE);
    secondary->initialized = true;

    klog_info("[ATA] Secondary Master: %s\n",
              secondary->master_present ? "Present" : "Not Present");
    klog_info("[ATA] Secondary Slave: %s\n",
              secondary->slave_present ? "Present" : "Not Present");

    return 0;
}
```

---

## 常见问题排查

### 问题 1：所有设备都显示不存在

**症状**：
```
[ATA] Primary Master: Not Present
[ATA] Primary Slave: Not Present
```

**可能原因**：
1. QEMU 没有添加磁盘镜像
2. I/O 端口配置错误
3. 忘记添加 400ns 延迟

**解决方案**：
```bash
# 检查 QEMU 启动参数
qemu-system-x86_64 \
    -drive file=disk.img,format=raw,index=0,media=disk \
    -kernel kernel.elf
```

### 问题 2：字符串显示乱码

**症状**：
```
Model: "EMQ HARDDISS"
```

**原因**：字符串字节序转换错误

**解决方案**：确保使用 `ata_identify_string_to_c()` 函数

### 问题 3：检测超时

**症状**：设备检测卡在 `ata_wait_bsy()`

**可能原因**：
1. 设备选择错误
2. 命令发送错误
3. 硬件问题

**调试方法**：
```c
// 添加调试输出
klog_debug("Status: 0x%02X\n", inb(io_base + ATA_REG_STATUS));
```

---

## 验证输出

如果一切正常，你应该看到类似这样的输出：

```
[ATA] Initializing ATA driver...
[ATA] Primary Master: Present
[ATA] Primary Slave: Not Present
[ATA] Secondary Master: Not Present
[ATA] Secondary Slave: Not Present
[ATA] Device 0 Info:
       Model:    QEMU HARDDISK
       Serial:   QM00001
       Firmware: 2.5+
       LBA:      Yes
       LBA48:    Yes
       DMA:      No
       Capacity: 262144 sectors (128 MB)
```

---

## 到这里我们完成了什么

这篇文章我们实现了：
1. 设备选择函数 `ata_select_device()`
2. 设备存在检测 `ata_device_present()`
3. IDENTIFY 命令发送和数据读取
4. IDENTIFY 数据解析
5. 字符串字节序转换
6. 完整的初始化流程

现在我们的驱动可以检测到 ATA 设备并获取设备信息了。下一篇文章我们会实现核心的 PIO 读写功能。

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← ATA 硬件基础与 I/O 端口](02_ATA硬件基础与I_O端口.md)  | [PIO 模式读写实现 →](04_PIO模式读写实现.md)

</div>
