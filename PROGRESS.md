# CCOS Bootloader 开发进度

## 项目概述

两阶段引导程序，支持实模式 → 保护模式 → 长模式的完整启动流程。

## 已完成功能

### 1. 两阶段加载 (Two-Stage Bootloader)
- Stage 1 (MBR): BIOS 加载到 0x7c00，负责加载 Stage 2
- Stage 2: 加载到 0x7E00，执行核心初始化
- 使用 LBA 扩展读取，CHS 作为后备方案

### 2. 模式切换
| 模式 | 状态 | 说明 |
|------|------|------|
| 实模式 | ✅ | BIOS 中断打印字符 |
| 保护模式 | ✅ | GDT 设置完成 |
| 长模式 | ✅ | 页表 + PAE + 64位代码段 |

### 3. 显示输出
- BIOS 中断 (INT 10h, AH=0Eh)
- VGA 直接写显存 (0xB8000)

## 项目结构

```
CCOperatingSystem/
├── boot/              # 源代码
│   ├── boot.asm       # Stage 1 (MBR)
│   └── boot2.asm      # Stage 2
├── build/             # 构建产物
├── document/          # 文档
├── Makefile           # 构建脚本
├── run.sh             # 启动脚本
└── PROGRESS.md        # 本文件
```

## 下一步计划

- [ ] 加载内核镜像
- [ ] 内存检测 (Memory Detection)
- [ ] 中断描述符表 (IDT)
- [ ] 键盘驱动

## 更新日志

### 2026-02-14
- ✅ 修复 Stage 2 加载问题（DAP 结构、读取扇区数）
- ✅ 修复保护模式切换（GDT base 地址、far jump 编码）
- ✅ 成功进入 64 位长模式
- ✅ 优化项目目录结构
