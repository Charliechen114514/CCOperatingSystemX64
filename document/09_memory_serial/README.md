# CCOS 内存操作与串口驱动 文档中心

本目录包含 CCOS 在 **内存操作与串口驱动阶段** (stage/09_memory_serial) 的完整文档体系。

---

## 阶段概述

### 什么是内存操作与串口驱动阶段？

在 `stage/08_string_utils` 阶段，项目已实现字符串工具函数，但缺乏内存操作函数和串口调试输出能力。

在 **本阶段** (`stage/09_memory_serial`)，我们进行了两大核心改进：
1. **实现内存操作函数库** - 提供 memset、memcpy、memmove、memcmp 等标准内存操作
2. **实现串口驱动** - 基于 UART 16550 的串口驱动，支持内核调试输出

### 主要改进

#### 1. 内存操作函数库 (kernel/base/memory.c/h)
- **memset** - 内存填充
- **memcpy** - 内存拷贝（非重叠）
- **memmove** - 内存拷贝（支持重叠）
- **memcmp** - 内存比较

#### 2. 串口驱动 (kernel/driver/serial/)
- **UART 16550 驱动** - 符合 PC 标准的串口驱动
- **115200 8N1 配置** - 标准调试波特率
- **阻塞式输出** - 同步串口输出函数
- **ANSI 颜色支持** - 终端颜色转义序列

#### 3. 端口 I/O 操作 (kernel/driver/io/)
- **inb/outb** - x86 端口 I/O 指令封装
- **内联汇编** - 高效的硬件访问

#### 4. Bootloader 串口支持
- **16 位实模式串口初始化**
- **32 位保护模式串口支持**
- **64 位长模式串口支持**
- **全启动流程串口输出**

#### 5. 单元测试框架
- **主机端测试环境** - 在宿主机上运行内核代码测试
- **内存函数完整测试** - 覆盖边界条件和特殊场景

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 💡 经验总结

**内容**:
- 内存函数实现思路
- 串口驱动架构设计
- Bootloader 串口集成
- 测试框架搭建经验

**适合**:
- 理解设计思路
- 学习硬件驱动开发
- 了解开发历程

### 2. [技术参考](./技术参考.md) 📖 技术手册

**内容**:
- 内存操作 API 完整参考
- UART 16550 寄存器详解
- 端口 I/O 操作规范
- 测试框架 API

**适合**:
- 查询 API 详情
- 理解底层原理
- 扩展驱动功能

### 3. [调试工具指南](./调试工具指南.md) 🛠️ 工具参考

**内容**:
- 串口调试配置
- QEMU 串口重定向
- 主机端测试运行
- 调试最佳实践

**适合**:
- 学习调试技巧
- 配置调试环境
- 提升开发效率

### 4. [故障排查指南](./故障排查指南.md) 🔧 问题解决

**内容**:
- 串口输出异常诊断
- 内存函数 bug 处理
- 测试失败排查
- 编译链接问题

**适合**:
- 解决具体问题
- 学习调试思路
- 避免常见错误

---

## 代码结构

### 文件组织

```
CCOperatingSystemX64/
├── boot/
│   └── bootloader.asm                 # Bootloader（重大更新：串口支持）
├── kernel/
│   ├── base/
│   │   ├── memory.h                   # 内存操作声明（新增）
│   │   ├── memory.c                   # 内存操作实现（新增）
│   │   └── CMakeLists.txt             # 更新
│   ├── driver/
│   │   ├── serial/
│   │   │   ├── serial.h               # 串口驱动声明（新增）
│   │   │   ├── serial.c               # 串口驱动实现（新增）
│   │   │   └── serial_config.h        # 串口配置（新增）
│   │   ├── io/
│   │   │   ├── io.h                   # 端口 I/O 声明（新增）
│   │   │   └── io.c                   # 端口 I/O 实现（新增）
│   │   └── CMakeLists.txt             # 更新
│   ├── kernel_init.c                  # 初始化（更新：串口初始化）
│   └── kernel_main.c                  # 主函数（更新：串口输出）
├── test/
│   ├── test_memory.c                  # 内存函数测试（新增）
│   ├── assert_stub.c                  # 断言桩实现（新增）
│   └── CMakeLists.txt                 # 测试配置（更新）
└── build/
    ├── bootloader.bin                 # 更新（包含串口代码）
    └── kernel.bin                     # 更新（包含串口驱动）
```

### 新增文件概览

| 文件 | 类型 | 功能描述 |
|------|------|----------|
| `memory.c/h` | C | 标准 C 内存操作函数实现 |
| `serial.c/h` | C | UART 16550 串口驱动 |
| `serial_config.h` | C | 串口寄存器定义 |
| `io.c/h` | C | x86 端口 I/O 封装 |
| `test_memory.c` | C | 内存函数单元测试 |
| `assert_stub.c` | C | 主机端断言桩实现 |

