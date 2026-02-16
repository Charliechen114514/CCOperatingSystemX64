# CCOS 动态内核加载与构建流水线 文档中心

本目录包含 CCOS 支持大内核加载和动态配置生成流水线的完整文档体系。

---

## 阶段概述

### 从 CMake 统一构建到动态内核加载 (Stage 04 → Stage 05)

本阶段实现了以下关键功能：

| 功能 | 描述 | 文件 |
|------|------|------|
| 动态配置生成 | 构建时自动计算内核大小并生成配置头文件 | [cmake/GenerateKernelSize.cmake](../../cmake/GenerateKernelSize.cmake) |
| CHS 大内核支持 | 支持最大 64MB 内核的 CHS 模式加载 | [boot/bootloader.asm](../../boot/bootloader.asm) |
| 磁盘布局验证 | 自动验证磁盘和内存布局，防止重叠 | [scripts/verify_disk_layout.py](../../scripts/verify_disk_layout.py) |
| 配置头文件 | 统一的配置接口，支持 Mock 和自动生成 | [boot/boot_config.inc](../../boot/boot_config.inc) |

---

## 核心变更

### 1. 新增组件

```
CCOperatingSystemX64/
├── boot/
│   ├── boot_config.inc          # 新增：动态配置头文件（含 Mock 配置）
│   └── bootloader.asm           # 修改：新增 load_kernel_chs 函数
├── cmake/
│   └── GenerateKernelSize.cmake  # 新增：配置生成脚本
├── scripts/
│   └── verify_disk_layout.py    # 新增：磁盘布局验证脚本
└── CMakeLists.txt               # 修改：集成验证脚本和 Python 依赖
```

### 2. 构建流水线

```mermaid
graph TD
    A[cmake ..] --> B[生成 boot_config.inc<br/>(Mock值)]
    B --> C[编译 bootloader.asm<br/>(使用配置)]
    C --> D[编译 kernel.asm]
    D --> E[生成 boot.img]
    E --> F[verify_disk_layout.py<br/>(验证布局)]
    F --> G[构建成功]
```

### 3. 磁盘布局

```
┌─────────┬──────────┬──────────────────┐
│  LBA    │  扇区    │      内容        │
├─────────┼──────────┼──────────────────┤
│   0-2   │  1-3     │ bootloader.bin   │
│   3+    │  4+      │ kernel.bin       │
└─────────┴──────────┴──────────────────┘
```

### 4. 内存布局

```
Stage 1:     0x7C00 - 0x7E00   (512 bytes)
Stage 2:     0x7E00 - 0x8400   (1536 bytes)
--- 62 sector gap (31744 bytes) ---
Kernel:      0x10000 - 0x10200+ (动态大小，最大64MB)
```

---

## 文档导航

### 1. [技术参考](./技术参考.md) 📖 技术手册

**内容**:
- 动态配置系统架构
- CHS 与 LBA 寻址模式详解
- 磁盘布局与内存映射
- 构建流水线工作原理

**适合**:
- 理解系统架构设计
- 学习配置生成机制
- 了解大内核加载实现

---

### 2. [开发笔记](./开发笔记.md) 💡 经验总结

**内容**:
- 动态配置设计的演进过程
- Mock 配置与自动生成的权衡
- 构建系统集成经验
- 未来改进方向（LBA 优先策略、多批读取）

**适合**:
- 理解设计思路
- 学习他人的经验
- 规划自己的开发路径

---

### 3. [调试工具指南](./调试工具指南.md) 🛠️ 工具参考

**内容**:
- CMake 配置生成工具使用
- Python 验证脚本详解
- QEMU 运行与调试技巧
- 构建系统调试方法

**适合**:
- 学习构建工具的使用
- 查询具体工具命令
- 提升调试效率

---

### 4. [故障排查指南](./故障排查指南.md) 🔧 问题解决

**内容**:
- 内核过大加载失败
- 磁盘布局重叠问题
- 配置生成错误
- CHS 参数计算错误

