# CCOS CMake 构建系统与 C 内核 文档中心

本目录包含 CCOS 在 **CMake 构建系统与 C 内核阶段** (stage/04_cmake_union) 的完整文档体系。

---

## 阶段概述

### 什么是 CMake 构建系统与 C 内核阶段？

在 `stage/03_unified_boots` 阶段，项目使用简单的 Makefile 构建，内核是纯汇编代码 (`kernel.asm`)。

在 **本阶段** (`stage/04_cmake_union`)，我们进行了两大核心改进：
1. **迁移到 CMake 构建系统** - 替换 Makefile，提供更强大的构建能力
2. **内核切换到 C 语言** - 用 C 代码替换纯汇编内核

### 主要改进

#### 1. CMake 构建系统
- **模块化构建** - 使用 `add_subdirectory()` 分离 boot 和 kernel
- **自动依赖检测** - CMake 自动处理文件依赖关系
- **构建类型支持** - Debug/Release 配置，支持调试符号和优化选项
- **自定义目标** - `run`, `vga-run`, `debug` 等便捷目标
- **工具链检测** - 自动查找 NASM、QEMU 等工具

#### 2. C 内核架构
- **汇编入口** (`kernel_entry.asm`) - 设置栈和 BSS，跳转到 C 代码
- **C 主函数** (`kernel_main.c`) - 内核主要逻辑用 C 编写
- **链接脚本** (`linker.ld`) - 精确控制内存布局
- **类型定义** (`types.h`) - 标准 C 类型定义

#### 3. Freestanding C 环境
- `-ffreestanding` - 不依赖标准库
- `-fno-builtin` - 禁用内置函数
- `-nostdlib` - 不链接标准库
- `-mcmodel=large` - 支持大内存模型

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 💡 经验总结

**内容**:
- 从 Makefile 迁移到 CMake 的决策过程
- C 内核架构设计思路
- Freestanding C 环境配置要点
- 遇到的挑战和解决方案

**适合**:
- 理解架构变更的原因
- 学习构建系统设计
- 了解开发历程

### 2. [技术参考](./技术参考.md) 📖 技术手册

**内容**:
- CMake 语法和命令详解
- 链接脚本语法和结构
- Freestanding C 编译选项
- 汇编与 C 代码接口规范

**适合**:
- 查询技术细节
- 理解底层原理
- 扩展构建系统

### 3. [调试工具指南](./调试工具指南.md) 🛠️ 工具参考

**内容**:
- GDB 调试 C 代码的方法
- CMake 调试构建配置
- 符号表和源码级调试
- 常用调试命令

**适合**:
- 学习调试技巧
- 解决构建问题
- 验证代码行为

### 4. [故障排查指南](./故障排查指南.md) 🔧 问题解决

**内容**:
- CMake 配置问题诊断
- 链接错误处理
- C 运行时环境问题
- 编译器标志冲突

**适合**:
- 解决具体问题
- 学习调试思路
- 避免常见错误

---

## 代码结构

### 文件组织

```
CCOperatingSystemX64/
├── CMakeLists.txt              # 根 CMake 配置
├── linker.ld                   # 内核链接脚本
├── cmake/
│   └── ccos_config.h.in        # CMake 配置头文件模板
├── boot/
│   ├── bootloader.asm          # 统一 bootloader
│   ├── lib/                    # Bootloader 库
│   └── CMakeLists.txt          # Bootloader 构建规则
├── kernel/
│   ├── kernel_entry.asm        # 内核汇编入口
│   ├── kernel_main.c           # 内核 C 主函数
│   ├── include/
│   │   └── types.h             # 类型定义
│   └── CMakeLists.txt          # 内核构建规则
└── build/                      # 构建输出目录 (自动生成)
    ├── bootloader.bin
    ├── kernel.bin
    ├── kernel.elf              # 带调试符号的 ELF
    └── boot.img
```

### 构建流程

```
┌─────────────────────────────────────────────────────────────┐
│                        CMake 配置                            │
│  cmake -B build -DCMAKE_BUILD_TYPE=Debug                    │
└───────────────────────────┬─────────────────────────────────┘
                            │
        ┌───────────────────┴───────────────────┐
        │                                       │
┌───────▼──────────────┐          ┌────────────▼──────────────┐
│  Bootloader 构建     │          │   内核构建                │
│  (NASM, -f bin)      │          │                            │
│                      │          │  ┌──────────────────────┐ │
│ bootloader.asm       │          │  │ kernel_entry.asm     │ │
│       + %include     │          │  │ (NASM -f elf64)      │ │
│       ──────────────►│          │  │         ────────────►│ │
│                      │          │  │ kernel_entry.o       │ │
│ bootloader.bin       │          │  └──────────────────────┘ │
└──────────────────────┘          │                            │
        │                         │  ┌──────────────────────┐ │
        │                         │  │ kernel_main.c        │ │
        │                         │  │ (GCC -ffreestanding) │ │
        │                         │  │         ────────────►│ │
        │                         │  │ kernel_main.o       │ │
        │                         │  └──────────────────────┘ │
        │                         │                            │
        │                         │  ┌──────────────────────┐ │
        │                         │  │ 链接 (LD)            │ │
        │                         │  │ kernel_entry.o +     │ │
        │                         │  │ kernel_main.o        │ │
        │                         │  │         ────────────►│ │
        │                         │  │ kernel.elf           │ │
        │                         │  └──────────┬───────────┘ │
        │                         │             │              │
        │                         │  ┌──────────▼───────────┐ │
        │                         │  │ objcopy -O binary    │ │
        │                         │  │         ────────────►│ │
        │                         │  │ kernel.bin           │ │
        │                         │  └──────────────────────┘ │
        └─────────────────────────┴──────────────────────────┘
                                      │
                         ┌────────────▼─────────────┐
                         │    创建启动镜像          │
                         │  bootloader.bin (sectors │
                         │   0-1) + kernel.bin     │
                         │   (sector 2+)           │
                         │         ───────────────►│
                         │  boot.img               │
                         └─────────────────────────┘
```

