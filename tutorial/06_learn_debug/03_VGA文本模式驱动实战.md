# VGA 文本模式驱动实战

调试的时候，我们需要看到一些输出。虽然可以用串口，但 VGA 直接显示在屏幕上，更直观一些。

---

## 第一步：理解 VGA 硬件

### 显存地址

VGA（Video Graphics Array）是 IBM PC 兼容机的标准显示接口。在文本模式下：

- **显存基地址**：`0xB8000`（彩色文本模式）
- **分辨率**：80 字符 × 25 行
- **每个字符**：2 字节（1 字符 + 1 属性）

### VGA 条目格式

显存中的每个字符占用 2 字节：

```
位 15-8: 属性字节
  位 7-4: 背景色 (0-15)
  位 3-0: 前景色 (0-15)

位 7-0: ASCII 字符
```

举个例子，要在屏幕左上角显示白色的 'A'（黑色背景）：

```
地址       内容
0xB8000:   0x41  ('A')
0xB8001:   0x0F  (白色前景，黑色背景)
```

### 颜色代码

| 值 | 颜色 | 显示效果 |
|----|------|----------|
| 0 | 黑色 | Black |
| 1 | 蓝色 | Blue |
| 2 | 绿色 | Green |
| 3 | 青色 | Cyan |
| 4 | 红色 | Red |
| 5 | 品红 | Magenta |
| 6 | 棕色 | Brown |
| 7 | 浅灰 | Light Grey |
| 8-15 | 对应颜色的"亮"版本 | Bright Colors |

⚠️ **注意**：在传统的 VGA 文本模式下，颜色值是 4 位的，所以范围是 0-15。

---

## 第二步：实现 VGA 结构体

### 创建目录结构

```bash
# 在项目根目录执行
mkdir -p kernel/driver/vga
```

### 创建 vga_config.h

首先定义 VGA 的硬件常量：

```c
/**
 * @file vga_config.h
 * @brief VGA 硬件配置常量
 */

#pragma once

#define VGA_BASE_ADDR 0xB8000
#define VGA_WIDTH     80
#define VGA_HEIGHT    25
```

### 创建 vga.h

```c
/**
 * @file vga.h
 * @brief VGA 文本模式驱动接口
 */

#pragma once
#include "defines/types.h"

typedef struct CCOS_VGA CCOS_VGA;
typedef uint8_t vga_sz_t;
typedef uint16_t vga_cursor_t;

// VGA 颜色枚举
typedef enum vga_color_t {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_BRIGHT_BLUE = 9,
    VGA_COLOR_BRIGHT_GREEN = 10,
    VGA_COLOR_BRIGHT_CYAN = 11,
    VGA_COLOR_BRIGHT_RED = 12,
    VGA_COLOR_BRIGHT_MAGENTA = 13,
    VGA_COLOR_YELLOW = 14,
    VGA_COLOR_WHITE = 15
} vga_color_t;

// VGA 可设置属性
typedef enum VGA_PROPERTY {
    CURSOR_X,
    CURSOR_Y,
    CURSOR_FONT_COLOR,
    CURSOR_BACKGROUND_COLOR
} vga_property_t;

// VGA 设备结构体
typedef struct CCOS_VGA {
    volatile char* base_addr;        // 显存基地址
    vga_sz_t width;                  // 屏幕宽度（字符数）
    vga_sz_t height;                 // 屏幕高度（字符数）
    vga_cursor_t native_cursor_pos;  // 光标位置（X在高字节，Y在低字节）
    vga_color_t font_color;          // 当前前景色
    vga_color_t background_color;    // 当前背景色
} CCOS_VGA;

// 初始化函数
void system_vga_init(void);

// 获取 VGA 实例
CCOS_VGA* vga_instance(void);

// 清屏
void vga_clear(CCOS_VGA* vga, vga_color_t background);

// 获取/设置光标位置
vga_cursor_t vga_get_cursor(const CCOS_VGA* vga);
void vga_set_cursor(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y);

// 设置属性
void set_vga_property(CCOS_VGA* vga, void* data, vga_property_t what_property);

// 滚屏
void vga_scroll(CCOS_VGA* vga, int lines);

// 打印字符串
void vga_print_string(CCOS_VGA* vga, const char* string);
void vga_print_stringn(CCOS_VGA* vga, const char* string, vga_sz_t str_sz);
```

