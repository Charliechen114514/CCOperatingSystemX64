# CCOS 中断基础 文档中心

本目录包含 CCOS Stage 13 - 中断基础开发的完整文档体系。

---

## 阶段概述

**Stage 13: 中断基础**

本阶段实现了 CCOS 的中断处理基础设施，为操作系统内核提供完整的异常和硬件中断处理能力。

### 核心成果

- **8259 PIC 驱动** ([`kernel/driver/pic/pic.h`](../../kernel/driver/pic/pic.h))
  - 双片级联 8259A PIC 控制器驱动
  - IRQ 重映射到向量 32-47
  - 精细的 IRQ 屏蔽控制
  - EOI（中断结束）机制

- **中断描述符表（IDT）** ([`kernel/interrupt/idt.h`](../../kernel/interrupt/idt.h))
  - 256 个 IDT 条目管理
  - 32 个 CPU 异常处理（向量 0-31）
  - 16 个硬件 IRQ 处理（向量 32-47）
  - 可注册的自定义中断处理程序

- **中断/异常处理 Stub** ([`kernel/interrupt/interrupt.asm`](../../kernel/interrupt/interrupt.asm))
  - 32 个 CPU 异常 ISR stub
  - 16 个 IRQ stub
  - **x86-64 ABI 16 字节栈对齐修复**
  - 完整的 CPU 状态保存/恢复

- **定时器中断支持** ([`kernel/interrupt/interrupt.c`](../../kernel/interrupt/interrupt.c))
  - IRQ0（定时器）处理示例
  - 定时器滴答计数器
  - 演示中断处理程序注册机制

- **QEMU Monitor 调试** ([`scripts/build_helpers/build_release_qemu_monitor_run.sh`](../../scripts/build_helpers/build_release_qemu_monitor_run.sh))
  - telnet 端口 4444 连接
  - 实时查看 PIC 状态和寄存器

---

## 目录结构

```
kernel/
├── driver/
│   └── pic/
│       ├── pic.h              # PIC 驱动接口
│       ├── pic.c              # PIC 实现
│       └── pic_constants.h    # PIC 常量定义
└── interrupt/
    ├── idt.h                  # IDT 接口
    ├── idt.c                  # IDT 实现
    ├── idt_constants.h        # IDT 常量定义
    ├── interrupt.h            # 中断子系统接口
    ├── interrupt.c            # 中断实现
    ├── interrupt.asm          # 汇编 stub
    └── CMakeLists.txt         # 构建配置
```

---

## 文档导航

### 1. [开发笔记](./开发笔记.md) 设计思路

**内容**:
- 为什么需要中断系统
- 设计决策（8259 PIC 选择、IRQ 重映射、栈对齐方案）
- 架构设计与模块协作
- 栈对齐问题的发现与修复
- 常见陷阱与注意事项
- 未来改进方向（APIC、MSI 等）

**适合**:
- 理解设计思路
- 学习系统架构
- 查找开发经验

---

### 2. [技术参考](./技术参考.md) 技术手册

**内容**:
- x86 中断基础（中断与异常区别、IDT 结构）
- 8259 PIC 技术参考（ICW/OCW 命令、I/O 端口）
- 中断子系统完整 API 参考
- 数据结构定义（interrupt_frame_t、idt_entry_t）
- 常量定义（异常向量、IRQ 向量、门类型属性）

**适合**:
- 查询 API 用法
- 理解底层实现
- 深入技术细节

---

### 3. [故障排查指南](./故障排查指南.md) 问题诊断

**内容**:
- 中断未触发问题
- 异常处理崩溃问题
- 系统挂起问题
- QEMU 调试问题
- 符号表问题

每个问题按"症状-原因-解决方案"格式组织。

**适合**:
- 遇到问题时查阅
- 学习问题排查思路
- 理解错误原因

---

### 4. [调试工具指南](./调试工具指南.md) 工具使用

**内容**:
- QEMU Monitor 调试技巧
- GDB 中断调试方法
- 串口日志分析
- 符号表调试

**适合**:
- 学习调试工具
- 查询具体命令
- 提升调试效率

---

## 快速开始

### 查看代码结构

1. **PIC 驱动接口** → 查看 [`kernel/driver/pic/pic.h`](../../kernel/driver/pic/pic.h)
2. **IDT 接口** → 查看 [`kernel/interrupt/idt.h`](../../kernel/interrupt/idt.h)
3. **中断子系统接口** → 查看 [`kernel/interrupt/interrupt.h`](../../kernel/interrupt/interrupt.h)
4. **汇编 Stub** → 查看 [`kernel/interrupt/interrupt.asm`](../../kernel/interrupt/interrupt.asm)

### 使用示例

