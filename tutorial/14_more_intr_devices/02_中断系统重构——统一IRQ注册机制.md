# 02 - 中断系统重构——统一IRQ注册机制

在开始添加各种设备驱动之前，我们得先把地基打牢。说实话，stage 13 的中断系统虽然能用，但有个很明显的问题——注册 IRQ 处理器的方式太原始了。

---

## 旧方式的困境

在 stage 13，如果你要注册一个 IRQ 处理器，大概是这样写的：

```c
void my_irq_handler(interrupt_frame_t* frame) {
    // 处理中断...
    pic_send_eoi(frame->vector_number - 32);
}

void init_my_device(void) {
    idt_register_handler(33, my_irq_handler);  // 33 = IRQ 1
    pic_enable_irq(1);
}
```

看起来还行对吧？但问题是：

1. **没有名字**——你没法知道这个 IRQ 是谁注册的，调试时看到 "IRQ 1 fired" 完全不知道是哪个设备
2. **没有统计**——没法知道某个 IRQ 被触发了多少次
3. **没有上下文**——没法给处理器传递额外参数，只能用全局变量
4. **标志支持差**——有些设备需要自动发送 EOI，有些不需要，没法区分

如果你要支持多个设备共享一个 IRQ（比如 PCI 设备经常这么做），这种方式就完全不够用了。

而且最要命的是——处理器函数签名不统一。旧方式用 `interrupt_handler_fn(frame, error_code)`，但我们想要的是 `irq_handler_fn(frame, context)`。这导致每次都要写适配代码，真的很烦。

---

## 新设计方案

我们想要的是这样一个 API：

```c
// 描述符结构，包含处理器所有信息
typedef struct irq_descriptor {
    const char* name;              // 处理器名字
    irq_handler_fn handler;        // 处理器函数
    void* context;                 // 上下文指针
    irq_handler_flags_t flags;     // 标志
    uint64_t invocation_count;     // 调用计数
} irq_descriptor_t;

// 注册函数
int irq_register_handler(uint8_t irq, irq_descriptor_t* descriptor);

// 简单注册宏
#define IRQ_REGISTER_SIMPLE(irq, handler_fn, name_str) ...
```

这样我们就可以：
- 给每个 IRQ 处理器起名字，调试友好
- 统计每个 IRQ 被触发的次数
- 通过 context 传递任意参数
- 支持特殊标志（比如自动 EOI）

### 细节设计

#### IRQ 表结构

我们需要一个全局表来存储 IRQ 处理器信息：

```c
typedef struct irq_vector_entry {
    irq_descriptor_t* descriptor;  // 处理器描述符
    bool in_use;                  // 是否被占用
} irq_vector_entry_t;

// 16 条 IRQ 线
static irq_vector_entry_t irq_table[16] = {0};
```

每个 IRQ 可以有一个处理器（共享 IRQ 暂不支持，但结构已经为未来准备好了）。

#### 注册函数实现

```c
int irq_register_handler(uint8_t irq, irq_descriptor_t* descriptor) {
    // 参数检查
    if (irq >= 16) return -1;
    if (descriptor == NULL) return -2;
    if (descriptor->handler == NULL) return -3;

    irq_vector_entry_t* entry = &irq_table[irq];

    // 检查是否已被占用
    if (entry->in_use && entry->descriptor != NULL) {
        return -4;  // 已被注册
    }

    // 注册处理器
    entry->descriptor = descriptor;
    entry->in_use = true;

    klog_trace("Registered IRQ %d handler: %s\n", irq,
               descriptor->name ? descriptor->name : "unnamed");

    return 0;
}
```

#### 简单注册宏

每次都手写 `irq_descriptor_t` 结构体太啰嗦了，我们搞个宏：

```c
#define IRQ_REGISTER_SIMPLE(irq, handler_fn, name_str) \
    do { \
        static irq_descriptor_t __desc_##handler_fn = { \
            .name = (name_str), \
            .handler = (irq_handler_fn)(handler_fn), \
            .context = NULL, \
            .flags = IRQ_FLAG_NONE, \
            .invocation_count = 0 \
        }; \
        irq_register_handler((irq), &__desc_##handler_fn); \
    } while(0)
```

