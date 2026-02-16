# CCOS 字符串工具库与断言系统 文档中心

本目录包含 CCOS 在 **字符串工具库与断言系统阶段** (stage/08_string_utils) 的完整文档体系。

---

## 阶段概述

### 什么是字符串工具库与断言系统阶段？

在 `stage/07_vga_with_vscode_debug` 阶段，项目已具备 VGA 图形界面和调试能力，但缺乏基础库函数支持。

在 **本阶段** (`stage/08_string_utils`)，我们实现了两大核心基础设施：
1. **C 标准字符串函数库** - 完整的字符串操作、比较、搜索功能
2. **断言系统** - 内核级别的错误检测和报告机制

### 主要改进

#### 1. 字符串工具库 (`kernel/base/string.c`)

**基础操作**:
- `strlen`, `strnlen` - 字符串长度计算
- `strcpy`, `strncpy` - 字符串复制

**字符串比较**:
- `strcmp`, `strncmp` - 大小写敏感比较
- `strcasecmp`, `strncasecmp` - 忽略大小写比较
- `tolower` - 字符大小写转换

**字符串搜索**:
- `strchr`, `strrchr` - 字符查找
- `strstr` - 子串查找
- `strpbrk` - 字符集匹配
- `strspn`, `strcspn` - 前缀长度计算

**字符串分割**:
- `strtok` - 非线程安全分割
- `strtok_r` - 线程安全分割（可重入）

#### 2. 断言系统 (`kernel/assert/`)

**断言宏**:
- `CCOS_ASSERT` - 始终启用的断言
- `CCOS_DEBUG_ASSERT` - 仅 Debug 模式启用

**后端支持**:
- VGA 错误信息显示
- 整数转字符串工具
- 系统停止动作（`int3` + `hlt`）

#### 3. 测试框架 (`test/test_string.c`)

**测试宏**:
- `TEST_ASSERT_EQ` - 相等断言
- `TEST_ASSERT_STR_EQ` - 字符串相等断言
- `TEST_INFO` - 信息输出
- `TEST_PASS` - 测试通过标记

**主机环境支持**:
- `host_support.h` - 提供标准库替代品
- 独立的测试可执行文件

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 💡 经验总结

**内容**:
- 字符串函数实现细节和优化
- 断言系统设计思路
- 测试框架构建经验
- 遇到的挑战和解决方案

**适合**:
- 理解设计思路
- 学习库函数实现
- 了解开发历程

### 2. [技术参考](./技术参考.md) 📖 技术手册

**内容**:
- 完整的字符串函数 API 参考
- 断言系统使用指南
- 测试宏详细说明
- 性能特性分析

**适合**:
- 查询 API 详情
- 理解底层原理
- 扩展功能

---

## 代码结构

### 文件组织

```
CCOperatingSystemX64/
├── kernel/
│   ├── base/
│   │   ├── CMakeLists.txt      # 字符串库构建规则
│   │   ├── string.h            # 字符串函数声明
│   │   └── string.c            # 字符串函数实现
│   ├── assert/
│   │   ├── CMakeLists.txt      # 断言库构建规则
│   │   ├── assert.h            # 断言宏定义
│   │   ├── assert.c            # 断言实现
│   │   ├── assert_action_backend.h  # 后端接口
│   │   └── assert_action_backend.c  # 后端实现
│   └── defines/
│       └── types.h             # 类型定义（新增 bool）
├── test/
│   ├── CMakeLists.txt          # 测试构建规则（更新）
│   ├── include/
│   │   └── host_support.h      # 主机环境支持（新增）
│   └── test_string.c           # 字符串函数测试（新增）
└── build/                      # 构建输出目录
    ├── libkernel_base.a        # 静态库
    └── test_string             # 测试可执行文件
```

### 新增文件概览

| 文件 | 类型 | 功能描述 |
|------|------|----------|
| `string.c/h` | C | 完整的 C 标准字符串函数实现 |
| `assert.c/h` | C | 断言宏和实现 |
| `assert_action_backend.c/h` | C | 断言失败后的 VGA 报告和系统停止 |
| `test_string.c` | C | 字符串函数的单元测试 |
| `host_support.h` | C | 主机环境的测试支持宏 |
| `types.h` | C | 新增 `bool` 类型定义 |

### 字符串函数分类

```
┌─────────────────────────────────────────────────────────┐
│                   字符串函数库                          │
└───────────────────────┬─────────────────────────────────┘
                        │
        ┌───────────────┼───────────────┐
        │               │               │
    ┌───▼───────┐  ┌───▼──────┐  ┌───▼──────────┐
    │  基础操作   │  │  比较    │  │    搜索      │
    │            │  │         │  │             │
    │ strlen     │  │ strcmp  │  │ strchr      │
    │ strnlen    │  │ strncmp │  │ strrchr     │
    │ strcpy     │  │ strcasecmp │ strstr     │
    │ strncpy    │  │ strncasecmp │ strpbrk    │
    │            │  │ tolower │  │ strspn      │
    └────────────┘  └──────────┘  │ strcspn     │
                                    └─────────────┘

        ┌───────────────────────────────────────┐
        │           字符串分割                   │
        │                                       │
        │  strtok      - 非线程安全             │
        │  strtok_r    - 线程安全（可重入）     │
        └───────────────────────────────────────┘
```