⚠️ **注意**：`base_addr` 是 `volatile char*`，因为我们要直接操作硬件内存，必须告诉编译器不要优化掉这些访问。

---

## 第三步：实现核心函数

### 创建 vga.c

```c
/**
 * @file vga.c
 * @brief VGA 文本模式驱动实现
 */

#include "vga.h"
#include "vga_config.h"

// 内部 VGA 实例
static CCOS_VGA internal_vga_instance;

// 获取 VGA 实例（单例模式）
CCOS_VGA* vga_instance(void) {
    return &internal_vga_instance;
}

// 初始化 VGA 系统
void system_vga_init(void) {
    internal_vga_instance.height = VGA_HEIGHT;
    internal_vga_instance.width = VGA_WIDTH;
    internal_vga_instance.base_addr =
        (char*)(uintptr_t)VGA_BASE_ADDR;
    internal_vga_instance.native_cursor_pos = 0;   // (0, 0)
    internal_vga_instance.font_color = VGA_COLOR_WHITE;
    internal_vga_instance.background_color = VGA_COLOR_BLACK;
}

// 构造 VGA 条目（字符 + 颜色属性）
static inline uint16_t vga_entry(char c, vga_color_t font, vga_color_t background) {
    // 低8位：ASCII字符
    // 高8位：属性字节（背景色<<4 | 前景色）
    return (uint16_t)c | ((uint16_t)(background << 4 | font) << 8);
}

// 清屏
void vga_clear(CCOS_VGA* vga, vga_color_t background) {
    if (vga == NULL)
        return;

    uint16_t blank = vga_entry(' ', VGA_COLOR_BLACK, background);
    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;

    for (vga_sz_t y = 0; y < vga->height; y++) {
        for (vga_sz_t x = 0; x < vga->width; x++) {
            video[y * vga->width + x] = blank;
        }
    }

    // 重置光标到 (0, 0)
    vga->native_cursor_pos = 0;
    vga->background_color = background;
}

// 获取光标位置
vga_cursor_t vga_get_cursor(const CCOS_VGA* vga) {
    if (vga == NULL)
        return 0;
    return vga->native_cursor_pos;
}

// 设置光标位置
void vga_set_cursor(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y) {
    if (vga == NULL)
        return;

    // 限制在屏幕范围内
    if (x >= vga->width)
        x = vga->width - 1;
    if (y >= vga->height)
        y = vga->height - 1;

    // 编码光标位置：X 在高字节，Y 在低字节
    vga->native_cursor_pos = ((uint16_t)x << 8) | (uint16_t)y;
}

// 打印单个字符（内部函数）
static void vga_putc(CCOS_VGA* vga, char c) {
    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;

    // 解码光标位置
    vga_sz_t x = (vga_sz_t)((vga->native_cursor_pos & 0xFF00) >> 8);  // 高字节
    vga_sz_t y = (vga_sz_t)(vga->native_cursor_pos & 0x00FF);          // 低字节

    if (c == '\n') {
        // 换行：移到下一行开头
        x = 0;
        y++;
        if (y >= vga->height) {
            vga_scroll(vga, 1);
            y = vga->height - 1;
        }
    } else {
        // 打印字符
        uint16_t entry = vga_entry(c, vga->font_color, vga->background_color);
        video[y * vga->width + x] = entry;

        // 前进光标
        x++;
        if (x >= vga->width) {
            x = 0;
            y++;
            if (y >= vga->height) {
                vga_scroll(vga, 1);
                y = vga->height - 1;
            }
        }
    }

    // 更新编码的光标位置
    vga->native_cursor_pos = ((uint16_t)x << 8) | (uint16_t)y;
}

// 打印字符串
void vga_print_string(CCOS_VGA* vga, const char* string) {
    if (vga == NULL || string == NULL)
        return;

    while (*string != '\0') {
        vga_putc(vga, *string);
        string++;
    }
}

// 打印定长字符串
void vga_print_stringn(CCOS_VGA* vga, const char* string, vga_sz_t str_sz) {
    if (vga == NULL || string == NULL)
        return;

    for (vga_sz_t i = 0; i < str_sz; i++) {
        vga_putc(vga, string[i]);
    }
}

// 设置属性
void set_vga_property(CCOS_VGA* vga, void* data, vga_property_t what_property) {
    if (vga == NULL || data == NULL)
        return;

    switch (what_property) {
        case CURSOR_X: {
            vga_sz_t x = *(vga_sz_t*)data;
            vga_sz_t y = (vga_sz_t)(vga->native_cursor_pos & 0x00FF);
            vga_set_cursor(vga, x, y);
            break;
        }
        case CURSOR_Y: {
            vga_sz_t y = *(vga_sz_t*)data;
            vga_sz_t x = (vga_sz_t)((vga->native_cursor_pos & 0xFF00) >> 8);
            vga_set_cursor(vga, x, y);
            break;
        }
        case CURSOR_FONT_COLOR:
            vga->font_color = *(vga_color_t*)data;
            break;
        case CURSOR_BACKGROUND_COLOR:
            vga->background_color = *(vga_color_t*)data;
            break;
        default:
            break;
    }
}

// 滚屏实现
void vga_scroll(CCOS_VGA* vga, int lines) {
    if (vga == NULL || lines == 0)
        return;

    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;
    uint16_t blank = vga_entry(' ', VGA_COLOR_BLACK, vga->background_color);
    vga_sz_t width = vga->width;
    vga_sz_t height = vga->height;

    if (lines > 0) {
        // 向上滚动（内容向上移动）
        vga_sz_t scroll_lines = (vga_sz_t)lines;
        if (scroll_lines >= height) {
            // 滚动超过屏幕高度，直接清屏
            for (vga_sz_t i = 0; i < width * height; i++) {
                video[i] = blank;
            }
        } else {
            // 移动内容向上
            for (vga_sz_t y = 0; y < height - scroll_lines; y++) {
                for (vga_sz_t x = 0; x < width; x++) {
                    video[y * width + x] = video[(y + scroll_lines) * width + x];
                }
            }
            // 清空底部行
            for (vga_sz_t y = height - scroll_lines; y < height; y++) {
                for (vga_sz_t x = 0; x < width; x++) {
                    video[y * width + x] = blank;
                }
            }
        }
    } else {
        // 向下滚动（内容向下移动）
        vga_sz_t scroll_lines = (vga_sz_t)(-lines);
        if (scroll_lines >= height) {
            for (vga_sz_t i = 0; i < width * height; i++) {
                video[i] = blank;
            }
        } else {
            // 移动内容向下
            for (vga_sz_t y = height - 1; y >= scroll_lines; y--) {
                for (vga_sz_t x = 0; x < width; x++) {
                    video[y * width + x] = video[(y - scroll_lines) * width + x];
                }
            }
            // 清空顶部行
            for (vga_sz_t y = 0; y < scroll_lines; y++) {
                for (vga_sz_t x = 0; x < width; x++) {
                    video[y * width + x] = blank;
                }
            }
        }
    }
}
```

