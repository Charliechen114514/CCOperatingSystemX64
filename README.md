<div align="center">

# 🖥️ CCOperatingSystemX64

### **从零构建的 64 位 x86_64 操作系统**

> 项目从CCOperateSystem（笔者的X86操作系统）派生！

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![Progress](https://img.shields.io/badge/progress-90%_for_base_stage-blue)]()
[![Platform](https://img.shields.io/badge/platform-x86__64-orange)]()

**升级到X64，全新的[CCOperateSystem](https://github.com/Charliechen114514/CCOperateSystem)**

---

</div>

## 📖 项目简介

**CCOperatingSystemX64** 是一个完全从零开始的 x86_64 架构操作系统开发项目。项目旨在通过实践深入理解计算机系统底层原理，包括：

- 🔧 **双阶段 Bootloader** - 支持 LBA 与 CHS 双模式磁盘读取
- 🚀 **64位长模式** - 完整的 x86_64 长模式切换与页表设置
- 🏗️ **CMake 构建系统** - 现代化的构建与调试流程
- 🐛 **QEMU 集成调试** - 支持 GDB 远程调试与 VNC 图形界面

### 核心特性

| 特性 | 说明 |
|------|------|
| **双阶段 Bootloader** | 支持 LBA 与 CHS 双模式磁盘读取 |
| **64位长模式** | 完整的 x86_64 长模式切换与页表设置 |
| **CMake 构建系统** | 现代化的构建与调试流程 |
| **VSCode 调试** | 一键启动 GDB 远程调试 |
| **模块化设计** | 清晰的代码组织，易于学习 |

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

# 运行（VGA 图形模式 - 推荐）
cmake --build build --target vga-run

# 一键构建并运行
cmake -B build && cmake --build build --target build-and-vga-run
```

### 调试

按 `F5` 即可启动 VSCode 调试，支持断点、变量监视、调用栈查看等功能。

详见 [调试教程](document/debug_tutorial/)

---

## ✨ 功能一览

### 系统基础

| 模块 | 功能 |
|------|------|
| **Bootloader** | 双阶段加载、LBA/CHS 兼容、64位长模式切换 |
| **内核入口** | 独立入口点、BSS 段清零、栈帧设置 |
| **串口驱动** | COM1 配置、ANSI 颜色、同步输出 |

### 显示系统

| 模块 | 功能 |
|------|------|
| **VGA 文本模式** | 80x25 显示、16 色支持、硬件/软件光标 |
| **格式化输出** | printf 风格输出、多后端支持 |
| **欢迎界面** | ASCII 艺术 logo、模块化设计 |

### 内存与数据结构

| 模块 | 功能 |
|------|------|
| **字符串库** | 完整字符串操作 (strlen/strcpy/strcmp/strtok 等) |
| **内存操作** | memset/memcpy/memmove/memcmp |
| **双向链表** | Linux kernel 风格链表操作 |
| **位图操作** | 位图管理、位查找、位图运算 |
| **数学工具** | 对齐、幂运算、除法变体、位操作宏 |

### 中断与设备驱动

| 模块 | 功能 |
|------|------|
| **中断框架** | IDT、异常处理、IRQ 描述符系统 |
| **PIC 控制器** | 8259A 初始化、IRQ 重映射、EOI 处理 |
| **PIT 定时器** | 可编程间隔定时器驱动 |
| **键盘驱动** | PS/2 控制器、扫描码解析、环形缓冲区 |
| **串口中断** | RX/TX 中断、串口 Shell |
| **RTC 时钟** | CMOS RTC、周期性中断、闹钟功能 |

### 开发工具

| 模块 | 功能 |
|------|------|
| **日志系统** | 五级日志、多后端输出、可配置过滤 |
| **断言系统** | 运行时/静态断言、可配置后端 |
| **栈回溯** | 符号解析、表格化输出、安全验证 |
| **Shell 系统** | 多后端支持、命令注册、内置命令 |

---

## 📚 学习路径

### 教程 (Tutorial)

每章教程都配有对应代码，从零开始构建操作系统：

| 章节 | 内容 |
|------|------|
| [01 Bootloader](tutorial/01_bootloader/) | BIOS 加载与 MBR 引导 |
| [02 加载汇编内核](tutorial/02_load_asm_kernel/) | 简单内核加载 |
| [03 统一引导](tutorial/03_unified_boots/) | 多内核支持 |
| [04 CMake 联合构建](tutorial/04_cmake_union/) | 现代化构建系统 |
| [05 大内核支持](tutorial/05_load_large_kernel/) | 超过扇区限制的内核 |
| [06 调试入门](tutorial/06_learn_debug/) | GDB 基础调试 |
| [07 VGA + VSCode 调试](tutorial/07_vga_with_vscode_debug/) | 图形模式与完整调试 |
| [08 字符串工具](tutorial/08_string_utils/) | C 标准库实现 |
| [09 内存与串口](tutorial/09_memory_serial/) | 内存操作与串口通信 |
| [10 格式化日志](tutorial/10_base_with_format_log/) | printf 与日志系统 |
| [11 链表与位图](tutorial/11_list_math_bitmaps_as_base_finished/) | 数据结构基础 |
| [12 栈回溯](tutorial/12_stacktrace_supports/) | 符号解析与调试 |
| [13 中断基础](tutorial/13_interrupt_base/) | IDT/PIT/键盘驱动 |
| [14 更多中断设备](tutorial/14_more_intr_devices/) | 串口中断/RTC/Shell |

### 文档 (Document)

深入理解各模块设计与实现细节：

| 文档 | 说明 |
|------|------|
| [Bootloader 设计](document/01_bootloader/) | 引导程序原理与实现 |
| [内核加载](document/02_load_asm_kernel/) | 内核加载流程 |
| [大内核支持](document/05_load_large_kernel/) | 超大内核加载方案 |
| [构建指南](document/build.md) | CMake 构建系统详解 |
| [调试教程](document/debug_tutorial/) | 调试技巧与工具使用 |
| [开发经验](document/experience/) | 开发过程中的经验总结 |

---

## 🎯 进度与规划

当前项目已完成基础中断与设备驱动阶段，详见 [PROGRESS.md](PROGRESS.md)

---

## 🛠️ 技术栈

| 类别 | 技术 |
|------|------|
| **汇编** | NASM (x86_64) |
| **C 语言** | GCC (freestanding) |
| **构建** | CMake 4.2+ |
| **调试** | QEMU + GDB |
| **版本控制** | Git |

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

---

## 📄 许可证

本项目采用 [MIT 许可证](LICENSE)

---

<div align="center">

**Made with ❤️ for OS Development Enthusiasts**

[⬆ 返回顶部](#-ccoperatingsystemx64)

</div>