这样注册就变成了一行代码：

```c
IRQ_REGISTER_SIMPLE(1, keyboard_irq_handler, "PS/2 Keyboard");
```

---

## 中断分发逻辑修改

有了新的 IRQ 表，我们需要修改 `interrupt_handler()` 函数来使用它。

### 旧逻辑

旧逻辑是这样的（简化）：

```c
void interrupt_handler(uint64_t vector, uint64_t error_code, interrupt_frame_t* frame) {
    if (vector >= 32 && vector < 48) {
        uint8_t irq = vector - 32;
        if (custom_handlers[vector] != NULL) {
            custom_handlers[vector](frame, error_code);
        }
        pic_send_eoi(irq);
    }
}
```

问题是要手动管理 `custom_handlers[]` 数组，而且调用后忘记发 EOI 会出问题。

### 新逻辑

新逻辑检查 `irq_table[]`：

```c
void interrupt_handler(uint64_t vector, uint64_t error_code, interrupt_frame_t* frame) {
    if (vector < 32) {
        // CPU 异常...
    } else if (vector >= 32 && vector < 48) {
        uint8_t irq = vector - 32;
        irq_vector_entry_t* entry = &irq_table[irq];

        if (entry->in_use && entry->descriptor != NULL) {
            // 调用新风格的处理器
            irq_descriptor_t* desc = entry->descriptor;
            desc->invocation_count++;  // 统计
            desc->handler(frame, desc->context);

            // 检查是否需要自动 EOI
            if (!(desc->flags & IRQ_FLAG_AUTOEOI)) {
                pic_send_eoi(irq);
            }
        } else if (custom_handlers[vector] != NULL) {
            // 兼容旧风格
            custom_handlers[vector](frame, error_code);
            pic_send_eoi(irq);
        } else {
            // 没有注册处理器
            klog_warn("IRQ %d occurred but no handler registered\n", irq);
            pic_send_eoi(irq);
        }
    }
}
```

这样就有了：
- 调用计数统计（`invocation_count`）
- 自动 EOI 支持（`IRQ_FLAG_AUTOEOI`）
- 兼容旧代码（`custom_handlers` 回退）
- 更好的错误提示

---

## 动手修改代码

好，理论讲完了，让我们来实际修改代码。

### 第一步：修改 idt.h

首先，我们在 `kernel/interrupt/idt.h` 中添加新的定义：

```c
// 打开 kernel/interrupt/idt.h

// 在文件末尾、#endif 之前添加：

/* ============================================================================
 * IRQ Handler Registration Types (New)
 * ============================================================================ */

/**
 * @brief IRQ handler flags
 */
typedef enum irq_handler_flags {
    IRQ_FLAG_NONE = 0,
    IRQ_FLAG_AUTOEOI = (1 << 0),  // Handler sends EOI automatically
} irq_handler_flags_t;

/**
 * @brief New IRQ handler function type with context support
 */
typedef void (*irq_handler_fn)(interrupt_frame_t* frame, void* context);

/**
 * @brief IRQ descriptor - describes an IRQ handler
 */
typedef struct irq_descriptor {
    const char* name;              // Handler name
    irq_handler_fn handler;        // Handler function
    void* context;                 // Context pointer
    irq_handler_flags_t flags;     // Handler flags
    uint64_t invocation_count;     // Statistics
} irq_descriptor_t;

/**
 * @brief Register an IRQ handler with descriptor
 */
int irq_register_handler(uint8_t irq, irq_descriptor_t* descriptor);

/**
 * @brief Unregister an IRQ handler
 */
int irq_unregister_handler(uint8_t irq, irq_descriptor_t* descriptor);

/**
 * @brief Simple IRQ registration macro
 */
#define IRQ_REGISTER_SIMPLE(irq, handler_fn, name_str) \
    do { \
        static irq_descriptor_t __desc_##handler_fn = { \
            .name = (name_str), \
            .handler = (irq_handler_fn)(handler_fn), \
            .context = NULL, \
            .flags = IRQ_FLAG_NONE, \
            .invocation_count = 0 \
        }; \
        irq_register_handler((irq), &__desc_##handler_fn); \
    } while(0)
```

