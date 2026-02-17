# 05 - 键盘驱动实现——扫描码转ASCII

上一节我们了解了 PS/2 协议和扫描码，现在让我们来实现扫描码到 ASCII 的转换。这是键盘驱动的核心逻辑，也是最容易出问题的地方。

---

## 从扫描码到 ASCII

扫描码是按键的物理标识，而 ASCII 是字符的编码。这两者不是一一对应的——同一个键在不同修饰状态下会产生不同的字符。

### 基本转换

最简单的情况：没有修饰键，直接查表。

```c
// 扫描码到 ASCII 查找表（US QWERTY, Set 1）
static const char scancode_to_ascii[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z',
    'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0,
    // ... 省略部分
};
```

使用时直接索引：

```c
char c = scancode_to_ascii[scancode];
```

但这里有个问题：扫描码 0x81-0xFF（Break Code）怎么办？我们会在调用前检查。

---

## 修饰键处理

### Shift 键

Shift 键（左右）的作用是切换到"上档"字符——数字变成符号，字母变大写。

我们需要两个查找表：普通和 Shift 状态。

```c
// 普通字符
static const char ascii_normal[128] = {
    // ... '1', '2', '3', ..., 'q', 'w', 'e', ...
};

// Shift 字符
static const char ascii_shifted[128] = {
    // ... '!', '@', '#', ..., 'Q', 'W', 'E', ...
};
```

然后根据 Shift 状态选择：

```c
static volatile bool shift_pressed = false;

char scancode_to_ascii(uint8_t scancode) {
    uint8_t code = scancode & 0x7F;  // 去掉 break 位
    bool is_break = (scancode & 0x80) != 0;

    // 更新 Shift 状态
    if (code == 0x2A || code == 0x36) {  // LShift 或 RShift
        shift_pressed = !is_break;
        return 0;  // 修饰键不产生字符
    }

    // 只处理 Make Code
    if (is_break) {
        return 0;
    }

    // 根据状态选择表
    if (shift_pressed) {
        return ascii_shifted[code];
    } else {
        return ascii_normal[code];
    }
}
```

### Caps Lock

Caps Lock 只影响字母键，而且它的效果是"切换"的——按一次开启，再按一次关闭。

```c
static volatile bool caps_lock = false;

char scancode_to_ascii(uint8_t scancode) {
    uint8_t code = scancode & 0x7F;
    bool is_break = (scancode & 0x80) != 0;

    // 处理 Shift
    if (code == 0x2A || code == 0x36) {
        shift_pressed = !is_break;
        return 0;
    }

    // 处理 Caps Lock
    if (code == 0x3A) {  // Caps Lock
        if (!is_break) {  // 只在 Make 时切换
            caps_lock = !caps_lock;
        }
        return 0;
    }

    if (is_break) {
        return 0;
    }

    // 选择字符
    bool use_shift = shift_pressed;

    // Caps Lock 只影响字母
    if (caps_lock && is_letter(code)) {
        use_shift = !use_shift;  // 反转 Shift 状态
    }

    if (use_shift) {
        return ascii_shifted[code];
    } else {
        return ascii_normal[code];
    }
}
```

### 判断是否为字母

我们需要一个函数来判断某个扫描码是否是字母：

```c
static inline bool is_letter(uint8_t scancode) {
    // QWERTY 行: 0x10-0x19 (Q-P)
    // ASDF 行: 0x1E-0x26 (A-L)
    // ZXCV 行: 0x2C-0x32 (Z-M)
    return (scancode >= 0x10 && scancode <= 0x19) ||
           (scancode >= 0x1E && scancode <= 0x26) ||
           (scancode >= 0x2C && scancode <= 0x32);
}
```

---

## 完整查找表

让我们把完整的查找表写出来。这部分代码很冗长但很必要。

### 普通字符表

```c
static const char scancode_to_ascii_table[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',      // 0x00-0x0F
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,   // 0x10-0x1F
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z',   // 0x20-0x2F
    'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0,     // 0x30-0x3F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                               // 0x40-0x4F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                               // 0x50-0x5F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                               // 0x60-0x6F
    0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0                             // 0x70-0x7F
};
```

### Shift 字符表

