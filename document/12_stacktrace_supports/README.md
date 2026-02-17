# CCOS 栈回溯支持 文档中心

本目录包含 CCOS Stage 12 - 栈回溯支持开发的完整文档体系。

---

## 阶段概述

**Stage 12: 栈回溯支持**

本阶段实现了 CCOS 的栈回溯 (stack trace) 功能，为内核调试提供强大的调用栈追踪能力。

### 核心成果

- **栈回溯系统** ([`kernel/stacktrace/stacktrace.h`](../../kernel/stacktrace/stacktrace.h))
  - 基于帧指针 (RBP) 的栈遍历
  - 可配置的栈帧深度限制
  - 栈帧有效性验证
  - 循环检测机制

- **符号解析系统** ([`kernel/stacktrace/symbol.h`](../../kernel/stacktrace/symbol.h))
  - 地址到符号名的映射
  - 偏移量计算 (function + 0xoffset)
  - 二分查找优化
  - 自动符号表生成

- **符号表构建** ([`scripts/extract_symbols.py`](../../scripts/extract_symbols.py))
  - 从 ELF 文件提取符号
  - 自动生成符号表代码
  - 字符串表优化

- **美观的输出格式**
  - 表格化栈回溯显示
  - 符号名高亮显示
  - 集成现有日志系统

---

## 目录结构

```
kernel/
├── stacktrace/
│   ├── stacktrace.h         # 栈回溯接口
│   ├── stacktrace.c         # 栈回溯实现
│   ├── symbol.h             # 符号表接口
│   ├── symbol.c             # 符号表实现
│   ├── symbols.h            # 符号表声明
│   ├── symbols.c            # 自动生成的符号表
│   └── CMakeLists.txt       # 构建配置
└── scripts/
    └── extract_symbols.py   # 符号提取脚本
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 设计决策与架构思考
- x86_64 调用约定解析
- 符号表构建方案
- 常见问题与解决方案
- 未来改进方向

**适合**:
- 理解设计思路
- 学习系统架构
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- x86_64 栈帧结构详解
- 符号表数据结构
- API 函数参考
- 符号提取脚本说明

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 问题诊断

**内容**:
- 常见编译错误
- 符号表生成问题
- 栈回溯异常处理
- 调试技巧

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

### 4. [调试工具指南](./调试工具指南.md) 工具使用

**内容**:
- GDB 调试栈回溯
- 符号表验证工具
- QEMU 调试技巧
- 性能分析方法

**适合**:
- 学习调试工具
- 查询具体命令
- 提升调试效率

---

## 快速开始

### 查看代码结构

1. **栈回溯接口** → 查看 [`kernel/stacktrace/stacktrace.h`](../../kernel/stacktrace/stacktrace.h)
2. **符号表接口** → 查看 [`kernel/stacktrace/symbol.h`](../../kernel/stacktrace/symbol.h)
3. **符号提取脚本** → 查看 [`scripts/extract_symbols.py`](../../scripts/extract_symbols.py)

### 使用示例

```c
#include "stacktrace/stacktrace.h"

// 在任意位置打印栈回溯
void some_function() {
    // 打印默认 16 层栈帧
    dump_stack_full();

    // 或指定最大帧数
    dump_stack(32);
}

// 在错误处理中使用
void error_handler(const char* msg) {
    klog_error("Error: %s\n", msg);
    klog_error("Stack trace:\n");
    dump_stack_full();
    system_halt();
}
```

### 示例输出

```
╔═══════════════════════════════════════════════════════════════╗
║                      Stack Trace                             ║
╠═══════════════════════════════════════════════════════════════╣
║  RSP: 0x0000000000007f00                                    ║
║  RBP: 0x0000000000007f20                                    ║
╠═══════════════════════════════════════════════════════════════╣
║  #  Address           Function                           ║
╠═══════════════════════════════════════════════════════════════╣
║  0   0x000000000001001a  kernel_main                    ║
║  1   0x0000000000007c00  kernel_start                   ║
╚═══════════════════════════════════════════════════════════════╝
```

---

## 技术亮点

### 1. 基于 x86_64 调用约定的栈遍历

利用 System V AMD64 ABI 的帧指针约定：

```c
// 每个栈帧的结构:
// +--------+ <---- RBP
// | old RBP|
// +--------+
// | ret RIP|
// +--------+
// |  ...   |  局部变量
// +--------+ <---- RSP
```

### 2. 符号表二分查找

符号表按地址排序，使用二分查找快速定位：

```c
// 时间复杂度: O(log n)
// 空间复杂度: O(1)
bool find_symbol(uintptr_t addr, const char** out_name, uintptr_t* out_offset);
```

### 3. 自动化符号表生成

Python 脚本从 ELF 文件自动提取符号：

```bash
# 构建时自动执行
python scripts/extract_symbols.py build/kernel/kernel.elf
```

### 4. 安全验证机制

- 栈帧对齐检查 (8 字节对齐)
- 栈边界验证
- 循环检测 (防止无限循环)
- 最大帧数限制 (防止栈溢出)

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

- **阶段**: Stage 12
- **分支**: `stage/12_stacktrace_supports`
- **提交**:
  - `8871b91` - remove timestamp
  - `1d1f81e` - support dump stacks
  - `6f0c411` - stacktrace supports
  - `5b5451b` - finish the merge of stage11
- **日期**: 2026-02-17
- **作者**: Charliechen114514

---

## 相关资源

### 项目文档
- [../PROGRESS.md](../PROGRESS.md) - 项目进度
- [../11_list_math_bitmaps_as_base_finished/README.md](../11_list_math_bitmaps_as_base_finished/) - 上一阶段文档

### 源码文件
- [`kernel/stacktrace/stacktrace.h`](../../kernel/stacktrace/stacktrace.h)
- [`kernel/stacktrace/symbol.h`](../../kernel/stacktrace/symbol.h)
- [`scripts/extract_symbols.py`](../../scripts/extract_symbols.py)

### 外部参考
- [System V AMD64 ABI](https://gitlab.com/x86-psABIs/x86-64-ABI)
- [ELF Format](https://refspecs.linuxfoundation.org/elf/elf.pdf)
- [DWARF Debugging Format](https://dwarfstd.org/)

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-17
