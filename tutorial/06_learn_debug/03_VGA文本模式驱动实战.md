# VGA 文本模式驱动实战

调试的时候，我们需要看到一些输出。虽然可以用串口，但 VGA 直接显示在屏幕上，更直观一些。

---

## 第一步：理解 VGA 硬件

在我们动手写代码之前，先了解一下 VGA 硬件是怎么工作的。VGA（Video Graphics Array）是 IBM PC 兼容机的标准显示接口，它的文本模式是最简单的显示方式。

### 显存地址和布局

在文本模式下，VGA 的显存被映射到物理地址 `0xB8000`。这意味着我们可以直接往这个地址写入数据，屏幕上就会显示相应的内容。

屏幕分辨率是 80 字符 × 25 行，总共可以显示 2000 个字符。每个字符占用 2 字节，所以整个显存是 4000 字节。

第一个字节是字符的 ASCII 码，第二个字节是颜色属性。比如要在屏幕左上角显示白色的 'A'，我们需要在 `0xB8000` 写入 `0x41`（'A' 的 ASCII 码），在 `0xB8001` 写入 `0x0F`（白色前景，黑色背景）。

### 颜色编码

VGA 支持 16 种颜色，每种颜色用一个 4 位数值表示。低 4 位是前景色（字符颜色），高 4 位是背景色。

| 值 | 颜色 | 英文名 |
|----|------|--------|
| 0 | 黑色 | Black |
| 1 | 蓝色 | Blue |
| 2 | 绿色 | Green |
| 3 | 青色 | Cyan |
| 4 | 红色 | Red |
| 5 | 品红 | Magenta |
| 6 | 棕色 | Brown |
| 7 | 浅灰 | Light Grey |
| 8-15 | 对应颜色的"亮"版本 | Bright Colors |

所以属性字节的计算方式是：`背景色 << 4 | 前景色`。白色黑色就是 `0 << 4 | 15 = 15 = 0x0F`。

---

## 第二步：创建 VGA 配置头文件

我们先把硬件相关的常量定义好，这样后面改起来也方便。

### 创建目录

```bash
mkdir -p kernel/driver/vga
```

### 创建 vga_config.h

这个文件很简单，就三个常量：

```c
#pragma once

#define VGA_WIDTH (80)
#define VGA_HEIGHT (25)
#define VGA_BASE_ADDR (0xB8000)
```

屏幕宽度 80 个字符，高度 25 行，显存基地址 `0xB8000`。这些是 VGA 文本模式的标准值，不用改。

---

## 第三步：定义 VGA 数据结构

接下来我们定义 VGA 驱动需要的数据结构和函数接口。这些都在 `vga.h` 里。

### 类型定义

首先定义几个类型别名，让代码更清晰：

```c
typedef struct CCOS_VGA CCOS_VGA;
typedef uint8_t vga_sz_t;
typedef uint16_t vga_cursor_t;
```

`vga_sz_t` 用于屏幕尺寸相关的值，`vga_cursor_t` 用于光标位置。你可能好奇为什么要定义这些别名，这主要是为了代码的可读性。看到 `vga_sz_t` 就知道这是屏幕坐标值，看到普通的 `uint8_t` 就不太清楚它的用途。

### 颜色枚举

然后是颜色枚举，把 0-15 的数值映射成有意义的名字：

```c
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
```

### VGA 设备结构体

这是核心结构体，保存 VGA 的所有状态：

```c
typedef struct CCOS_VGA {
    volatile char* base_addr;        // 显存基地址
    vga_sz_t width;                  // 屏幕宽度（字符数）
    vga_sz_t height;                 // 屏幕高度（字符数）
    vga_cursor_t native_cursor_pos;  // 光标位置（X在高字节，Y在低字节）
    vga_color_t font_color;          // 当前前景色
    vga_color_t background_color;    // 当前背景色
} CCOS_VGA;
```

这里有个关键点：`base_addr` 被声明为 `volatile char*`。`volatile` 关键字告诉编译器这个指针指向的内存可能会被程序外部改变，不要优化掉对这些内存的访问。这对于硬件操作非常重要，否则编译器可能会把一些"看起来没用"的写操作优化掉。

另一个有趣的设计是 `native_cursor_pos`。我们把 X 和 Y 坐标打包到一个 16 位整数里，高字节是 X，低字节是 Y。这样只需要一个变量就能存储光标位置，节省内存，也方便传递。

