# CCOS 内存检测与物理帧管理 文档中心

本目录包含 CCOS Stage 15 - 内存检测与物理帧管理的完整文档体系。

---

## 阶段概述

**Stage 15: 内存检测与物理帧管理**

本阶段实现了操作系统内存管理的基础设施，包括 BIOS 内存地图检测和基于位图的物理帧分配器。这是实现虚拟内存、堆管理器等高级内存功能的前提。

### 核心成果

- **E820 内存地图检测** ([`kernel/mm/memory_detect/e820.h`](../../kernel/mm/memory_detect/e820.h))
  - Bootloader 端三种 BIOS 内存检测方法（E820/E801/88h）
  - 内存地图存储到固定地址 0xC000
  - 内核端完整的 E820 解析与查询接口
  - 内存统计与可用性检查

- **物理帧分配器** ([`kernel/mm/pframe/pframe.h`](../../kernel/mm/pframe/pframe.h))
  - 基于位图的 4KB 物理帧管理
  - 单帧和多帧连续分配
  - 自动标记内核和保留区域
  - 内存统计与调试支持

- **内存配置生成系统** ([`cmake/MemConfig.cmake`](../../cmake/MemConfig.cmake))
  - Bootloader 和内核共享的内存配置头文件
  - 编译时自动生成 C 头文件和汇编 INC 文件
  - 支持内存大小、检测地址等参数配置

- **辅助工具模块** ([`kernel/mm/memory_detect/memory_state_helper.h`](../../kernel/mm/memory_detect/memory_state_helper.h))
  - 内存布局验证
  - 区域重叠检测
  - 内存状态导出功能

- **构建系统重构** ([`cmake/QemuTargets.cmake`](../../cmake/QemuTargets.cmake))
  - QEMU 运行目标模块化
  - 统一的运行配置管理

---

## 目录结构

```
kernel/
├── mm/
│   ├── memory_detect/
│   │   ├── e820.h                      # E820 内存地图接口
│   │   ├── e820.c                      # E820 内存地图实现
│   │   ├── memory_state_helper.h       # 内存状态辅助工具
│   │   ├── memory_state_helper.c       # 内存状态辅助实现
│   │   └── CMakeLists.txt              # 构建配置
│   └── pframe/
│       ├── pframe.h                    # 物理帧分配器接口
│       ├── pframe.c                    # 物理帧分配器实现
│       └── CMakeLists.txt              # 构建配置
cmake/
├── MemConfig.cmake                     # 内存配置生成脚本
├── MemConfig.h.in                      # C 头文件模板
├── MemConfig.inc.in                    # 汇编 INC 文件模板
├── MemSizeConfig.cmake                 # 内存大小配置脚本
├── QemuTargets.cmake                   # QEMU 运行目标配置
boot/
├── bootloader.asm                      # 新增内存检测功能
└── serial_constants.inc                # 串口常量定义
scripts/
└── validate_e820_config.py             # E820 配置验证脚本
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要内存检测和物理帧管理
- 设计决策（E820 选择、位图分配器、内存布局）
- 架构设计与模块协作
- Bootloader 与内核的数据传递机制
- 常见陷阱与注意事项
- 未来改进方向

**适合**:
- 理解设计思路
- 学习系统架构
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- BIOS 内存检测技术详解（E820/E801/INT 15h/88h）
- E820 内存条目格式与类型定义
- 物理帧分配器 API 完整参考
- 位图分配算法详解
- 内存配置生成系统
- 数据结构定义与常量

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 问题诊断

**内容**:
- 内存检测失败问题
- 物理帧分配异常
- 内存地图解析错误
- QEMU 内存配置问题
- 内核崩溃相关排查

每个问题按"症状-原因-解决方案"格式组织。

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

### 4. [调试工具指南](./调试工具指南.md) 工具使用

**内容**:
- QEMU 内存调试技巧
- E820 配置验证脚本使用
- 物理帧分配器调试方法
- 内存状态查看工具
- 常见调试命令

**适合**:
- 学习调试工具
- 查询具体命令
- 提升调试效率

---

## 快速开始

### 查看代码结构

1. **E820 内存地图接口** → 查看 [`kernel/mm/memory_detect/e820.h`](../../kernel/mm/memory_detect/e820.h)
2. **物理帧分配器接口** → 查看 [`kernel/mm/pframe/pframe.h`](../../kernel/mm/pframe/pframe.h)
3. **内存配置定义** → 查看 [`ccos_config.h`](../../build/include/ccos_config.h) (构建生成)
4. **Bootloader 内存检测** → 查看 [`boot/bootloader.asm`](../../boot/bootloader.asm)

### 使用示例

```c
#include "mm/memory_detect/e820.h"
#include "mm/pframe/pframe.h"

// 内核初始化时调用
void kernel_init(void) {
    // 1. 初始化 E820 内存地图解析
    e820_init();

    // 2. 打印内存地图（调试用）
    e820_dump_map();

    // 3. 获取内存统计
    mem_stats_t stats;
    e820_get_stats(&stats);
    klog_info("Total: %u MB, Usable: %u MB\n",
              stats.total_mb, stats.usable_mb);

    // 4. 初始化物理帧分配器
    pframe_init();

    // 5. 打印帧分配器状态
    pframe_dump();
}

