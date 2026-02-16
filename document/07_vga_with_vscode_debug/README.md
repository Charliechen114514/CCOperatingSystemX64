# CCOS VGA 图形界面与 VSCode 调试 文档中心

本目录包含 CCOS 在 **VGA 图形界面与 VSCode 调试阶段** (stage/07_vga_with_vscode_debug) 的完整文档体系。

---

## 阶段概述

### 什么是 VGA 图形界面与 VSCode 调试阶段？

在 `stage/06_learn_debug` 阶段，项目已建立基础调试能力，但 VGA 输出较为简单，且 VSCode 调试体验不够完善。

在 **本阶段** (`stage/07_vga_with_vscode_debug`)，我们进行了两大核心改进：
1. **增强 VGA 图形界面能力** - 添加 GUI Helper 函数库和精美的启动欢迎界面
2. **完善 VSCode 调试体验** - 改进调试脚本，实现自动化构建和调试流程

### 主要改进

#### 1. VGA GUI Helper 函数库
- **基础绘图原语** - 矩形、线条、填充矩形
- **面板系统** - 带标题和边框的 UI 面板
- **文本布局** - 居中文本、进度条
- **动画效果** - 颜色循环、渐变边框

#### 2. 精美的启动欢迎界面
- **ASCII Art Logo** - CCOS 品牌标识
- **星空背景** - 随机生成的装饰性星点
- **动画边框** - 彩色渐变循环效果
- **打字机效果** - 逐字符显示文本

#### 3. 内核初始化重构
- **kernel_init 模块** - 将启动逻辑从 kernel_main 分离
- **清晰的模块边界** - 便于后续扩展和维护

#### 4. VSCode 调试体验改进
- **自动化构建** - 调试前自动清理并重新构建
- **智能监控** - 自动检测 GDB 断开并停止 QEMU
- **状态管理** - PID 文件管理，防止进程泄漏

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 💡 经验总结

**内容**:
- VGA GUI 架构设计思路
- 欢迎界面的实现细节
- VSCode 调试脚本的改进历程
- 遇到的挑战和解决方案

**适合**:
- 理解设计思路
- 学习图形界面开发
- 了解开发历程

### 2. [技术参考](./技术参考.md) 📖 技术手册

**内容**:
- VGA GUI Helper API 完整参考
- 面板系统设计规范
- 动画效果实现原理
- VSCode 调试脚本语法

**适合**:
- 查询 API 详情
- 理解底层原理
- 扩展 GUI 功能

### 3. [调试工具指南](./调试工具指南.md) 🛠️ 工具参考

**内容**:
- VSCode 调试配置详解
- launch_qemu_for_vscode_debug.sh 脚本使用
- GDB 连接自动化
- 调试流程最佳实践

**适合**:
- 学习调试技巧
- 配置调试环境
- 提升开发效率

### 4. [故障排查指南](./故障排查指南.md) 🔧 问题解决

**内容**:
- VGA 显示异常诊断
- VSCode 调试连接问题
- 构建失败处理
- 进程管理问题

**适合**:
- 解决具体问题
- 学习调试思路
- 避免常见错误

---

## 代码结构

### 文件组织

```
CCOperatingSystemX64/
├── CMakeLists.txt                      # 根 CMake 配置（更新）
├── scripts/
│   └── launch_qemu_for_vscode_debug.sh # VSCode 调试启动脚本（重大更新）
├── kernel/
│   ├── kernel_main.c                   # 内核入口（简化）
│   ├── kernel_init.c                   # 内核初始化（新增）
│   ├── kernel_init.h                   # 初始化头文件（新增）
│   ├── kernel_entry.asm                # 汇编入口
│   └── driver/
│       └── vga/
│           ├── vga.h                   # VGA 核心接口
│           ├── vga.c                   # VGA 核心实现
│           ├── vga_helpers.h           # 辅助函数声明（新增）
│           ├── vga_helpers.c           # 辅助函数实现（新增）
│           ├── vga_config.h            # VGA 配置
│           ├── vga_example.c           # 示例代码
│           └── gui_helper/             # GUI Helper 库（新增目录）
│               ├── gui_helper.h        # GUI 接口声明
│               └── gui_helper.c        # GUI 实现
└── build/                              # 构建输出目录
    ├── bootloader.bin
    ├── kernel.bin
    ├── kernel.elf                      # 带调试符号的 ELF
    └── boot.img
```

### 新增文件概览

| 文件 | 类型 | 功能描述 |
|------|------|----------|
| `kernel_init.c/h` | C | 内核初始化模块，包含欢迎界面显示逻辑 |
| `vga_helpers.c/h` | C | VGA 辅助函数，提供定位字符输出和延迟功能 |
| `gui_helper.c/h` | C | GUI 绘图原语库，提供面板、矩形、线条等 |
| `launch_qemu_for_vscode_debug.sh` | Shell | 改进的 VSCode 调试脚本，支持自动构建和监控 |

### VGA GUI Helper API

