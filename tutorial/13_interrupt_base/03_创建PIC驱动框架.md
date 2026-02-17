# 创建 PIC 驱动框架 —— Stage 13 中断基础实战指南

## 前言

上一篇文章我们讲了 x86 中断机制的理论知识，现在到了真正动手的时候了。说实话，写内核代码最怕的就是纸上谈兵，看得再多不如自己动手写一遍。

今天我们开始实现中断系统的第一个模块：8259 PIC 驱动。我们先搭建好框架，定义好接口和常量，为后续的实现做好准备。这部分的代码虽然不多，但每一行都有它的道理，我们一步一步来。

---

## 第一步：创建目录结构

现在我们来创建一个干净的 PIC 驱动目录。先把目录结构规划好，避免以后代码多了之后乱成一团。

```bash
# 在项目根目录下
cd /home/charliechen/CCOperatingSystemX64

# 创建 PIC 驱动目录
mkdir -p kernel/driver/pic

# 查看目录结构
tree kernel/driver/pic -L 1
```

你应该看到一个空目录。很好，现在我们开始往里面填东西。

说实话，目录结构这点看似简单，但我见过太多项目因为一开始没规划好，后面重构起来痛苦不堪。我们把 PIC 驱动相关的代码放在 `kernel/driver/pic/` 目录下，与后续其他驱动（键盘、串口等）平级，这样结构清晰，以后维护也方便。

---

## 第二步：定义 I/O 端口常量

8259 PIC 通过 I/O 端口与 CPU 通信。每个 PIC 芯片有两个端口：命令端口和数据端口。我们先把这些端口地址定义出来。

创建 `kernel/driver/pic/pic_constants.h`：

```c
/**
 * @file pic_constants.h
 * @brief 8259 PIC 常量定义
 * @date 2026-02-17
 */

#pragma once

/* ============================================================================
 * PIC I/O Ports
 * ============================================================================ */

// Master PIC ports
#define PIC1_CMD 0x20  // Master PIC command port
#define PIC1_DATA 0x21 // Master PIC data port

// Slave PIC ports
#define PIC2_CMD 0xA0  // Slave PIC command port
#define PIC2_DATA 0xA1 // Slave PIC data port

/* ============================================================================
 * PIC Commands
 * ============================================================================ */

// ICW1 (Initialization Command Word 1) bits
#define PIC_ICW1_INIT     0x01  // Initialization bit
#define PIC_ICW1_ICW4     0x01  // ICW4 needed
#define PIC_INIT         0x11  // INIT + ICW4 needed

// ICW4 (Initialization Command Word 4) bits
#define PIC_ICW4_8086     0x01  // 8086/88 (MCS-80/85) mode

// OCW2 (Operation Command Word 2) - EOI command
#define PIC_EOI           0x20  // Non-specific EOI
```

这些常量的含义需要解释一下：

**I/O 端口地址**是 x86 架构规定的，8259 PIC 硬件就是连接在这些端口上：
- Master PIC 的命令端口是 `0x20`，数据端口是 `0x21`
- Slave PIC 的命令端口是 `0xA0`，数据端口是 `0xA1`

**ICW1_INIT** 和 **ICW1_ICW4** 是初始化命令字的标志位，我们后面会详细讲解。

**PIC_EOI** 是"中断结束"命令，每次处理完中断后必须发送这个命令。

---

## 第三步：理解 PIC 命令字

8259 PIC 有两种主要的命令：初始化命令字（ICW）和操作命令字（OCW）。我们需要理解它们的作用，才能正确配置 PIC。

### ICW1 - 初始化开始

向 PIC 的命令端口发送 ICW1 来开始初始化：

```
Bit 7-5: 0 (x86 模式)
Bit 4:   1 (需要 ICW4)
Bit 3:   0 (边缘触发模式)
Bit 1:   1 (需要 ICW3 - 级联模式)
Bit 0:   1 (初始化)

ICW1 = 0x11 (PIC_INIT)
```

