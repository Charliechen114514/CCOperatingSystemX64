# CCOS 开发进度

x86_64 操作系统开发 - 从 bootloader 到内核

---

## 📋 最新更新 (2026-02-15)

### 动态内核加载配置系统 v2

已完成动态内核配置生成流水线的完整集成，支持最大64MB内核的CHS模式加载。

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

#### 验证输出示例

```
============================================================
  DISK LAYOUT CALCULATION
============================================================
Bootloader:
  Size: 1082 bytes = 3 sectors
  Location: Sector 1-3 (LBA 0-3-1)

Kernel:
  Size: 83 bytes = 1 sectors
  Location: Sector 3+ (LBA 3+)
  Load address: 0x10000

CHS Parameters (for fallback):
  Cylinder: 0
  Head: 0
  Sector: 3 (1-based)

Memory Layout:
  Stage 2 ends at:  0x33792
  Kernel starts at: 0x10000
  Memory gap: 31744 bytes (62 sectors)
  ✓ Memory gap sufficient for safe loading
============================================================
```

#### 下一步工作

- [ ] 实现 LBA 优先 + CHS 回退的加载策略
- [ ] 支持多扇区分批读取（内核 > 127 扇区时）

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
- 模块化工具库 (`boot/lib/`)

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

### 3. 磁盘布局

```
┌─────────┬──────────┬──────────────────┐
│  扇区   │  大小    │      内容        │
├─────────┼──────────┼──────────────────┤
│   0-2   │  1536B   │ bootloader.bin   │
│   3+    │   512B   │ kernel.bin       │
└─────────┴──────────┴──────────────────┘
```

> 注：bootloader.asm 合并了 Stage 1 + Stage 2，共占用 3 个扇区

### 4. 模式切换

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

## 🔬 预期现象

### 启动输出 (VGA)

运行 `make run` 后，屏幕应显示：

```
CCOS Bootloader Stage 1...
Loading Stage 2...
Stage 2 running...
[LOAD] Loading kernel...
[OK] Kernel loaded to 0x10000!
[1] GDT loaded, PM enabled...
```

VGA 显存 (`0xB8000`) 调试字符：

```
行 0 位置 12-16:  G H I J K
行 5 位置 0:     X (内核输出)
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
│   └── lib/              # Bootloader 工具库
│       ├── bios_screen.asm   # VGA 屏幕操作
│       ├── bios_string.asm   # 字符串打印
│       ├── disk_io.asm       # 磁盘 I/O
│       ├── lmode.asm         # 长模式切换
│       └── pmode.asm         # 保护模式切换
├── kernel/                # 内核源码
│   └── kernel.asm       # 64 位内核入口（待替换为 C）
├── build/                 # 构建产物
│   ├── bootloader.bin   # Bootloader (1007B)
│   ├── kernel.bin       # 内核 (18B)
│   └── boot.img         # 完整启动镜像
├── document/              # 文档
│   ├── 01_bootloader/   # Bootloader 文档
│   └── 02_load_asm_kernel/  # 内核加载文档
├── Makefile              # 构建脚本
└── PROGRESS.md           # 本文件
```

---

## 🚀 快速开始

```bash
# 构建
make

# 运行
make run

# 调试
make debug

# 清理
make clean
```

---

## 📖 文档索引

### 01_bootloader
- [README](document/01_bootloader/README.md) - 文档导航
- [开发笔记](document/01_bootloader/开发笔记.md) - Bootloader 开发经验
- [技术参考](document/01_bootloader/技术参考.md) - x86 技术细节
- [故障排查](document/01_bootloader/故障排查指南.md) - 问题诊断
- [调试工具](document/01_bootloader/调试工具指南.md) - 工具使用

### 02_load_asm_kernel
- [README](document/02_load_asm_kernel/README.md) - 文档导航
- [开发笔记](document/02_load_asm_kernel/开发笔记.md) - 内核加载实现
- [技术参考](document/02_load_asm_kernel/技术参考.md) - 内存/磁盘布局
- [故障排查](document/02_load_asm_kernel/故障排查指南.md) - 问题诊断
- [调试工具](document/02_load_asm_kernel/调试工具指南.md) - 工具使用

---

## 📋 下一步计划

- [ ] ACPI 内存检测（探测可用内存）
- [ ] 串口驱动（UART 调试输出）

---

