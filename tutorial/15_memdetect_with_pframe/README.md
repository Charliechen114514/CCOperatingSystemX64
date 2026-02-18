# CCOS 内存检测与物理帧管理 - 教程

本阶段将实现 BIOS 内存检测和物理帧分配器，这是内核内存管理的基础。

---

## 阶段概述

### 你将学到什么

| 技能 | 描述 |
|------|------|
| BIOS 内存检测 | INT 15h/E820、E801、88h 三种方法 |
| 三级回退策略 | 兼容新旧硬件的检测方案 |
| 实模式汇编 | 在 Bootloader 中调用 BIOS 中断 |
| 位图分配器 | O(1) 复杂度的物理内存管理 |
| 内存地图解析 | 从 BIOS 获取系统内存布局 |
| CMake 配置生成 | Bootloader 和内核共享常量 |

### 与前一个阶段的对比

| 特性 | stage/14 | stage/15 (本阶段) |
|-----|----------|------------------|
| 内存检测 | ❌ 无 | ✅ E820/E801/88h 三级回退 |
| 内存地图 | ❌ 硬编码 | ✅ 动态解析 |
| 物理帧分配 | ❌ 无 | ✅ 位图分配器 |
| 可用内存查询 | ❌ 无 | ✅ e820_is_range_usable() |
| 动态内存分配 | ❌ 无 | ✅ pframe_alloc/free |

---

## 文档导航

本教程包含 10 篇文档，按开发顺序排列：

### 1. [为什么内核需要知道内存地图](./01_为什么内核需要知道内存地图.md)
**动机和问题分析**

- 当前内核对内存一无所知的困境
- 硬编码内存布局的局限性
- 虚拟内存管理的前置条件

### 2. [BIOS 内存检测的三把钥匙](./02_BIOS内存检测的三把钥匙.md)
**三种检测方法介绍**

- E820 详细内存地图
- E801 两区域检测
- INT 15h/88h 传统方法
- 为什么需要三级回退策略

### 3. [从零实现 E820 内存检测](./03_从零实现E820内存检测.md)
**Bootloader 端 E820 实现**

- 实模式汇编调用 INT 15h/E820
- E820 条目结构与调用约定
- 连续值处理与签名验证
- 内存地图存储布局

### 4. [回退方案 E801 与 88h](./04_回退方案E801与88h.md)
**兼容性回退实现**

- E801 两区域检测实现
- 88h 传统方法实现
- 转换为统一 E820 格式
- 回退流程设计

### 5. [内核解析内存地图](./05_内核解析内存地图.md)
**内核端 E820 解析器**

- 从固定地址读取内存地图
- e820_init() 实现
- 查询与统计 API
- 内存类型判断

### 6. [位图分配器设计与实现](./06_位图分配器设计与实现.md)
**位图数据结构**

- 为什么选择位图方案
- 位图数据结构设计
- 核心位操作实现
- bitmap API 使用

### 7. [物理帧分配器实战](./07_物理帧分配器实战.md)
**物理帧分配器实现**

- pframe_init() 初始化流程
- 分配/释放算法实现
- 保留区域处理
- 连续帧分配

### 8. [CMake 配置同步方案](./08_CMake配置同步方案.md)
**常量共享方案**

- Bootloader 与内核常量共享问题
- configure_file 模板生成
- 构建系统集成
- 验证脚本使用

### 9. [调试与验证](./09_调试与验证.md)
**调试和测试**

- 串口输出内存地图
- 位图可视化调试
- QEMU 内存配置
- 常见问题排查

### 10. [完整测试与总结](./10_完整测试与总结.md)
**端到端测试**

- 完整编译运行流程
- 预期输出验证
- 性能分析
- 未来改进方向

---

## 快速开始

### 环境要求

```bash
# 检查工具
nasm -v          # NASM version 2.x.x
gcc --version    # gcc (Ubuntu xx.x.x.x) xx.x.x
cmake --version  # cmake version x.x.x
qemu-system-x86_64 --version  # QEMU emulator version x.x.x
python3 --version # Python 3.x (验证脚本需要)
```