### ICW2 - 向量偏移

告诉 PIC IRQ 0 对应的中断向量号：

```
Master: IRQ 基准向量 (通常 32 = 0x20)
Slave:  IRQ 基准向量 (通常 40 = 0x28)
```

### ICW3 - 级联配置

```
Master: 0x04 (IR2 连接从 PIC)
Slave:  0x02 (级联标识)
```

### ICW4 - 模式配置

```
Bit 0: 1 (8086 模式)

ICW4 = 0x01 (PIC_ICW4_8086)
```

### OCW2 - EOI 命令

```
Bit 7-5: 001 (非特定 EOI)
Bit 4:   0
Bit 3:   0
Bit 2-0: 0

OCW2 = 0x20 (PIC_EOI)
```

---

## 第四步：定义公共接口

现在我们来定义 PIC 驱动的公共接口。这个头文件会被其他模块引用，所以设计要清晰合理。

创建 `kernel/driver/pic/pic.h`：

```c
/**
 * @file pic.h
 * @brief 8259 Programmable Interrupt Controller (PIC) driver
 *
 * The x86 architecture uses two 8259A PIC chips cascaded together:
 * - Master PIC: handles IRQs 0-7
 * - Slave PIC: handles IRQs 8-15
 *
 * The PIC must be remapped because the first 32 IRQ vectors (0-31) are
 * reserved for CPU exceptions. We remap IRQs 0-15 to vectors 32-47.
 */

#pragma once

#include "defines/types.h"

/* ============================================================================
 * PIC Initialization
 * ============================================================================ */

/**
 * @brief Initialize and remap the PIC
 *
 * Remaps IRQs 0-15 to IDT vectors 32-47 to avoid conflict with
 * CPU exceptions (vectors 0-31).
 *
 * @param offset1 Base vector for master PIC IRQs (typically 32)
 * @param offset2 Base vector for slave PIC IRQs (typically 40)
 */
void pic_init(uint8_t offset1, uint8_t offset2);

/**
 * @brief Send End of Interrupt (EOI) to the PIC
 *
 * This must be called at the end of an IRQ handler to acknowledge
 * the interrupt and allow further interrupts.
 *
 * @param irq The IRQ number (0-15)
 */
void pic_send_eoi(uint8_t irq);

/* ============================================================================
 * IRQ Masking
 * ============================================================================ */

/**
 * @brief Disable an IRQ line (mask it)
 *
 * @param irq The IRQ number (0-15) to disable
 */
void pic_disable_irq(uint8_t irq);

/**
 * @brief Enable an IRQ line (unmask it)
 *
 * @param irq The IRQ number (0-15) to enable
 */
void pic_enable_irq(uint8_t irq);

/**
 * @brief Get the current IRQ mask
 *
 * @param irq The IRQ number (0-15)
 * @return true if IRQ is masked (disabled), false if enabled
 */
bool pic_is_irq_masked(uint8_t irq);

/**
 * @brief Disable all IRQs (mask all interrupt lines)
 */
void pic_disable_all(void);

/**
 * @brief Enable all IRQs (unmask all interrupt lines)
 */
void pic_enable_all(void);
```

这个头文件定义了 PIC 驱动的完整公共接口。让我解释一下每个函数的作用：

**pic_init()**：初始化 PIC 并重映射 IRQ。这是我们首先需要调用的函数。

**pic_send_eoi()**：发送中断结束信号。这个非常重要！如果忘记发送 EOI，后续的中断就不会触发了。

**pic_disable_irq() / pic_enable_irq()**：控制单个 IRQ 线的启用和禁用。

**pic_is_irq_masked()**：查询某个 IRQ 当前是否被屏蔽。

**pic_disable_all() / pic_enable_all()**：控制所有 IRQ 线。

---

## 第五步：配置 CMake 构建