---

## 构建和使用

### 配置和构建

```bash
# 配置构建
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 构建所有目标（包括内核和测试）
cmake --build build

# 仅构建字符串库
cmake --build build --target kernel_base

# 构建测试
cmake --build build --target test_string
```

### 运行测试

```bash
# 运行字符串函数测试
./build/test/test_string

# 输出示例
# [INFO] Testing strlen...
# [PASS] strlen: all tests passed
# [INFO] Testing strnlen...
# [PASS] strnlen: all tests passed
# ...
# ==========================================
# Test Summary: X passed, 0 failed
# ==========================================
```

### 在内核中使用

```c
#include "base/string.h"
#include "assert/assert.h"

void example_function(void) {
    // 使用字符串函数
    char buffer[64];
    strcpy(buffer, "Hello");
    size_t len = strlen(buffer);

    // 使用断言
    CCOS_ASSERT(len == 5);
    CCOS_DEBUG_ASSERT(buffer[0] == 'H');
}
```

---

## 与前一个阶段的对比

| 特性 | stage/07 | stage/08 (本阶段) |
|-----|----------|------------------|
| 字符串处理 | 无标准实现 | 完整的 C 标准字符串库 |
| 断言支持 | 无 | 完整的断言系统 |
| 错误报告 | 无 | VGA 错误显示 + 调试器支持 |
| 测试框架 | 无 | 完整的单元测试支持 |
| `bool` 类型 | 无 | 新增到 `types.h` |
| 大小写转换 | 无 | `tolower` 实现 |
| 字符串分割 | 无 | `strtok` / `strtok_r` |
| 主机测试 | 不支持 | 独立的测试可执行文件 |

---

## 技术要点

### Freestanding 环境下的字符串实现

本项目在 freestanding 环境下实现字符串函数：

1. **无标准库依赖** - 不依赖 `<string.h>`
2. **自定义类型** - `size_t` 来自 `types.h`
3. **内联优化** - 简单函数使用内联展开
4. **边界检查** - 使用断言验证输入

### 断言系统设计

```
┌─────────────────────────────────────────────────────────┐
│                    断言宏                               │
│                                                         │
│  CCOS_ASSERT(x)        - 始终启用                       │
│  CCOS_DEBUG_ASSERT(x)  - 仅 Debug 模式                  │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│              ccos_assert_impl()                         │
│                                                         │
│  - 检查条件                                             │
│  - 收集上下文信息（文件、行、函数、表达式）             │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│            assert_backend_to_vga()                      │
│                                                         │
│  - 在 VGA 显示错误信息                                  │
│  - 白字红底，醒目显示                                   │
│  - 显示文件、行号、函数、表达式                          │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│           assert_failed_action()                        │
│                                                         │
│  - Debug: int3 断点（通知调试器）                       │
│  - Release: cli + hlt（停止系统）                       │
└─────────────────────────────────────────────────────────┘
```

### 测试策略

```
┌─────────────────────────────────────────────────────────┐
│                   test_string.c                         │
└───────────────────────┬─────────────────────────────────┘
                        │
        ┌───────────────┴───────────────┐
        │                               │
    ┌───▼─────────┐           ┌────────▼────────┐
    │  主机环境    │           │   内核环境       │
    │             │           │                 │
    │ - 使用标准库 │           │ - 使用 kernel_  │
    │ - 独立编译   │           │   base.a        │
    │ - 快速验证   │           │ - 集成测试      │
    └─────────────┘           └─────────────────┘
```

---

## 快速链接

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目总体进度
- [../07_vga_with_vscode_debug/](../07_vga_with_vscode_debug/) - VGA 图形界面文档
- [../09_memory_serial/](../09_memory_serial/) - 内存与串口文档

### 源代码
- [../../kernel/base/string.c](../../kernel/base/string.c) - 字符串函数实现
- [../../kernel/base/string.h](../../kernel/base/string.h) - 字符串函数声明
- [../../kernel/assert/](../../kernel/assert/) - 断言系统
- [../../test/test_string.c](../../test/test_string.c) - 单元测试

---

## 文档维护

### 更新历史
- **2026-02-16**: 创建字符串工具库与断言系统文档体系
  - README.md
  - 开发笔记.md
  - 技术参考.md

### 贡献指南
欢迎改进和补充文档：

1. **发现错误** → 直接修改并提交 PR
2. **补充案例** → 在相应文档添加
3. **新增章节** → 遵循现有格式

---

**文档维护者**: Claude Code + User
**最后更新**: 2026-02-16
**文档版本**: 1.0.0