### 第二步：修改 idt.c

然后修改 `kernel/interrupt/idt.c`，添加实现代码：

```c
// 打开 kernel/interrupt/idt.c

// 在文件顶部，idt_entry_t 定义之后添加：

/* ============================================================================
 * New IRQ Handler Table
 * ============================================================================ */

/**
 * @brief IRQ vector table entry
 */
typedef struct irq_vector_entry {
    irq_descriptor_t* descriptor;
    bool in_use;
} irq_vector_entry_t;

// IRQ handler table (16 IRQ lines)
static irq_vector_entry_t irq_table[16] = {0};
```

然后添加注册函数实现：

```c
// 在 idt_register_handler() 函数之后添加：

/**
 * @brief Register an IRQ handler with descriptor
 */
int irq_register_handler(uint8_t irq, irq_descriptor_t* descriptor) {
    if (irq >= 16) {
        klog_error("Invalid IRQ number: %d (must be 0-15)\n", irq);
        return -1;
    }

    if (descriptor == NULL) {
        klog_error("NULL descriptor for IRQ %d\n", irq);
        return -2;
    }

    if (descriptor->handler == NULL) {
        klog_error("NULL handler in descriptor for IRQ %d\n", irq);
        return -3;
    }

    irq_vector_entry_t* entry = &irq_table[irq];

    // Check if IRQ already has a handler
    if (entry->in_use && entry->descriptor != NULL) {
        klog_error("IRQ %d already has a handler registered (%s)\n",
                   irq, entry->descriptor->name);
        return -4;
    }

    // Register the handler
    entry->descriptor = descriptor;
    entry->in_use = true;

    klog_trace("Registered IRQ %d handler: %s\n", irq,
               descriptor->name ? descriptor->name : "unnamed");

    return 0;
}

/**
 * @brief Unregister an IRQ handler
 */
int irq_unregister_handler(uint8_t irq, irq_descriptor_t* descriptor) {
    if (irq >= 16) {
        return -1;
    }

    irq_vector_entry_t* entry = &irq_table[irq];

    if (entry->descriptor != descriptor) {
        klog_error("Descriptor mismatch for IRQ %d\n", irq);
        return -2;
    }

    // Clear the entry
    entry->descriptor = NULL;
    entry->in_use = false;

    klog_trace("Unregistered IRQ %d handler: %s\n", irq,
               descriptor->name ? descriptor->name : "unnamed");

    return 0;
}
```

最后修改 `interrupt_handler()` 函数：

```c
// 找到 void interrupt_handler(uint64_t vector, uint64_t error_code, interrupt_frame_t* frame)
// 替换整个函数实现：

void interrupt_handler(uint64_t vector, uint64_t error_code, interrupt_frame_t* frame) {
    (void)error_code;
    extern void pic_send_eoi(uint8_t irq);

    if (vector < 32) {
        // CPU exceptions - use default exception handler
        if (custom_handlers[vector] != NULL) {
            custom_handlers[vector](frame, error_code);
        } else {
            default_exception_handler(frame, vector, error_code);
        }
    } else if (vector >= 32 && vector < 48) {
        // IRQ interrupt
        uint8_t irq = vector - 32;
        irq_vector_entry_t* entry = &irq_table[irq];

        if (entry->in_use && entry->descriptor != NULL) {
            // Call new-style IRQ handler
            irq_descriptor_t* desc = entry->descriptor;
            desc->invocation_count++;
            desc->handler(frame, desc->context);

            // Check if handler sends EOI automatically
            if (!(desc->flags & IRQ_FLAG_AUTOEOI)) {
                pic_send_eoi(irq);
            }
        } else if (custom_handlers[vector] != NULL) {
            // Fallback to old-style handler for compatibility
            custom_handlers[vector](frame, error_code);
            pic_send_eoi(irq);
        } else {
            klog_warn("Meeting One IRQ: %d occurred but no handler registered\n", irq);
            pic_send_eoi(irq);
        }
    } else {
        // Spurious interrupt or unexpected interrupt
        klog_warn("Meeting Spurious interrupt: vector %d\n", vector);
        pic_send_eoi(7); // Send EOI for IRQ 7 (common spurious interrupt)
    }
}
```