**适合**:
- 遇到具体问题时查阅
- 学习问题排查思路
- 理解常见错误原因

---

## 快速开始

### 如果你想了解架构

1. **整体设计** → 查看 [技术参考 §1](./技术参考.md#1-动态配置系统)
2. **磁盘布局** → 查看 [技术参考 §2](./技术参考.md#2-磁盘布局)
3. **内存映射** → 查看 [技术参考 §3](./技术参考.md#3-内存布局)

### 如果你想学习开发

1. **设计思路** → 查看 [开发笔记 §1](./开发笔记.md#1-设计演进)
2. **构建集成** → 查看 [开发笔记 §2](./开发笔记.md#2-构建系统集成)
3. **未来方向** → 查看 [开发笔记 §3](./开发笔记.md#3-未来改进)

### 如果你想调试问题

1. **验证脚本** → 查看 [调试工具指南 §1](./调试工具指南.md#1-磁盘布局验证工具)
2. **构建调试** → 查看 [调试工具指南 §2](./调试工具指南.md#2-cmake-构建调试)
3. **常见问题** → 查看 [故障排查指南](./故障排查指南.md)

---

## 构建命令

### 标准构建

```bash
mkdir build && cd build
cmake ..
cmake --build . --target boot_img
```

### 验证磁盘布局

```bash
# 自动验证（构建时执行）
python3 ../scripts/verify_disk_layout.py \
    bootloader.bin \
    kernel.bin
```

### 运行测试

```bash
# 文本模式
make run

# VGA 模式 (VNC)
make vga-run
# 连接: vncviewer localhost:5900
```

---

## 关键指标

| 指标 | 值 | 说明 |
|------|-----|------|
| 最大内核大小 | 64 MB | 131,072 扇区 |
| Bootloader 大小 | 3 扇区 | Stage1(1) + Stage2(2) |
| 内存加载地址 | 0x10000 | 确保与 Bootloader 隔离 |
| 磁盘起始扇区 | LBA 4 | 扇区 4 (1-based) |

---

## 文档关系图

```
                    ┌──────────────────┐
                    │   项目根目录    │
                    │   (PROGRESS.md)  │
                    └────────┬─────────┘
                             │
                    ┌────────▼─────────┐
                    │  document/       │
                    │  (README.md) ◄──┘
                    └────────┬─────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
┌───────▼──────┐   ┌─────▼──────┐   ┌─────▼──────┐
│ 故障排查指南  │   │ 调试工具指南 │   │  技术参考   │
└──────────────┘   └─────────────┘   └─────────────┘
        │                                    │
        └──────────────────┬─────────────────┘
                           │
                    ┌──────▼──────┐
                    │  开发笔记   │
                    └─────────────┘
```

---

## 下一步工作

- [ ] 实现 LBA 优先 + CHS 回退的加载策略
- [ ] 支持多扇区分批读取（内核 > 127 扇区时）
- [ ] 集成 `GenerateKernelSize.cmake` 到构建流程（自动替换 mock 值）
- [ ] 支持超过 64MB 的内核大小
- [ ] ELF 格式内核加载支持

---

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../01_bootloader/](../01_bootloader/) - Bootloader 文档
- [../02_load_asm_kernel/](../02_load_asm_kernel/) - 内核加载文档

### 源码文件
- [boot/boot_config.inc](../../boot/boot_config.inc) - 配置头文件
- [boot/bootloader.asm](../../boot/bootloader.asm) - Bootloader 源码
- [cmake/GenerateKernelSize.cmake](../../cmake/GenerateKernelSize.cmake) - 配置生成脚本
- [scripts/verify_disk_layout.py](../../scripts/verify_disk_layout.py) - 验证脚本

### 外部资源
- [OSDev Wiki](https://wiki.osdev.org/) - OS 开发百科
- [Intel SDM](https://software.intel.com/content/www/us/en/develop/articles/intel-sdm.html) - x86 官方文档
- [CMake Documentation](https://cmake.org/documentation/) - CMake 官方文档

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-15