// 分配物理帧
void allocate_frames(void) {
    physical_addr_t addr;

    // 分配单个帧
    if (pframe_alloc(&addr) == PFRAME_OK) {
        klog_info("Allocated frame at: 0x%llx\n", addr);
    }

    // 分配多个连续帧
    if (pframe_alloc_n(&addr, 4) == PFRAME_OK) {
        klog_info("Allocated 4 frames at: 0x%llx\n", addr);
    }

    // 释放帧
    pframe_free(addr);
    pframe_free_n(addr, 4);
}

// 检查内存区域可用性
void check_memory_usability(void) {
    // 检查指定范围是否可用
    if (e820_is_range_usable(0x100000, 0x1000)) {
        klog_info("1MB+4KB region is usable\n");
    }

    // 查找可用内存区域
    uint64_t base, length;
    if (e820_find_usable_range(0x100000, 0x10000, &base, &length)) {
        klog_info("Found usable region: 0x%llx - 0x%llx\n",
                  base, base + length);
    }
}
```

---

## 与前一阶段对比

| 特性 | Stage 14 (更多中断设备) | Stage 15 (内存检测与物理帧) |
|------|-------------------------|----------------------------|
| 内存检测 | 无 | E820/E801/88h 三种方法 |
| 物理内存管理 | 无 | 位图帧分配器 |
| 内存配置 | 硬编码 | CMake 自动生成配置 |
| 构建系统 | 内联运行目标 | 模块化 QEMU 配置 |
| 内存布局约定 | 无 | 明确定义的内存布局 |
| 新增文件 | 25+ 个 | 20+ 个 |
| 代码行数 | ~5000+ | ~4000+ |

---

## 技术亮点

### 1. 三级回退内存检测

Bootloader 实现了完整的 BIOS 内存检测回退链：

```asm
; 首选: E820 - 详细内存地图
call detect_memory_e820
jc .try_e801

; 回退1: E801 - 两个内存区域
call detect_memory_e801
jc .try_88h

; 回退2: INT 15h/88h - 最大64MB
call detect_memory_88h
```

### 2. Bootloader-Kernel 数据传递

通过固定地址 0xC000 传递内存地图：

```c
// E820_STORAGE_ADDR 在 bootloader 和 kernel 间共享
#define E820_STORAGE_ADDR  0xC000
#define E820_MAX_ENTRIES   128
```

### 3. 位图帧分配算法

```c
// 每个位对应一个 4KB 帧
// 位图大小 = (总内存 / 4KB) / 8 字节

typedef struct {
    uint64_t* bitmap;        // 位图数组
    uint64_t bitmap_size;    // 位图大小（位数）
    uint64_t total_frames;   // 总帧数
    uint64_t managed_start;  // 管理内存起始地址
    uint64_t managed_end;    // 管理内存结束地址
} pframe_allocator_t;
```

### 4. 内存配置自动生成

CMake 在构建时自动生成配置文件：

```cmake
# 生成 C 头文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/MemConfig.h.in
    ${CMAKE_BINARY_DIR}/include/mem_config.h
)

# 生成汇编 INC 文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/MemConfig.inc.in
    ${CMAKE_BINARY_DIR}/mem_detect_constants.inc
)
```

---

## 文档关系图

```
                    ┌──────────────────┐
                    │   项目根目录    │
                    │  (PROGRESS.md)   │
                    └────────┬─────────┘
                             │
                    ┌────────▼─────────┐
                    │  document/       │
                    └────────┬─────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
┌───────▼──────┐   ┌─────▼──────┐   ┌─────▼──────┐
│ 开发笔记     │   │ 技术参考   │   │ 故障排查   │
└──────────────┘   └─────────────┘   └─────────────┘
        │                    │
        └──────────────────┬─────────┘
                           │
                    ┌──────▼──────┐
                    │ 调试工具指南 │
                    └─────────────┘
```

---

## 内存布局约定

| 地址范围 | 大小 | 用途 |
|---------|------|------|
| 0x0 - 0x7FFF | 32KB | BIOS 数据区 |
| 0x7C00 - 0x7DFF | 512B | MBR (Stage 1) |
| 0x7E00 - 0x8FFF | ~4KB | Stage 2 Bootloader |
| 0x9000 - 0xBFFF | 12KB | 页表 (PML4/PDPT/PD) |
| **0xC000 - 0xFFFF** | **16KB** | **内存地图存储** |
| 0x10000+ | - | 内核加载区域 |

---

## 版本信息

- **阶段**: Stage 15
- **分支**: `stage/15_memdetect_with_pframe`
- **提交**: `4b13ae8` - physical frame managed
- **日期**: 2026-02-18
- **作者**: Charliechen114514

---

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../14_more_intr_devices/README.md](../14_more_intr_devices/) - 上一阶段文档

### 源码文件
- [`kernel/mm/memory_detect/e820.h`](../../kernel/mm/memory_detect/e820.h)
- [`kernel/mm/pframe/pframe.h`](../../kernel/mm/pframe/pframe.h)
- [`boot/bootloader.asm`](../../boot/bootloader.asm)
- [`cmake/MemConfig.cmake`](../../cmake/MemConfig.cmake)

### 外部参考
- [BIOS E820 Memory Map](https://wiki.osdev.org/Detecting_Memory_(x86))
- [Physical Memory Management](https://wiki.osdev.org/Paging)
- [x86 Memory Map](https://wiki.osdev.org/Memory_Map_(x86))
- [OSDev.org](https://wiki.osdev.org)

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-18