### 函数声明

最后是函数接口：

```c
void system_vga_init(void);
CCOS_VGA* vga_instance(void);
void vga_clear(CCOS_VGA* vga, vga_color_t background);
vga_cursor_t vga_get_cursor(const CCOS_VGA* vga);
void vga_set_cursor(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y);
void set_vga_property(CCOS_VGA* vga, void* data, vga_property_t what_property);
void vga_scroll(CCOS_VGA* vga, int lines);
void vga_print_string(CCOS_VGA* vga, const char* string);
void vga_print_stringn(CCOS_VGA* vga, const char* string, vga_sz_t str_sz);
```

这些函数覆盖了 VGA 操作的所有基本需求：初始化、清屏、光标控制、滚屏、字符串输出。

---

## 第四步：实现 VGA 初始化和辅助函数

现在我们开始实现这些函数。先从简单的初始化和辅助函数开始。

### 内部实例和获取函数

我们使用单例模式，全局只有一个 VGA 实例：

```c
#include "vga.h"
#include "vga_config.h"

static CCOS_VGA internal_vga_instance;

CCOS_VGA* vga_instance(void) {
    return &internal_vga_instance;
}
```

`internal_vga_instance` 是静态变量，外部无法直接访问，只能通过 `vga_instance()` 函数获取指针。这是一种简单的封装，确保 VGA 状态不会被随意修改。

### 初始化函数

初始化函数设置 VGA 的所有初始状态：

```c
void system_vga_init(void) {
    internal_vga_instance.height = VGA_HEIGHT;
    internal_vga_instance.width = VGA_WIDTH;
    internal_vga_instance.base_addr =
        (char*)(uintptr_t)VGA_BASE_ADDR;
    internal_vga_instance.native_cursor_pos = 0;   // start at (0, 0)
    internal_vga_instance.font_color = 0x0F;       // white font
    internal_vga_instance.background_color = 0x00; // black background
}
```

这里把显存地址 `0xB8000` 转换成指针。`uintptr_t` 是一个整数类型，大小足以容纳指针，然后我们把它强制转换成 `char*`。这种转换在系统编程中很常见，但需要小心。

光标位置初始化为 0，表示在 (0, 0)，也就是屏幕左上角。前景色是白色（0x0F），背景色是黑色（0x00）。

### VGA 条目构造函数

这是一个内部辅助函数，用于构造 VGA 显存条目：

```c
static inline uint16_t vga_entry(char c, vga_color_t font, vga_color_t background) {
    return (uint16_t)c | ((uint16_t)(background << 4 | font) << 8);
}
```

这行代码看起来有点复杂，让我们拆开来看。`background << 4 | font` 把背景色移到高 4 位，然后和前景色合并。比如背景色 0，前景色 15，结果是 `0 << 4 | 15 = 15`。然后 `<< 8` 把这个 8 位的颜色值移到高 8 位。最后和字符 `c` 进行或运算，把字符放在低 8 位。

所以 `vga_entry('A', 15, 0)` 的返回值是 `0x0F41`，高 8 位是颜色属性，低 8 位是字符 'A'。

`static inline` 表示这是一个内部函数，只在当前文件可见，并且建议编译器内联展开，避免函数调用开销。

---

## 第五步：实现清屏和光标控制

### 清屏函数

清屏函数用空格填充整个屏幕：

```c
void vga_clear(CCOS_VGA* vga, vga_color_t background) {
    if (vga == NULL)
        return;

    uint16_t blank = vga_entry(' ', 0x0, background);
    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;

    for (vga_sz_t y = 0; y < vga->height; y++) {
        for (vga_sz_t x = 0; x < vga->width; x++) {
            video[y * vga->width + x] = blank;
        }
    }

    vga->native_cursor_pos = 0;
    vga->background_color = background;
}
```

首先检查空指针，这是一个好习惯，防止空指针解引用导致崩溃。然后构造一个"空白"条目，就是空格字符加上指定的背景色。

接着把 `base_addr` 转换成 `uint16_t*` 指针，这样我们就可以按 16 位单位访问显存，每次写入一个完整的字符+属性。

双重循环遍历所有位置，写入空白条目。最后把光标重置到 (0, 0)，并更新背景色。

### 光标获取函数

这个函数很简单，直接返回光标位置：

```c
vga_cursor_t vga_get_cursor(const CCOS_VGA* vga) {
    if (vga == NULL)
        return 0;
    return vga->native_cursor_pos;
}
```