### 解释几个关键点

**光标位置编码**：

我们将 X 和 Y 坐标打包到一个 16 位整数中：
- 高字节（8 位）：X 坐标（0-79）
- 低字节（8 位）：Y 坐标（0-24）

这样只需要一个变量就能存储光标位置。

**VGA 条目构造**：

```c
return (uint16_t)c | ((uint16_t)(background << 4 | font) << 8);
```

这行代码构造了正确的 VGA 条目格式。注意运算符优先级，我们用括号确保正确。

**换行处理**：

当遇到 `\n` 时，我们将 X 重置为 0，Y 增加 1。如果超出屏幕底部，就触发滚屏。

---

## 第四步：在内核中使用 VGA

### 更新 kernel_main.c

```c
/**
 * @file kernel_main.c
 * @brief 内核主函数
 */

#include "driver/vga/vga.h"

void kernel_main(void) {
    // 初始化 VGA
    system_vga_init();
    CCOS_VGA* vga = vga_instance();

    // 清屏
    vga_clear(vga, VGA_COLOR_BLACK);

    // 设置颜色
    vga_color_t white = VGA_COLOR_WHITE;
    set_vga_property(vga, &white, CURSOR_FONT_COLOR);

    // 打印欢迎消息
    vga_print_string(vga, "=== CCOS Kernel ===\n");
    vga_print_string(vga, "VGA driver initialized!\n");
    vga_print_string(vga, "Debug infrastructure is ready.\n");

    // 设置光标到第 5 行
    vga_sz_t row = 5;
    set_vga_property(vga, &row, CURSOR_Y);

    // 打印彩色文字
    vga_color_t colors[] = {
        VGA_COLOR_RED,
        VGA_COLOR_GREEN,
        VGA_COLOR_BLUE,
        VGA_COLOR_YELLOW,
        VGA_COLOR_CYAN
    };

    const char* messages[] = {
        "Red text\n",
        "Green text\n",
        "Blue text\n",
        "Yellow text\n",
        "Cyan text\n"
    };

    for (int i = 0; i < 5; i++) {
        set_vga_property(vga, &colors[i], CURSOR_FONT_COLOR);
        vga_print_string(vga, messages[i]);
    }

    // 无限循环（让内核停止在这里）
    while (1) {
        __asm__ volatile("hlt");
    }
}
```