---

## 构建和使用

### 配置和构建

```bash
# 配置构建 (Debug 模式，带调试符号)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 构建所有目标
cmake --build build

# 构建特定目标
cmake --build build --target kernel    # 仅构建内核
cmake --build build --target bootloader # 仅构建 bootloader
cmake --build build --target boot_img   # 仅构建启动镜像
```

### 运行

```bash
# 文本模式运行 (无图形界面)
cmake --build build --target run

# VGA 图形模式运行 (VNC 后端)
cmake --build build --target vga-run
# 在另一个终端连接: vncviewer localhost:5900

# 构建并运行 (一步完成)
cmake --build build --target build-and-vga-run
```

### 调试

```bash
# 启动 QEMU 调试模式
cmake --build build --target debug

# 在另一个终端连接 GDB
gdb build/kernel.elf
(gdb) target remote :1234
(gdb) break kernel_main
(gdb) continue
```

---

## 与前一个阶段的对比

| 特性 | stage/03 | stage/04 (本阶段) |
|-----|----------|------------------|
| 构建系统 | Makefile | CMake |
| 内核语言 | 纯汇编 | C + 汇编入口 |
| 调试支持 | 无符号 | ELF + 调试符号 |
| 构建配置 | 手动编辑 Makefile | CMake option + cache |
| 依赖管理 | 手动指定 | 自动检测 |
| 构建类型 | 单一 | Debug/Release |
| 自定义目标 | .PHONY 伪目标 | native CMake targets |
| 工具链检测 | 无 | find_program |
| 交叉编译 | 困难 | 原生支持 |

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
0x80000      栈顶 (向下增长)         kernel_entry.asm 设置
0xB8000      VGA 文本缓冲区          硬件固定
```

---

## 技术要点

### Freestanding C 环境

本项目的内核运行在 **freestanding** 环境中，这意味着：

1. **无标准库** - 没有 libc、libm 等
2. **无运行时** - 没有 `main()` 的常规初始化
3. **无操作系统** - 直接运行在硬件上
4. **自包含** - 所有功能必须自己实现

编译选项说明：
- `-ffreestanding` - 声明为 freestanding 环境
- `-fno-builtin` - 禁用 GCC 内置函数（如 memcpy）
- `-nostdlib` - 不链接标准库
- `-nostdinc` - 不包含标准头文件
- `-mcmodel=large` - 大内存模型（支持超过 2GB 代码）

### 汇编与 C 接口

```assembly
; kernel_entry.asm - 汇编入口
extern kernel_main       ; 声明外部 C 函数
global kernel_start      ; 导出入口符号

kernel_start:
    cli                  ; 关中断
    mov rsp, 0x80000     ; 设置栈
    call kernel_main     ; 调用 C 函数
    jmp $                ; 死循环
```

```c
// kernel_main.c - C 主函数
void kernel_main(void) {
    // C 代码逻辑
    volatile uint16_t *vga = (uint16_t *)0xB8000;
    vga[0] = 0x1F43;  // 显示 'C'
    while (1) {
        __asm__ volatile("hlt");
    }
}
```

---

## 快速链接

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目总体进度
- [../03_unified_boots/](../03_unified_boots/) - 统一 Bootloader 文档
- [../05_load_large_kernel/](../05_load_large_kernel/) - 大内核加载文档

### 源代码
- [../../CMakeLists.txt](../../CMakeLists.txt) - 根 CMake 配置
- [../../kernel/kernel_main.c](../../kernel/kernel_main.c) - 内核 C 主函数
- [../../kernel/kernel_entry.asm](../../kernel/kernel_entry.asm) - 内核汇编入口
- [../../linker.ld](../../linker.ld) - 链接脚本

---

## 文档维护

### 更新历史
- **2026-02-16**: 创建 CMake 构建系统与 C 内核文档体系
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

**作者**: CharlieChen114514
**最后更新**: 2026-02-16