### 光标设置函数

设置光标需要做一些边界检查：

```c
void vga_set_cursor(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y) {
    if (vga == NULL)
        return;

    if (x >= vga->width)
        x = vga->width - 1;
    if (y >= vga->height)
        y = vga->height - 1;

    vga->native_cursor_pos = ((uint16_t)x << 8) | (uint16_t)y;
}
```

如果 X 或 Y 超出屏幕范围，我们把它"钳制"到最大有效值。比如 X = 100，屏幕宽度是 80，就会被设置成 79。

然后我们把坐标打包：`x << 8` 把 X 移到高字节，`| y` 把 Y 放在低字节。解码的时候反过来操作就行了。

---

## 第六步：实现字符输出

字符输出是 VGA 驱动的核心功能，也是最复杂的部分。

### 单字符输出

这个函数在当前光标位置输出一个字符，然后移动光标：

```c
static void vga_putc(CCOS_VGA* vga, char c) {
    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;
    vga_sz_t x = (vga_sz_t)((vga->native_cursor_pos & 0xFF00) >> 8);
    vga_sz_t y = (vga_sz_t)(vga->native_cursor_pos & 0x00FF);
```

首先解码光标位置。`& 0xFF00` 取出高字节，`>> 8` 右移 8 位得到 X 坐标。`& 0x00FF` 取出低字节就是 Y 坐标。

接下来处理换行符：

```c
    if (c == '\n') {
        x = 0;
        y++;
        if (y >= vga->height) {
            vga_scroll(vga, 1);
            y = vga->height - 1;
        }
    }
```

遇到换行符时，X 重置为 0（回到行首），Y 增加 1（移到下一行）。如果 Y 超出屏幕底部，就向上滚动一行，光标停在最后一行。

对于普通字符，我们需要把它写到显存，然后移动光标：

```c
    } else {
        uint16_t entry = vga_entry(c, vga->font_color, vga->background_color);
        video[y * vga->width + x] = entry;

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

    vga->native_cursor_pos = ((uint16_t)x << 8) | (uint16_t)y;
}
```

先构造 VGA 条目，然后计算显存位置 `y * vga->width + x`，写入条目。这个公式其实就是在二维数组中找一维索引。

然后 X 增加 1。如果 X 超出屏幕宽度，就换行（X 重置为 0，Y 增加 1）。同样需要处理 Y 超出屏幕的情况。

最后更新编码的光标位置。

### 字符串输出

有了单字符输出，字符串输出就很简单了：

```c
void vga_print_string(CCOS_VGA* vga, const char* string) {
    if (vga == NULL || string == NULL)
        return;

    while (*string != '\0') {
        vga_putc(vga, *string);
        string++;
    }
}
```

遍历字符串，每个字符调用 `vga_putc`。遇到 `\0` 就停止。

还有一个定长版本：

```c
void vga_print_stringn(CCOS_VGA* vga, const char* string, vga_sz_t str_sz) {
    if (vga == NULL || string == NULL)
        return;

    for (vga_sz_t i = 0; i < str_sz; i++) {
        vga_putc(vga, string[i]);
    }
}
```

这个函数可以输出包含 `\0` 的字符串，或者只输出字符串的前 n 个字符。

---

## 第七步：实现滚屏功能

滚屏是终端输出的重要功能，当输出超过屏幕底部时，内容向上滚动。

### 滚屏函数

```c
void vga_scroll(CCOS_VGA* vga, int lines) {
    if (vga == NULL || lines == 0)
        return;

    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;
    uint16_t blank = vga_entry(' ', 0x0, vga->background_color);
    vga_sz_t width = vga->width;
    vga_sz_t height = vga->height;
```

`lines > 0` 表示向上滚，`lines < 0` 表示向下滚。

### 向上滚动

```c
    if (lines > 0) {
        vga_sz_t scroll_lines = (vga_sz_t)lines;
        if (scroll_lines >= height) {
            for (vga_sz_t i = 0; i < width * height; i++) {
                video[i] = blank;
            }
        } else {
            for (vga_sz_t y = 0; y < height - scroll_lines; y++) {
                for (vga_sz_t x = 0; x < width; x++) {
                    video[y * width + x] = video[(y + scroll_lines) * width + x];
                }
            }
            for (vga_sz_t y = height - scroll_lines; y < height; y++) {
                for (vga_sz_t x = 0; x < width; x++) {
                    video[y * width + x] = blank;
                }
            }
        }
    }
```