---

## 第五步：运行测试

### 编译

```bash
cmake --build build
```

### 运行

```bash
qemu-system-x86_64 -drive format=raw,file=build/boot.img -vga std
```

或者使用 make 目标（如果你配置了）：

```bash
make vga-run
```

### 预期输出

你应该在 QEMU 窗口中看到：

```
=== CCOS Kernel ===
VGA driver initialized!
Debug infrastructure is ready.

Red text
Green text
Blue text
Yellow text
Cyan text
```

每行文字的颜色应该和文字描述一致。

### 调试验证

现在我们可以用 GDB 来验证 VGA 驱动：

```bash
./scripts/debug.sh
```

在 GDB 中：

```
(gdb) b system_vga_init
Breakpoint 1 at 0x...

(gdb) b vga_print_string
Breakpoint 2 at 0x...

(gdb) c
Continuing.

Breakpoint 1, 0x... in system_vga_init ()
```

单步执行，观察 VGA 显存的变化：

```
(gdb) si
...

(gdb) x/10x 0xB8000
0xb8000: 0x0f41  0x0f43  0x0f4f  0x0f53  ...
```

你会看到显存中出现了字符数据。

---

## 常见问题

### 屏幕全黑

可能原因：
1. VGA 没有初始化
2. 显存地址错误
3. QEMU 没有使用正确的 VGA 模式

检查：
- 确认调用了 `system_vga_init()`
- 确认 QEMU 参数包含 `-vga std`
- 用 GDB 查看 `0xB8000` 处的内存

### 乱码显示

可能原因：
1. VGA 条目格式错误
2. 字符编码问题

检查 `vga_entry` 函数的实现，确保属性字节在高 8 位。

### 颜色不对

检查颜色枚举值是否正确，以及 `vga_entry` 函数中的颜色编码。

---

## 总结

我们完成了 VGA 文本模式驱动：

- ✅ 理解了 VGA 硬件基础
- ✅ 实现了 VGA 结构体和 API
- ✅ 实现了颜色和光标控制
- ✅ 实现了滚屏和清屏
- ✅ 在内核中成功使用 VGA 输出

现在我们有一个可靠的输出方式，可以在调试时看到信息了。但在 VSCode 中调试才是最舒服的，下一篇我们会配置 VSCode 图形化调试。

→ [下一篇：VSCode 图形化调试配置](./04_VSCode图形化调试配置.md)


---

<div align="center">

## 文档导航

[← 从零搭建GDB远程调试](02_从零搭建GDB远程调试.md)  | [VSCode图形化调试配置 →](04_VSCode图形化调试配置.md)

</div>
