<div align="center">

# 🖥️ CCOperatingSystemX64

### **从零构建的 64 位 x86_64 操作系统**，项目从CCOperateSystem（笔者的X86操作系统）派生

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![Progress](https://img.shields.io/badge/progress-45%25-blue)]()
[![Platform](https://img.shields.io/badge/platform-x86__64-orange)]()

---

**升级到X64，全新的[CCOperateSystem](https://github.com/Charliechen114514/CCOperateSystem)**

</div>

---

## 🌟 项目简介

**CCOperatingSystemX64** 是一个完全从零开始的 x86_64 架构操作系统开发项目。项目旨在通过实践深入理解计算机系统底层原理，包括：

- 🔧 **双阶段 Bootloader** - 支持 LBA 与 CHS 双模式磁盘读取
- 🚀 **64位长模式** - 完整的 x86_64 长模式切换与页表设置
- 🏗️ **CMake 构建系统** - 现代化的构建与调试流程
- 🐛 **QEMU 集成调试** - 支持 GDB 远程调试与 VNC 图形界面

---

## 🚀 快速开始

### 前置要求

| 工具 | 版本要求 | 用途 |
|------|----------|------|
| **NASM** | >= 2.15 | 汇编器 |
| **GCC** | >= 15.2.1 | C 编译器 |
| **CMake** | >= 4.2.3 | 构建系统 |
| **QEMU** | >= 10.2.0 | 系统模拟器 |
| **Python** | >= 3.14.2 | 构建脚本 |

### 构建与运行

```bash
# 克隆项目
git clone https://github.com/Charliechen114514/CCOperatingSystemX64
cd CCOperatingSystemX64

# 配置并构建
cmake -B build
cmake --build build

# 运行（文本模式）
cmake --build build --target run

# 运行（VGA 图形模式 - 推荐）
cmake --build build --target vga-run

# 构建并运行（一步到位）
cmake -B build && cmake --build build --target build-and-vga-run
```

### 调试模式

```bash
# 启动 QEMU 调试服务器
cmake --build build --target debug

# 在另一个终端连接 GDB
gdb build/kernel.elf -ex 'target remote :1234'
```

### ✅ 已完成功能

#### Bootloader (v2.0)
- **Stage 1 (MBR)**: BIOS 加载、欢迎信息、Stage 2 加载
- **Stage 2**: LBA 扩展读取、CHS 兼容模式、动态磁盘读取、64位长模式切换、页表设置
- **错误处理**: 完善的错误码与调试输出

#### 内核基础
- 64位长模式执行环境
- 独立内核入口点
- BSS 段自动清零
- 基础 VGA 文本输出

---

## 📚 文档

- [Bootloader 开发文档](document/01_bootloader/)
- [内核加载文档](document/02_load_asm_kernel/)
- [大内核支持文档](document/05_load_large_kernel/)
- [构建指南](document/build.md)

---

## 🎯 下一步计划

请到这里来——详见 [PROGRESS.md](PROGRESS.md)

---

## 🛠️ 技术栈

- **汇编**: NASM (x86_64)
- **C 语言**: GCC (freestanding)
- **构建**: CMake 4.2+
- **调试**: QEMU + GDB
- **版本控制**: Git

---

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件

---

<div align="center">

**Made with ❤️ for OS Development Enthusiasts**

[⬆ 返回顶部](#-ccoperatingsystemx64)

</div>