```c
// 基础绘图
void vga_draw_rect(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                   vga_sz_t width, vga_sz_t height, vga_color_t color);

void vga_draw_fill_rect(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                        vga_sz_t width, vga_sz_t height,
                        char fill_char, vga_color_t font, vga_color_t bg);

// 线条绘制
void vga_draw_hline(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                    vga_sz_t length, char line_char, vga_color_t color);

void vga_draw_vline(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                    vga_sz_t length, char line_char, vga_color_t color);

// 面板系统
void vga_draw_panel(CCOS_VGA* vga, const vga_panel_t* panel);

// 文本布局
void vga_draw_text_centered(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                            vga_sz_t width, const char* text,
                            vga_color_t font, vga_color_t bg);

// 进度条
void vga_draw_bar(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                  vga_sz_t width, vga_sz_t filled,
                  char fill_char, vga_color_t fill_color,
                  vga_color_t empty_color);
```

---

## 构建和使用

### 配置和构建

```bash
# 配置构建 (Debug 模式，带调试符号)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 构建所有目标
cmake --build build
```

### VSCode 调试

```bash
# 方式一：使用脚本自动启动（推荐）
./scripts/launch_qemu_for_vscode_debug.sh

# 方式二：在 VSCode 中按 F5 启动调试
# 确保已配置 .vscode/launch.json
```

### 文本模式运行

```bash
# 普通运行（无调试）
cmake --build build --target run
```

### VGA 模式运行

```bash
# VGA 图形模式运行
cmake --build build --target vga-run
# 在另一个终端连接: vncviewer localhost:5900
```

---

## 与前一个阶段的对比

| 特性 | stage/06 | stage/07 (本阶段) |
|-----|----------|------------------|
| VGA 输出能力 | 基础字符输出 | GUI Helper 库 + 面板系统 |
| 启动界面 | 简单文本 | 精美 ASCII Art + 动画 |
| 内核初始化 | 在 kernel_main 中 | 独立 kernel_init 模块 |
| VSCode 调试 | 手动构建 | 自动构建 + 智能监控 |
| 进程管理 | 手动清理 | PID 文件 + 自动检测 |
| 绘图原语 | 无 | 矩形、线条、填充、面板 |
| 文本布局 | 无 | 居中、进度条 |
| 动画效果 | 无 | 颜色循环、打字机效果 |

---

## 内存布局

```
地址         内容                    来源
───────────────────────────────────────────────────────────
0x7C00       Bootloader (Stage 1)    bootloader.asm section .mbr
0x7E00       Bootloader (Stage 2)    bootloader.asm section .stage2
0x10000      内核代码段 (.text)      kernel.elf → kernel.bin
             └─ kernel_entry.asm
             └─ kernel_main.c
             └─ kernel_init.c       (新增)
             └─ vga.c               (已有)
             └─ vga_helpers.c       (新增)
             └─ gui_helper.c        (新增)
0x80000      栈顶 (向下增长)         kernel_entry.asm 设置
0xB8000      VGA 文本缓冲区          硬件固定
```

---

## 技术要点

### VGA GUI 架构设计

本项目采用分层架构设计 VGA 图形系统：

```
┌─────────────────────────────────────────────────────────┐
│                    应用层                               │
│  (kernel_init.c - 欢迎界面、系统信息)                    │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│                  GUI Helper 层                          │
│  (gui_helper.c - 面板、矩形、线条、布局)                 │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│                  VGA Helpers 层                         │
│  (vga_helpers.c - 定位输出、延迟函数)                    │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│                   VGA 核心层                            │
│  (vga.c - 基础字符输出、光标管理)                        │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│                   硬件层                                │
│  (VGA 文本模式缓冲区 @ 0xB8000)                         │
└─────────────────────────────────────────────────────────┘
```

### VSCode 调试流程

```
┌─────────────────────────────────────────────────────────┐
│              1. 启动调试脚本                            │
│         ./scripts/launch_qemu_for_vscode_debug.sh      │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│              2. 自动清理并重新构建                       │
│     rm -rf build/ && cmake -DCMAKE_BUILD_TYPE=Debug    │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│              3. 启动 QEMU 调试模式                      │
│         qemu-system-x86_64 -s -S                       │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│              4. 在 VSCode 中按 F5                       │
│         GDB 连接到 localhost:1234                      │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│              5. 开始调试                                │
│         设置断点、单步执行、查看变量                    │
└─────────────────────────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│              6. 停止调试时                              │
│         检测 GDB 断开，自动停止 QEMU                   │
└─────────────────────────────────────────────────────────┘
```

---

## 快速链接

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目总体进度
- [../06_learn_debug/](../06_learn_debug/) - 调试基础设施文档
- [../08_string_utils/](../08_string_utils/) - 字符串工具文档

### 源代码
- [../../kernel/kernel_init.c](../../kernel/kernel_init.c) - 内核初始化实现
- [../../kernel/driver/vga/gui_helper/](../../kernel/driver/vga/gui_helper/) - GUI Helper 库
- [../../kernel/driver/vga/vga_helpers.c](../../kernel/driver/vga/vga_helpers.c) - VGA 辅助函数
- [../../scripts/launch_qemu_for_vscode_debug.sh](../../scripts/launch_qemu_for_vscode_debug.sh) - VSCode 调试脚本

---

## 文档维护

### 更新历史
- **2026-02-16**: 创建 VGA 图形界面与 VSCode 调试文档体系
  - README.md
  - 开发笔记.md
  - 技术参考.md
  - 调试工具指南.md
  - 故障排查指南.md

### 贡献指南
欢迎改进和补充文档：

1. **发现错误** → 直接修改并提交 PR
2. **补充案例** → 在相应文档添加
3. **新增章节** → 遵循现有格式

---

**文档维护者**: Claude Code + User
**最后更新**: 2026-02-16
**文档版本**: 1.0.0