```c
static const char scancode_to_ascii_shifted[128] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',         // 0x00-0x0F
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,    // 0x10-0x1F
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z',       // 0x20-0x2F
    'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0,      // 0x30-0x3F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                // 0x40-0x4F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                // 0x50-0x5F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                // 0x60-0x6F
    0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0                             // 0x70-0x7F
};
```

---

## 完整转换函数

整合起来：

```c
static volatile bool shift_pressed = false;
static volatile bool caps_lock = false;

static inline bool is_letter(uint8_t scancode) {
    return (scancode >= 0x10 && scancode <= 0x19) ||
           (scancode >= 0x1E && scancode <= 0x26) ||
           (scancode >= 0x2C && scancode <= 0x32);
}

static char scancode_to_ascii(uint8_t scancode) {
    // 检查 Break Code
    if (scancode & 0x80) {
        return 0;
    }

    if (scancode >= 128) {
        return 0;
    }

    // 应用 Shift 和 Caps Lock
    bool use_shift = shift_pressed;

    if (caps_lock && is_letter(scancode)) {
        use_shift = !use_shift;
    }

    if (use_shift) {
        return scancode_to_ascii_shifted[scancode];
    } else {
        return scancode_to_ascii_table[scancode];
    }
}
```

---

## 修饰键状态管理

在实际驱动中，我们需要在中断处理器中更新修饰键状态。

### 扫描码常量

```c
#define SCANCODE_LSHIFT    0x2A
#define SCANCODE_RSHIFT    0x36
#define SCANCODE_LCTRL     0x1D
#define SCANCODE_LALT      0x38
#define SCANCODE_CAPSLOCK  0x3A
```

### 中断处理器

```c
void keyboard_irq_handler(interrupt_frame_t* frame, void* context) {
    (void)frame;
    (void)context;

    uint8_t status = inb(0x64);
    if (!(status & 0x01)) {
        pic_send_eoi(1);
        return;
    }

    uint8_t scancode = inb(0x60);
    uint8_t code = scancode & 0x7F;
    bool is_break = (scancode & 0x80) != 0;

    switch (code) {
        case SCANCODE_LSHIFT:
        case SCANCODE_RSHIFT:
            shift_pressed = !is_break;
            break;

        case SCANCODE_CAPSLOCK:
            if (!is_break) {
                caps_lock = !caps_lock;
            }
            break;

        case SCANCODE_LCTRL:
        case SCANCODE_LALT:
            // 暂时忽略，可以扩展
            break;

        default:
            // 普通键，只在 Make 时处理
            if (!is_break) {
                char c = scancode_to_ascii(scancode);
                if (c != 0) {
                    // 写入环形缓冲区
                    ring_write(&keyboard_buffer, c);
                }
            }
            break;
    }

    pic_send_eoi(1);
}
```

---

## 不可打印字符

有些按键不产生 ASCII 字符，比如功能键（F1-F12）、方向键等。这些键的扫描码在我们的查找表中对应 0，所以会被忽略。

如果你需要支持这些键，可以扩展查找表，或者使用转义序列（比如 '\x1B[A' 代表上箭头）。

---

## 常见问题

### 问题 1：大小写总是错的

**原因**：可能 Shift 和 Caps Lock 的逻辑弄反了。

**解决方案**：记住 Caps Lock 只影响字母，而 Shift 影响所有有"上档"字符的键。

### 问题 2：数字行出来的总是符号

**原因**：Shift 状态没有正确复位。

**解决方案**：确保 Break Code 也更新了 Shift 状态：

```c
if (code == SCANCODE_LSHIFT || code == SCANCODE_RSHIFT) {
    shift_pressed = !is_break;  // 重要：要根据 is_break 设置
}
```

### 问题 3：Caps Lock 状态不同步

**原因**：Caps Lock 的 LED 没有更新。

**解决方案**：发送命令更新键盘 LED（后续章节会讲）。

---

## 接下来

现在我们有了扫描码到 ASCII 的转换逻辑，下一节我们会实现完整的中断处理器，包括环形缓冲区集成和 API 封装。

→ [下一篇：键盘中断处理与集成测试](./06_键盘中断处理与集成测试.md)

---

<div align="center">

## 文档导航

[← PS/2协议与扫描码详解](./04_PS/2协议与扫描码详解.md) | [键盘中断处理与集成测试 →](./06_键盘中断处理与集成测试.md)

</div>
