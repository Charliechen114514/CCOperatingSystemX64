# CCOS 基础库与格式化日志系统 文档中心

本目录包含 CCOS Stage 10 - 基础库与格式化日志系统开发的完整文档体系。

---

## 阶段概述

**Stage 10: 基础库与格式化日志系统**

本阶段实现了 CCOS 的基础字符串处理库和可扩展的内核格式化日志系统。

### 核心成果

- **基础字符串库** ([`kernel/base/strhelpers.h`](../../kernel/base/strhelpers.h))
  - 字符分类函数 (`isspace`, `isdigit`)
  - 字符串转数值 (`strtol`, `strtoul`, `atoi`)
  - 数值转字符串 (`itoa`, `uitoa`)
  - 字符串反转 (`reverse_str`)

- **格式化日志系统** ([`kernel/klogs/`](../../kernel/klogs/))
  - 类 printf 格式化输出 (`kprintf`, `kvprintf`)
  - 分级日志系统 (`klog_trace`, `klog_debug`, `klog_info`, `klog_warn`, `klog_error`)
  - 可扩展的后端架构 ([`kprintf_backends.h`](../../kernel/klogs/kprintf_backends.h))
  - 串口后端实现 ([`serial_backends.c`](../../kernel/klogs/backends/serial_backends.c))

- **ANSI 颜色支持** ([`kernel/driver/serial/serial_color.h`](../../kernel/driver/serial/serial_color.h))
  - 完整的颜色代码定义
  - 日志级别颜色映射
  - 终端转义序列封装

- **欢迎界面重构** ([`kernel/welcomes/`](../../kernel/welcomes/))
  - 串口彩色欢迎界面 ([`serial_welcome.c`](../../kernel/welcomes/serial_welcome.c))
  - VGA 欢迎界面 ([`vga_welcomes.c`](../../kernel/welcomes/vga_welcomes.c))
  - 统一启动接口 ([`welcome.c`](../../kernel/welcomes/welcome.c))

---

## 目录结构

```
kernel/
├── base/
│   ├── strhelpers.h        # 字符串辅助函数声明
│   ├── strhelpers.c        # 字符串辅助函数实现
│   └── help_macros.h       # 辅助宏定义
├── klogs/
│   ├── kprintf.h           # 日志系统主接口
│   ├── kprintf.c           # 日志系统核心实现
│   ├── kprintf_config.h    # 日志系统配置
│   ├── kprintf_backends.h  # 后端抽象接口
│   ├── kprintf_backends.c  # 后端管理实现
│   └── backends/
│       ├── serial_backends.h   # 串口后端接口
│       └── serial_backends.c   # 串口后端实现
├── driver/
│   └── serial/
│       ├── serial_color.h      # ANSI 颜色定义
│       └── serial_color.c      # 颜色代码生成
└── welcomes/
    ├── welcome.h          # 欢迎界面统一接口
    ├── welcome.c          # 欢迎界面调度
    ├── serial_welcome.c   # 串口欢迎界面
    └── vga_welcomes.c     # VGA 欢迎界面
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 💡 设计思路

**内容**:
- 设计决策与架构思考
- 代码组织与模块化
- 常见问题与解决方案
- 未来改进方向

**适合**:
- 理解设计思路
- 学习系统架构
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 📖 技术手册

**内容**:
- 数据结构与算法详解
- API 函数参考
- ANSI 转义序列规范
- 格式化字符串语法

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 🔧 问题诊断

**内容**:
- 常见编译错误
- 运行时问题诊断
- 日志输出异常
- 调试技巧

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

### 4. [调试工具指南](./调试工具指南.md) 🛠️ 工具使用

**内容**:
- GDB 调试日志系统
- 串口监控工具
- 内存分析技巧
- 性能分析方法

**适合**:
- 学习调试工具
- 查询具体命令
- 提升调试效率

---

## 快速开始

### 查看代码结构

1. **基础字符串库** → 查看 [`kernel/base/strhelpers.h`](../../kernel/base/strhelpers.h)
2. **日志系统接口** → 查看 [`kernel/klogs/kprintf.h`](../../kernel/klogs/kprintf.h)
3. **后端架构** → 查看 [`kernel/klogs/kprintf_backends.h`](../../kernel/klogs/kprintf_backends.h)

### 使用示例

```c
#include "klogs/kprintf.h"

// 初始化日志系统
klog_init(KLOG_BACKEND_SERIAL);

// 设置日志级别
klog_set_level(KLOG_LEVEL_INFO);

// 使用分级日志
klog_trace("这条不会被显示 (TRACE < INFO)");
klog_debug("这条也不会被显示 (DEBUG < INFO)");
klog_info("系统启动中...");
klog_warn("警告：内存不足");
klog_error("错误：设备初始化失败");

// 使用格式化输出
kprintf(KLOG_BACKEND_SERIAL, "值: %d, 指针: %p\n", 42, ptr);
```

---

## 技术亮点

### 1. 可扩展的后端架构

通过抽象接口 `KLogBackendOps`，支持多种输出后端：

```c
typedef struct {
    void (*process)(const char* str, int level);
    bool (*is_ready)(void);
} KLogBackendOps;
```

未来可轻松添加 VGA、文件、网络等后端。

### 2. 分级日志过滤

支持 5 个日志级别，运行时可动态调整过滤级别：

```
TRACE (0) < DEBUG (1) < INFO (2) < WARN (3) < ERROR (4)
```

### 3. ANSI 颜色支持

完整的 ANSI 颜色代码定义，根据日志级别自动着色：

```
TRACE  → Gray
DEBUG  → Magenta
INFO   → Green
WARN   → Yellow
ERROR  → Red
```

### 4. 自实现的字符串工具

完全自实现的字符串转换函数，不依赖标准库：

- `strtol` / `strtoul` - 支持多进制 (2-36)
- `itoa` / `uitoa` - 数值转字符串
- `isspace` / `isdigit` - 字符分类

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

## 版本信息

- **阶段**: Stage 10
- **分支**: `stage/10_base_with_format_log`
- **提交**:
  - `1463292` - polish the serial kprintf and helpers for build
  - `7193f5c` - refactor the boot welcome guis
- **日期**: 2026-02-16
- **作者**: Charliechen114514

---

## 相关资源

### 项目文档
- [../PROGRESS.md](../PROGRESS.md) - 项目进度
- [../09_memory_serial/README.md](../09_memory_serial/) - 上一阶段文档

### 源码文件
- [`kernel/base/strhelpers.h`](../../kernel/base/strhelpers.h)
- [`kernel/klogs/kprintf.h`](../../kernel/klogs/kprintf.h)
- [`kernel/klogs/kprintf_backends.h`](../../kernel/klogs/kprintf_backends.h)

### 外部参考
- [ANSI Escape Codes](https://en.wikipedia.org/wiki/ANSI_escape_code)
- [printf Format Specifiers](https://en.cppreference.com/w/c/io/fprintf)

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-16