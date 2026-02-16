# CCOS 统一 Bootloader 文档中心

本目录包含 CCOS Bootloader 在 **统一架构阶段** (stage/03_unified_boots) 的完整文档体系。

---

## 阶段概述

### 什么是统一 Bootloader？

在 `stage/02_kernel_asm_loader` 阶段，bootloader 由两个独立文件组成：
- `boot.asm` - Stage 1 (MBR)
- `boot2.asm` - Stage 2 (加载器)

在 **本阶段** (`stage/03_unified_boots`)，我们将两个阶段合并为一个文件：
- `bootloader.asm` - 包含 Stage 1 + Stage 2 的统一文件
- `boot/lib/` - 分离的可复用库函数

### 主要改进

1. **单文件架构**
   - Stage 1 和 Stage 2 合并到同一文件
   - 使用 NASM 的 `section` 和 `vstart` 指令分离代码段
   - 简化构建流程

2. **模块化库设计**
   - `boot/lib/bios_screen.asm` - BIOS 屏幕操作
   - `boot/lib/bios_string.asm` - BIOS 字符串打印
   - `boot/lib/disk_io.asm` - 磁盘 I/O 操作
   - `boot/lib/pmode.asm` - 保护模式设置
   - `boot/lib/lmode.asm` - 长模式操作

3. **代码复用**
   - 消除重复代码
   - 统一的接口设计
   - 更清晰的职责分离

4. **简化的构建系统**
   - 减少编译步骤
   - 统一的输出文件
   - 更清晰的目标定义

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 💡 经验总结

**内容**:
- 统一架构的设计决策
- 代码重构过程
- 模块化设计思路
- 遇到的挑战和解决方案

**适合**:
- 理解重构动机
- 学习模块化设计
- 了解开发历程

### 2. [技术参考](./技术参考.md) 📖 技术手册

**内容**:
- NASM section 和 vstart 详解
- 单文件多阶段架构
- 库函数接口规范
- 内存布局变化

**适合**:
- 理解技术细节
- 学习 NASM 高级特性
- 扩展库函数

### 3. [调试工具指南](./调试工具指南.md) 🛠️ 工具参考

**内容**:
- 反汇编验证技巧
- section 偏移计算
- 符号调试方法
- 常见问题诊断

**适合**:
- 学习调试技巧
- 解决链接问题
- 验证代码布局

### 4. [故障排查指南](./故障排查指南.md) 🔧 问题解决

**内容**:
- Stage 1/Stage 2 跳转问题
- section 地址计算错误
- 库函数调用失败
- 构建系统问题

**适合**:
- 解决具体问题
- 学习调试思路
- 避免常见错误

---

## 代码结构

### 文件组织

```
boot/
├── bootloader.asm          # 统一 bootloader (Stage 1 + Stage 2)
└── lib/
    ├── bios_screen.asm     # BIOS 屏幕操作库
    ├── bios_string.asm     # BIOS 字符串操作库
    ├── disk_io.asm         # 磁盘 I/O 库
    ├── pmode.asm           # 保护模式库
    └── lmode.asm           # 长模式库
```

### 内存布局

```
地址         内容                    来源
────────────────────────────────────────────
0x7C00       Stage 1 代码            section .mbr
0x7E00       Stage 2 代码            section .stage2 vstart=0x7E00
0x9000       PML4 页表               运行时创建
0xA000       PDPT 页表               运行时创建
0xB000       PD 页表                 运行时创建
0x10000      内核映像                从磁盘加载
0xB8000      VGA 文本缓冲区          硬件固定
```

---

## 构建和使用

### 编译

```bash
# 编译统一 bootloader
make

# 输出文件
# build/bootloader.bin - 包含 Stage 1 + Stage 2 的完整 bootloader
```

### 运行

```bash
# 使用 QEMU 运行
qemu-system-x86_64 -drive format=raw,file=build/boot.img -serial stdio

# 查看串口输出
qemu-system-x86_64 -drive format=raw,file=build/boot.img -serial file:serial.log
```

### 调试

```bash
# 启动 GDB 调试
qemu-system-x86_64 -drive format=raw,file=build/boot.img -s -S
gdb build/bootloader.bin

# 在 GDB 中
(gdb) target remote :1234
(gdb) break *0x7C00    # Stage 1 入口
(gdb) break *0x7E00    # Stage 2 入口
(gdb) continue
```

---

## 与前一个阶段的对比

| 特性 | stage/02 | stage/03 (本阶段) |
|-----|----------|------------------|
| 文件数量 | 2 个 (boot.asm, boot2.asm) | 1 个 (bootloader.asm) |
| 库函数 | 分散在各文件中 | 独立的 lib/ 目录 |
| 构建步骤 | 两次编译 + 合并 | 一次编译 |
| 代码复用 | 低 (重复代码多) | 高 (模块化) |
| 可维护性 | 中 | 高 |
| 扩展性 | 中 | 高 |

---

## 快速链接

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目总体进度
- [../01_bootloader/](../01_bootloader/) - 基础 bootloader 文档
- [../02_load_asm_kernel/](../02_load_asm_kernel/) - 内核加载文档

### 源代码
- [../../boot/bootloader.asm](../../boot/bootloader.asm) - 统一 bootloader 源码
- [../../boot/lib/](../../boot/lib/) - 库函数源码

---

## 文档维护

### 更新历史
- **2026-02-16**: 创建统一 bootloader 文档体系
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