```c
#include "interrupt/interrupt.h"

// 内核初始化时调用
void kernel_init(void) {
    // 初始化整个中断子系统
    interrupt_init();

    // 此时定时器中断已启用，定时器滴答计数器开始工作
}

// 在其他地方使用
void some_function(void) {
    // 禁用中断
    interrupt_disable();

    // 临界区代码...

    // 重新启用中断
    interrupt_enable();

    // 获取定时器滴答数
    uint64_t ticks = timer_get_ticks();
}
```

### 中断处理程序注册示例

```c
#include "interrupt/idt.h"

// 自定义中断处理函数
void my_irq_handler(interrupt_frame_t* frame) {
    klog_info("My IRQ handler called! vector=%d\n", frame->vector_number);

    // 处理中断...

    // 发送 EOI
    pic_send_eoi(frame->vector_number - 32);
}

// 注册处理程序（通常在初始化时）
void init_my_device(void) {
    // 注册 IRQ 1 (键盘)
    idt_register_handler(33, my_irq_handler);

    // 启用 IRQ 1
    pic_enable_irq(1);
}
```

---

## 与前一阶段对比

| 特性 | Stage 12 (栈回溯) | Stage 13 (中断基础) |
|------|-------------------|---------------------|
| 调试能力 | 栈回溯、符号解析 | + 中断异常捕获 |
| 硬件交互 | 无 | PIC 驱动 |
| 系统响应 | 轮询模式 | 中断驱动 |
| 定时能力 | 无 | 定时器滴答 |
| 新增文件 | 6 个 | 13 个 |
| 符号数量 | 96 | 156 |

---

## 技术亮点

### 1. x86-64 ABI 栈对齐修复

发现并修复了中断处理中的关键栈对齐问题：

```asm
; 在每个 ISR stub 中
push rsp                    ; 保存原始 RSP
and rsp, ~0xF               ; 16 字节对齐
push qword 0                ; 虚拟对齐 dummy
push qword vector_number    ; 中断向量号
push qword 0                ; 虚拟错误码（如无错误码）
jmp interrupt_common        ; 通用处理入口
```

### 2. 模块化设计

```
┌─────────────────────────────────────────┐
│         中断子系统 (interrupt)          │
│  - interrupt_init()                     │
│  - timer_handler()                      │
└──────────────┬──────────────────────────┘
               │
     ┌─────────┴─────────┐
     │                   │
┌────▼────┐         ┌────▼────┐
│   PIC   │         │   IDT   │
│ 驱动模块 │         │ 管理模块 │
└─────────┘         └─────────┘
```

### 3. 完整的 CPU 状态保存

```c
typedef struct PACKED {
    uint64_t error_code;      // 错误码
    uint64_t rip;             // 指令指针
    uint64_t cs;              // 代码段
    uint64_t rflags;          // RFLAGS 寄存器
    uint64_t rsp;             // 栈指针
    uint64_t ss;              // 栈段
} interrupt_frame_t;
```

### 4. IRQ 重映射

将 IRQ 0-15 从默认向量 0-15 重映射到 32-47，避免与 CPU 异常冲突：

```
默认映射（冲突）:
  IRQ 0-15  →  向量 0-15  (与 CPU 异常重叠!)

重映射后:
  IRQ 0-15  →  向量 32-47 (避免冲突)
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

## 版本信息

- **阶段**: Stage 13
- **分支**: `develop`
- **提交**: `62fb0cf` - intr first codes OK
- **日期**: 2026-02-17
- **作者**: Charliechen114514

---

## 相关资源

### 项目文档
- [../PROGRESS.md](../PROGRESS.md) - 项目进度
- [../12_stacktrace_supports/README.md](../12_stacktrace_supports/) - 上一阶段文档

### 源码文件
- [`kernel/driver/pic/pic.h`](../../kernel/driver/pic/pic.h)
- [`kernel/interrupt/idt.h`](../../kernel/interrupt/idt.h)
- [`kernel/interrupt/interrupt.h`](../../kernel/interrupt/interrupt.h)
- [`kernel/interrupt/interrupt.asm`](../../kernel/interrupt/interrupt.asm)
- [`INTERRUPT_STACK_ALIGNMENT_FIX.md`](../../INTERRUPT_STACK_ALIGNMENT_FIX.md)

### 外部参考
- [8259A PIC Datasheet](https://pdos.csail.mit.edu/6.828/2016/readtures/hardware/8259A.pdf)
- [x86_64 System V ABI](https://gitlab.com/x86-psABIs/x86-64-ABI)
- [Intel SDM Vol. 3A - Chapter 6: Interrupt and Exception Handling](https://software.intel.com/content/www/us/en/develop/download/intel-64-and-ia-32-architectures-sdm-combined-volumes-1-2a-2b-2c-2d-3a-3b-3c-3d-and-4.html)
- [OSDev.org - Interrupts](https://wiki.osdev.org/Interrupts)
- [OSDev.org - 8259_PIC](https://wiki.osdev.org/8259_PIC)
- [OSDev.org - IDT](https://wiki.osdev.org/Interrupt_Descriptor_Table)

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-17