如果滚动行数超过或等于屏幕高度，直接清屏。否则，先把内容向上移动：第 y 行的内容移到第 y - scroll_lines 行。移动完成后，底部的空行用空白填充。

### 向下滚动

```c
    } else {
        vga_sz_t scroll_lines = (vga_sz_t)(-lines);
        if (scroll_lines >= height) {
            for (vga_sz_t i = 0; i < width * height; i++) {
                video[i] = blank;
            }
        } else {
            for (vga_sz_t y = height - 1; y >= scroll_lines; y--) {
                for (vga_sz_t x = 0; x < width; x++) {
                    video[y * width + x] = video[(y - scroll_lines) * width + x];
                }
            }
            for (vga_sz_t y = 0; y < scroll_lines; y++) {
                for (vga_sz_t x = 0; x < width; x++) {
                    video[y * width + x] = blank;
                }
            }
        }
    }
}
```

向下滚动是向上滚动的逆操作。我们从底部开始，把内容向下移动。注意这里的循环是从 `height - 1` 向下到 `scroll_lines`，这样才能正确覆盖。

---

## 第八步：实现属性设置

最后我们实现一个通用的属性设置函数：

```c
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
```

这个函数用 `void*` 指针接收数据，然后根据属性类型转换成正确的类型。这是一种通用的设计模式，可以方便地扩展新的属性类型。

设置 X 坐标时，保持 Y 坐标不变，反之亦然。设置颜色时直接赋值。

---

## 第九步：在内核中使用 VGA

VGA 驱动写好了，现在我们在内核中使用它。

### 更新 kernel_main.c

```c
#include "driver/vga/vga.h"

void kernel_main(void) {
    system_vga_init();
    CCOS_VGA* vga = vga_instance();

    vga_clear(vga, VGA_COLOR_BLACK);

    vga_color_t white = VGA_COLOR_WHITE;
    set_vga_property(vga, &white, CURSOR_FONT_COLOR);

    vga_print_string(vga, "=== CCOS Kernel ===\n");
    vga_print_string(vga, "VGA driver initialized!\n");
    vga_print_string(vga, "Debug infrastructure is ready.\n");
```

这段代码初始化 VGA，清空屏幕，设置前景色为白色，然后打印三行欢迎信息。

### 测试彩色输出

让我们试试彩色输出：

```c
    vga_sz_t row = 5;
    set_vga_property(vga, &row, CURSOR_Y);

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

    while (1) {
        __asm__ volatile("hlt");
    }
}
```

我们定义了颜色数组和消息数组，循环打印五种颜色的文字。最后进入无限循环，`hlt` 指令让 CPU 停机等待中断。

---

## 第十步：编译和运行

### 编译

```bash
cmake --build build
```

### 运行

```bash
qemu-system-x86_64 -drive format=raw,file=build/boot.img -vga std
```

或者使用 CMake 目标：

```bash
cmake --build build --target vga-run
```

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

每行文字的颜色应该和文字描述一致。如果看到这个输出，恭喜你，VGA 驱动工作正常！

### 调试验证

让我们用 GDB 验证 VGA 驱动是否真的写入了显存：

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

Breakpoint 1, system_vga_init () at vga.c:10
```

单步执行几步，然后查看显存：

```
(gdb) x/10x 0xB8000
0xb8000: 0x0f41  0x0f43  0x0f4f  0x0f53  ...
```

你会看到显存中出现了字符数据。`0x0f41` 的低 8 位是 `0x41`（'A'），高 8 位是 `0x0f`（白色黑色）。这说明 VGA 驱动确实在往显存写入数据。

---

## 总结

我们完成了 VGA 文本模式驱动的实现。这个驱动虽然简单，但它是一个完整的硬件驱动示例：理解硬件规范、设计数据结构、实现基本功能、处理边界情况、在内核中使用。

现在我们有一个可靠的输出方式，可以在调试时看到信息了。在下一篇文章中，我们会配置 VSCode 图形化调试，让调试体验更上一层楼。

→ [下一篇：VSCode 图形化调试配置](./04_VSCode图形化调试配置.md)


---

<div align="center">

## 文档导航

[← 从零搭建GDB远程调试](02_从零搭建GDB远程调试.md)  | [VSCode图形化调试配置 →](04_VSCode图形化调试配置.md)

</div>