### 构建和运行

```bash
# 配置构建
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 构建
cmake --build build

# 运行 QEMU（至少 128MB 内存）
qemu-system-x86_64 -m 128M -drive format=raw,file=build/boot.img \
    -nographic -serial mon:stdio

# 验证内存配置
python3 scripts/validate_e820_config.py
```

### 预期输出

```
[MEM] Detecting system memory...
[MEM] Method: E820 (detailed map)
=== CCOS Bootloader v1.0 ===
[Serial] Entering protected mode...
[Serial] Entering long mode...
=== CCOS Kernel ===
[E820] Memory Map (method: E820, entries: X)
[E820] Summary: Total=XXX MB, Usable=XXX MB
[PFRAME] Initialized: XXX frames
[PFRAME] Free: XXX, Reserved: XXX, Allocated: XXX
```

---

## 关键代码文件

```
boot/
└── bootloader.asm           # Bootloader（内存检测汇编实现）

kernel/mm/memory_detect/
├── e820.c                   # E820 解析器实现
├── e820.h                   # E820 API 定义
└── memory_state_helper.c    # 内存状态辅助函数

kernel/mm/pframe/
├── pframe.c                 # 物理帧分配器实现
└── pframe.h                 # 分配器 API 定义

kernel/bitmap/
├── bitmap.c                 # 位图操作实现
└── bitmap.h                 # 位图 API 定义

cmake/
├── MemConfig.cmake          # 内存配置生成脚本
├── MemConfig.h.in           # C 头文件模板
└── MemConfig.inc.in         # 汇编 INC 文件模板

scripts/
└── validate_e820_config.py  # 内存配置验证脚本
```

---

## 技术要点

### E820 内存类型

| 类型值 | 名称 | 描述 |
|--------|------|------|
| 1 | Usable | 可用 RAM |
| 2 | Reserved | 保留区域 |
| 3 | ACPI Reclaimable | ACPI 可回收 |
| 4 | ACPI NVS | ACPI NVS |
| 5 | Unusable | 不可用 |

### 三级回退策略

```
E820 (首选) → E801 (回退1) → 88h (回退2) → 失败
```

- E820: 详细内存地图，支持多种类型
- E801: 简单快速，只区分 1MB 上下
- 88h: 最简单，最大 64MB

### 物理帧分配

```
地址 → 帧号 → 位图索引
0x1000 → 帧 1 → bitmap[0] 的第 1 位
0x2000 → 帧 2 → bitmap[0] 的第 2 位
...
```

- 每帧 4KB (PAGE_SHIFT = 12)
- 每帧 1 bit
- O(1) 分配释放

---

## 常见问题

### Q: 为什么需要三级回退策略？

A: 不同年代的 BIOS 支持不同的内存检测方法。E820 是最详细的，但老机器可能不支持。三级回退确保最大兼容性。

### Q: 为什么内存地图存储在 0x6000？

A: 这个地址位于 BIOS 数据区 (0x0000-0x0500) 和 Stage 1 MBR (0x7C00) 之间的安全空隙，不会与其他组件冲突。

### Q: 位图分配器的开销是多少？

A: 每帧 1 bit。对于 1GB 内存，位图大小为 32KB。这是固定开销，与实际使用量无关。

### Q: 为什么物理帧分配从 1MB 开始？

A: 低 1MB 包含 BIOS 数据区、页表等关键区域，我们将其保留，从 1MB 以上开始分配。

---

## 下一步

完成本阶段后，你将掌握：

- ✅ BIOS 内存检测方法
- ✅ 实模式汇编编程
- ✅ 位图数据结构
- ✅ 物理内存管理
- ✅ CMake 配置生成

下一阶段我们将基于此实现**虚拟内存管理（VMM）**，实现页表映射和缺页异常处理。

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-18
