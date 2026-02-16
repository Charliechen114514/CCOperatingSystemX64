<div align="center">

# 🖥️ CCOperatingSystemX64

### **从零构建的 64 位 x86_64 操作系统**，项目从CCOperateSystem（笔者的X86操作系统）派生！

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![Progress](https://img.shields.io/badge/progress-65%25-blue)]()
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

#### VSCode 调试 (推荐)

项目已配置完整的 VSCode 调试支持，只需按 `F5` 即可启动调试：

- **断点调试**: 支持在 C/汇编代码中设置断点
- **变量监视**: 实时查看变量值和内存内容
- **调用栈**: 查看完整的函数调用链
- **单步执行**: 逐行或逐汇编指令调试

配置文件位于 `.vscode/launch.json`，自动处理：
- QEMU 调试端口 (1234)
- GDB 多语言支持 (C/Assembly)
- 符号文件自动加载

#### GDB 命令行调试

```bash
# 启动 QEMU 调试服务器
cmake --build build --target debug

# 在另一个终端连接 GDB
gdb build/kernel.elf -ex 'target remote :1234'
```

---

### ✅ 已完成功能

#### Bootloader (v2.0)
- **Stage 1 (MBR)**: BIOS 加载、欢迎信息、Stage 2 加载
- **Stage 2**: LBA 扩展读取、CHS 兼容模式、动态磁盘读取、64位长模式切换、页表设置
- **错误处理**: 完善的错误码与调试输出

#### 内核基础
- 64位长模式执行环境
- 独立内核入口点
- BSS 段自动清零
- 基础控制台输出

#### 串口驱动 🆕
- **串口初始化**: 支持 COM1 端口配置
- **同步输出**: sync_serial_puts() 非阻塞输出
- **ANSI 颜色**: 支持终端颜色转义序列
- **日志集成**: 与日志系统无缝集成

#### VGA 图形驱动 🆕
- **VGA 文本模式**: 80x25 字符显示支持
- **颜色支持**: 16 色 VGA 调色板
- **光标控制**: 硬件光标位置管理
- **滚动功能**: 屏幕内容向上滚动
- **格式化输出**: 支持 printf 风格的文本输出

#### 日志系统 🆕
- **分级日志**: TRACE/DEBUG/INFO/WARN/ERROR 五级日志
- **多后端支持**: 串口/VGA 双后端输出
- **可配置**: 运行时日志级别过滤
- **kprintf**: 内核 printf 风格格式化输出

#### 欢迎界面 🆕
- **模块化设计**: 支持串口/VGA 双欢迎界面
- **自动检测**: 根据编译目标选择显示方式
- **美观输出**: 支持 ASCII 艺术 logo

#### 基础库函数 🆕
- **字符串操作**: strlen, strcpy, strcmp, strchr, strstr, strtok 等完整字符串库
- **内存操作**: memset, memcpy, memmove, memcmp
- **数值转换**: strtol, strtoll, strtoul, atoi, itoa, uitoa
- **字符处理**: isspace, isdigit, tolower

#### 断言系统 🆕
- **运行时断言**: assert(cond) 宏
- **静态断言**: static_assert 编译时检查
- **断言后端**: 可配置的断言失败处理

#### 开发工具 🆕
- **VSCode 调试配置**: 一键启动调试环境
- **clangd 支持**: 完整的 LSP 代码补全
- **格式化配置**: 统一的代码风格
- **CMake 集成**: 现代化构建流程

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