### 第三步：更新 CMakeLists.txt

确保新的 idt.c 被编译。通常这已经在 CMakeLists.txt 中了，但检查一下总是好的：

```cmake
# kernel/interrupt/CMakeLists.txt
# 确保包含 idt.c
target_sources(kernel PRIVATE
    idt.c
    interrupt.c
    interrupt.asm
    # ...
)
```

---

## 测试新 API

现在我们来测试一下新的 API 是否正常工作。写个简单的测试：

### 创建测试文件

在 `kernel/interrupt/` 目录下创建一个测试函数（或者放在某个 demo 文件里）：

```c
// 测试 IRQ 注册
void test_irq_registration(void) {
    // 定义一个测试处理器
    static void test_irq_handler(interrupt_frame_t* frame, void* context) {
        (void)frame;
        (void)context;
        klog_info("Test IRQ handler called!\n");
    }

    // 使用简单宏注册
    IRQ_REGISTER_SIMPLE(0, test_irq_handler, "Test Timer");

    // 或者使用完整 API
    static irq_descriptor_t test_desc = {
        .name = "Manual Test",
        .handler = test_irq_handler,
        .context = NULL,
        .flags = IRQ_FLAG_NONE,
        .invocation_count = 0
    };
    irq_register_handler(1, &test_desc);
}
```

### 编译测试

```bash
cd /path/to/CCOperatingSystemX64
cmake --build build
```

如果编译通过，说明语法没问题。如果报错，检查：
- 类型定义是否正确
- 函数声明是否在头文件中
- 宏定义是否正确

---

## 常见问题

### 问题 1：编译报错 "undefined reference to `irq_register_handler`"

**原因**：链接器找不到函数实现。

**解决方案**：确保 `idt.c` 被编译并链接。检查 CMakeLists.txt 中是否包含了这个文件。

### 问题 2：中断不触发

**原因**：可能是忘记启用 IRQ 线。

**解决方案**：注册处理器后，记得调用 `pic_enable_irq(irq)` 来启用这条 IRQ 线。

### 问题 3：系统重启或卡死

**原因**：处理器函数有问题，比如忘记发送 EOI（如果没设置 `IRQ_FLAG_AUTOEOI`）。

**解决方案**：检查处理器函数是否正确处理了 EOI。如果是新风格处理器，确保没有设置 `IRQ_FLAG_AUTOEOI` 时手动发送 EOI。

---

## 验证成功

如何确认新 API 工作正常？看看日志输出：

```bash
qemu-system-x86_64 -drive format=raw,file=build/boot.img -serial stdio
```

你应该看到类似的输出：

```
[TRACE] Registered IRQ 0 handler: Test Timer
[TRACE] Registered IRQ 1 handler: Manual Test
[TRACE] All IRQs (0-15) enabled
[INFO] Interrupts enabled
```

如果 IRQ 0 触发，你会看到：

```
[INFO] Test IRQ handler called!
```

---

## 与旧代码的兼容

如果你有旧代码使用 `idt_register_handler()`，不用担心，我们还保留着：

```c
// 旧代码仍然有效
void old_handler(interrupt_frame_t* frame, uint64_t error_code) {
    // ...
}

idt_register_handler(32, old_handler);  // 仍然有效
```

`interrupt_handler()` 函数会先检查新的 `irq_table[]`，如果没有才回退到 `custom_handlers[]`。这样新旧代码可以共存，方便逐步迁移。

---

## 接下来

现在我们有了一个统一、灵活的 IRQ 注册机制。接下来我们会用这个机制来实现各种设备驱动。

下一节，我们先实现一个基础数据结构——环形缓冲区。键盘和串口都要用它，所以先把基础打好。

→ [下一篇：数据结构基础——环形缓冲区从零实现](./03_数据结构基础——环形缓冲区从零实现.md)

---

<div align="center">

## 文档导航

[← 为什么需要更多中断设备驱动](./01_为什么需要更多中断设备驱动.md) | [环形缓冲区 →](./03_数据结构基础——环形缓冲区从零实现.md)

</div>
