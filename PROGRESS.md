# CCOS 开发进度

x86_64 操作系统开发 - 从 bootloader 到内核

---

## 📋 最新更新 (2026-02-15)

### 动态内核加载配置系统 v3 + CHS 多扇区读取

已完成动态内核配置生成流水线和 CHS 动态多扇区读取功能。

#### 构建流程

```
┌─────────────────────────────────────────────────────────────────┐
│                        Build Pipeline                           │
├─────────────────────────────────────────────────────────────────┤
│ 1. bootloader (mock config)  →  bootloader.bin                 │
│ 2. kernel                     →  kernel.bin                     │
│ 3. final_config               →  boot_config_final.inc          │
│    ├─ 读取 bootloader.bin 大小                                   │
│    ├─ 读取 kernel.bin 大小                                       │
│    ├─ 计算 CHS 参数                                              │
│    └─ 打印磁盘布局信息                                           │
│ 4. boot_img                   →  boot.img (dd组装)              │
│ 5. verify_disk_layout                                         │
│ 6. verify_boot_image                                           │
└─────────────────────────────────────────────────────────────────┘
```

#### 新增/修改组件

| 组件 | 路径 | 说明 |
|------|------|------|
| Mock配置 | [boot/boot_config.inc](boot/boot_config.inc) | 首次编译使用 |
| 配置生成脚本 | [cmake/GenerateKernelSize.cmake](cmake/GenerateKernelSize.cmake) | 动态计算内核大小和CHS参数 |
| Boot镜像组装 | [cmake/AssembleBootImage.cmake](cmake/AssembleBootImage.cmake) | 动态dd命令 |
| 布局验证脚本 | [scripts/verify_disk_layout.py](scripts/verify_disk_layout.py) | 磁盘/内存布局验证和魔数检测 |
| Boot镜像验证 | [scripts/verify_boot_image.py](scripts/verify_boot_image.py) | dd烧录验证 |
| CHS 加载 | [boot/bootloader.asm](boot/bootloader.asm) | LBA→CHS转换 + 多扇区读取 |

#### 磁盘布局（动态计算）

```
┌─────────┬──────────┬──────────────────┐
│  LBA    │  扇区    │      内容        │
├─────────┼──────────┼──────────────────┤
│   0-2   │  1-3     │ bootloader.bin   │
│   3+    │  4+      │ kernel.bin       │
└─────────┴──────────┴──────────────────┘
```

#### 内存布局（确保隔离）

```
Stage 1:     0x7C00 - 0x7E00   (512 bytes)
Stage 2:     0x7E00 - 0x8400   (1536 bytes)
--- 62 sector gap (31744 bytes) ---
Kernel:      0x10000 - 0x10200+ (动态大小，最大64MB)
```

#### VGA 启动输出

```
READY TO BOOT CCOS
[1] Stage 1: Loading second stage...
[2] Stage 2: Loading kernel...
[LOAD] CHS: loading 1 sectors
READY TO BOOT KERNEL
```

---

## ✅ 已完成功能

### 1. Bootloader (统一架构)

| 组件 | 地址 | 状态 |
|------|------|------|
| Stage 1 | 0x7C00 | ✅ |
| Stage 2 | 0x7E00 | ✅ |

**功能**:
- LBA/CHS 磁盘读取
- 实模式字符串打印
- GDT 加载
- 保护模式切换
- 长模式切换 (64-bit)
- **LBA→CHS 动态转换**
- **CHS 多扇区分批读取 (最大127扇区/次)**

### 2. 内核加载

| 阶段 | 状态 |
|------|------|
| 实模式加载内核 | ✅ |
| 长模式跳转 | ✅ |
| 内核执行 | ✅ |

**功能**:
- Stage 2 在实模式加载 kernel.bin
- 内核加载到 `0x10000`
- 跳转到 64 位内核入口
- 内核成功输出验证字符

### 3. 模式切换

```
Real Mode (16-bit)
    ↓
Protected Mode (32-bit)
    ↓
Long Mode (64-bit)
    ↓
Kernel Execution
```

---

## 📋 下一步工作

- [ ] **实现 LBA 扩展读取 (INT 13h AH=42h)**
  - 使用 DAP (Disk Address Packet) 数据包
  - 支持大于 8GB 的磁盘访问
  - 检测 BIOS 扩展读写支持

- [ ] **实现 LBA 优先 + CHS 回退加载策略**
  ```
  尝试 LBA 扩展读取
      ↓
  成功？→ 完成
      ↓ 失败
  CHS 回退读取
      ↓
  成功？→ 完成 / 失败→错误
  ```

- [ ] **支持更大内核（>64KB）的分段加载**
  - 处理 ES:BX 跨段边界
  - 支持最大 64MB 内核

- [ ] ACPI 内存检测（探测可用内存）
- [ ] 串口驱动（UART 调试输出）

---

## 🔬 预期现象

### 启动输出 (VGA)

运行 `make run` 后，屏幕应显示：

```
READY TO BOOT CCOS
[1] Stage 1: Loading second stage...
[2] Stage 2: Loading kernel...
[LOAD] CHS: loading 1 sectors
READY TO BOOT KERNEL
```

### QEMU/GDB 调试

```bash
# 启动调试
make debug

# GDB 连接
gdb -ex "target remote :1234"

# 关键断点
b *0x7E00      # Stage 2 入口
b *0x10000     # 内核入口
```

---

## 📁 项目结构

```
CCOperatingSystemX64/
├── boot/                  # Bootloader 源码
│   ├── bootloader.asm    # 统一 Bootloader (Stage 1 + Stage 2)
│   └── boot_config.inc   # 自动生成的配置
├── kernel/                # 内核源码
│   └── kernel_main.c     # 64 位内核入口 (C)
├── build/                 # 构建产物
│   ├── bootloader.bin   # Bootloader
│   ├── kernel.bin       # 内核
│   └── boot.img         # 完整启动镜像
├── cmake/                 # CMake 构建脚本
│   ├── GenerateKernelSize.cmake
│   └── AssembleBootImage.cmake
├── scripts/               # 验证脚本
│   ├── verify_disk_layout.py
│   └── verify_boot_image.py
└── PROGRESS.md           # 本文件
```

---

## 🚀 快速开始

```bash
# 构建
cmake --build build

# 运行
make run

# 调试
make debug

# 清理
make clean
```

---