现在我们需要把这个模块加入到构建系统中。创建 `kernel/driver/CMakeLists.txt`（如果不存在）或修改现有的：

```cmake
# ============================================================================
# PIC 驱动模块
# ============================================================================

add_subdirectory(pic)

# 确保 pic 库会被链接到内核
# 这部分在 kernel/CMakeLists.txt 中处理
```

然后创建 `kernel/driver/pic/CMakeLists.txt`：

```cmake
# ============================================================================
# 8259 PIC Driver
# ============================================================================

add_library(pic STATIC
    pic.c
)

target_include_directories(pic PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_compile_options(pic PUBLIC ${CCOS_FREESTANDING_FLAGS})

set_target_properties(pic PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
```

现在我们还需要修改 `kernel/CMakeLists.txt`，确保 pic 库被链接：

```cmake
# 添加 pic 子目录
add_subdirectory(driver)

# 在链接依赖中添加 pic
target_link_libraries(kernel
    # ... 其他库 ...
    pic
)
```

---

## 第六步：创建占位实现

为了让编译能够通过，我们先创建一个空的 `pic.c` 文件，只包含占位函数。

创建 `kernel/driver/pic/pic.c`：

```c
/**
 * @file pic.c
 * @brief 8259 PIC driver implementation
 * @date 2026-02-17
 */

#include "pic.h"
#include "pic_constants.h"

// 占位实现，下一篇文章我们会填充完整逻辑

void pic_init(uint8_t offset1, uint8_t offset2) {
    (void)offset1;
    (void)offset2;
    // TODO: 实现 PIC 初始化
}

void pic_send_eoi(uint8_t irq) {
    (void)irq;
    // TODO: 实现 EOI 发送
}

void pic_disable_irq(uint8_t irq) {
    (void)irq;
    // TODO: 实现 IRQ 禁用
}

void pic_enable_irq(uint8_t irq) {
    (void)irq;
    // TODO: 实现 IRQ 启用
}

bool pic_is_irq_masked(uint8_t irq) {
    (void)irq;
    // TODO: 实现屏蔽状态查询
    return false;
}

void pic_disable_all(void) {
    // TODO: 实现全部禁用
}

void pic_enable_all(void) {
    // TODO: 实现全部启用
}
```

`(void)variable` 这种写法是为了告诉编译器"我知道这个参数没被使用，别报警告"。这只是临时的占位实现，下一篇文章我们会填充完整逻辑。

---

## 第七步：验证编译

现在我们来验证一下框架是否搭建正确，能否编译通过。

```bash
cd /home/charliechen/CCOperatingSystemX64/build
cmake ..
make
```

如果一切正常，你应该看到类似这样的输出：

```
[ 10%] Building C object kernel/driver/pic/CMakeFiles/pic.dir/pic.c.o
[ 20%] Linking C static library libpic.a
[ 30%] Built target pic
...
[100%] Built target kernel
```

如果你看到编译错误，检查以下几点：
1. `pic_constants.h` 是否被 `pic.c` 正确包含
2. CMake 配置是否正确
3. 文件路径是否正确

---

## 到这里我们完成了什么

这篇文章我们搭建了 PIC 驱动的框架：

- 创建了 `kernel/driver/pic/` 目录结构
- 定义了 I/O 端口和命令字常量
- 定义了公共接口
- 配置了 CMake 构建系统
- 创建了占位实现

虽然还没有实现具体逻辑，但框架已经搭好了。下一篇文章我们会实现 PIC 的初始化序列，这是配置 8259 PIC 的核心代码。

---

## 接下来

在下一篇文章中，我们会：
1. 实现 `pic_init()` 函数
2. 详细讲解 ICW1-ICW4 初始化序列
3. 理解为什么需要 IRQ 重映射
4. 实现完整的初始化流程

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← x86中断机制详解](02_x86中断机制详解.md)  | [PIC初始化序列实现 →](04_PIC初始化序列实现.md)

</div>