### 内存操作 API

```c
// 内存填充
void* memset(void* s, int c, size_t n);

// 内存拷贝（非重叠）
void* memcpy(void* dest, const void* src, size_t n);

// 内存拷贝（支持重叠）
void* memmove(void* dest, const void* src, size_t n);

// 内存比较
int memcmp(const void* s1, const void* s2, size_t n);
```

### 串口驱动 API

```c
// 初始化串口
bool serial_init(void);

// 发送字符串（阻塞）
void sync_serial_puts(const char* str);
```

### 端口 I/O API

```c
// 读端口
uint8_t inb(uint16_t port);

// 写端口
void outb(uint16_t port, uint8_t data);
```

---

## 构建和使用

### 配置和构建

```bash
# 配置构建
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 构建所有目标
cmake --build build

# 运行内存函数测试
cmake --build build --target test_memory
```

### QEMU 串口重定向

```bash
# 方法一：重定向到 stdio
qemu-system-x86_64 -drive format=raw,file=build/boot.img -nographic -serial mon:stdio

# 方法二：重定向到文件
qemu-system-x86_64 -drive format=raw,file=build/boot.img -serial file:serial.log

# 方法三：使用虚拟控制台
qemu-system-x86_64 -drive format=raw,file=build/boot.img -nographic -serial vc
```

### VGA 模式串口输出

```bash
# VGA 模式运行，串口重定向到 stdio
qemu-system-x86_64 -drive format=raw,file=build/boot.img \
    -vga std -display vnc=:0 \
    -nographic -serial mon:stdio
```

---

## 与前一个阶段的对比

| 特性 | stage/08 | stage/09 (本阶段) |
|-----|----------|------------------|
| 内存操作 | 无 | memset/memcpy/memmove/memcmp |
| 串口驱动 | 无 | UART 16550 驱动 |
| 端口 I/O | 无 | inb/outb 封装 |
| 调试输出 | 仅 VGA | VGA + 串口双输出 |
| Bootloader 串口 | 无 | 全模式串口支持 |
| 单元测试 | 无 | 主机端测试框架 |
| 启动信息 | VGA 显示 | VGA + 串口日志 |

---

## 内存布局

```
地址         内容                    来源
───────────────────────────────────────────────────────────
0x7C00       Bootloader Stage 1      bootloader.asm（实模式串口）
0x7E00       Bootloader Stage 2      bootloader.asm（保护模式串口）
0x10000      内核代码段
             ├─ kernel_entry.asm    （长模式串口初始化）
             ├─ kernel_main.c       （串口输出）
             ├─ kernel_init.c       （串口初始化）
             ├─ memory.c            （新增）
             ├─ serial.c            （新增）
             └─ io.c                （新增）
0x80000      栈顶 (向下增长)
0xB8000      VGA 文本缓冲区
0x3F8        COM1 串口端口           硬件固定
```

---

## 技术要点

### UART 16550 架构

```
┌─────────────────────────────────────────────────────────┐
│                   应用层                                │
│  (kernel_init.c, kernel_main.c - 调用串口输出)          │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│                  串口驱动层                             │
│  (serial.c - sync_serial_puts, serial_init)             │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│                  端口 I/O 层                            │
│  (io.c - inb, outb)                                     │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│                  硬件层                                 │
│  (UART 16550 @ 0x3F8)                                   │
└─────────────────────────────────────────────────────────┘
```

### 串口初始化流程

```
1. 禁用中断 (IER = 0x00)
       ↓
2. 启用 DLAB (LCR = 0x80)
       ↓
3. 设置波特率除数 (DLL=1, DLM=0 → 115200)
       ↓
4. 配置 8N1 格式 (LCR = 0x03)
       ↓
5. 启用 FIFO (FCR = 0xC7)
       ↓
6. 设置调制解调器控制 (MCR = 0x0B)
       ↓
7. 初始化完成
```

---

## 快速链接

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目总体进度
- [../08_string_utils/](../08_string_utils/) - 字符串工具文档
- [../10_base_with_format_log/](../10_base_with_format_log/) - 格式化日志文档

### 源代码
- [../../kernel/base/memory.c](../../kernel/base/memory.c) - 内存操作实现
- [../../kernel/driver/serial/](../../kernel/driver/serial/) - 串口驱动
- [../../kernel/driver/io/](../../kernel/driver/io/) - 端口 I/O
- [../../test/test_memory.c](../../test/test_memory.c) - 内存测试

---

## 文档维护

### 更新历史
- **2026-02-16**: 创建内存操作与串口驱动文档体系
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