# GUI 绘图库 —— 矩形、面板和进度条

有了 `vga_put_char_at()` 这个"画笔"，我们就可以构建更高级的绘图功能了。

说实话，每次画个矩形都要写一堆循环代码，真的很烦。而且同样的代码到处复制，维护起来简直是噩梦。

所以我们要做一个 GUI 绘图库 —— 把常用的绘图功能封装成函数，随用随调。

---

## 我们要做什么？

我们将实现以下功能：

| 功能 | 函数名 | 说明 |
|------|--------|------|
| 空心矩形 | `vga_draw_rect()` | 画一个矩形边框 |
| 填充矩形 | `vga_draw_fill_rect()` | 用字符填充矩形区域 |
| 水平线 | `vga_draw_hline()` | 画一条水平线 |
| 垂直线 | `vga_draw_vline()` | 画一条垂直线 |
| 面板 | `vga_draw_panel()` | 带标题的矩形边框 |
| 居中文本 | `vga_draw_text_centered()` | 在区域内居中显示文本 |
| 进度条 | `vga_draw_bar()` | 绘制进度条 |

---

## 第一步 —— 创建 GUI Helper 目录

我们先创建一个新的目录来存放 GUI 相关代码：

```bash
# 创建 gui_helper 目录
mkdir -p kernel/driver/vga/gui_helper

# 创建头文件
touch kernel/driver/vga/gui_helper/gui_helper.h

# 创建实现文件
touch kernel/driver/vga/gui_helper/gui_helper.c
```

为什么要单独开个目录？因为 GUI 功能可能会越来越多，单独管理更清晰。

---

## 第二步 —— 编写头文件

先写接口定义，这是我们的"设计图纸"：

```bash
cat > kernel/driver/vga/gui_helper/gui_helper.h << 'EOF'
/**
 * @file gui_helper.h
 * @author Charliechen114514
 * @brief VGA GUI Helper Functions - Common GUI drawing primitives
 * @version 0.1
 * @date 2026-02-16
 */

#pragma once
#include "vga/vga.h"

// Draw a rectangle border at specified position
void vga_draw_rect(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                   vga_sz_t width, vga_sz_t height, vga_color_t color);

// Draw a filled rectangle with specified color
void vga_draw_fill_rect(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                        vga_sz_t width, vga_sz_t height,
                        char fill_char, vga_color_t font, vga_color_t bg);

// Draw a horizontal line
void vga_draw_hline(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                    vga_sz_t length, char line_char, vga_color_t color);

// Draw a vertical line
void vga_draw_vline(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                    vga_sz_t length, char line_char, vga_color_t color);

// Panel structure for organized GUI layout
typedef struct {
    vga_sz_t x;
    vga_sz_t y;
    vga_sz_t width;
    vga_sz_t height;
    vga_color_t border_color;
    vga_color_t bg_color;
    vga_color_t text_color;
    const char* title;
} vga_panel_t;

// Draw a panel with border and optional title
void vga_draw_panel(CCOS_VGA* vga, const vga_panel_t* panel);

// Draw text centered in a rectangular area
void vga_draw_text_centered(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                            vga_sz_t width, const char* text,
                            vga_color_t font, vga_color_t bg);

// Draw a horizontal bar (useful for progress bars)
void vga_draw_bar(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                  vga_sz_t width, vga_sz_t filled,
                  char fill_char, vga_color_t fill_color,
                  vga_color_t empty_color);
EOF
```

---

## 第三步 —— 实现基础图形函数

现在我们来写实现代码。

### 空心矩形

```bash
cat > kernel/driver/vga/gui_helper/gui_helper.c << 'EOF_PART1'
/**
 * @file gui_helper.c
 * @author Charliechen114514
 * @brief VGA GUI Helper Functions - Implementation
 * @version 0.1
 * @date 2026-02-16
 */

#include "gui_helper.h"
#include "vga/vga_helpers.h"

// Draw a rectangle border at specified position
void vga_draw_rect(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                   vga_sz_t width, vga_sz_t height, vga_color_t color) {
    if (vga == NULL || width < 2 || height < 2)
        return;

    // Draw corners
    vga_put_char_at(vga, x, y, '+', color, VGA_COLOR_BLACK);
    vga_put_char_at(vga, x + width - 1, y, '+', color, VGA_COLOR_BLACK);
    vga_put_char_at(vga, x, y + height - 1, '+', color, VGA_COLOR_BLACK);
    vga_put_char_at(vga, x + width - 1, y + height - 1, '+', color, VGA_COLOR_BLACK);

    // Draw horizontal borders
    for (vga_sz_t i = 1; i < width - 1; i++) {
        vga_put_char_at(vga, x + i, y, '-', color, VGA_COLOR_BLACK);
        vga_put_char_at(vga, x + i, y + height - 1, '-', color, VGA_COLOR_BLACK);
    }

    // Draw vertical borders
    for (vga_sz_t i = 1; i < height - 1; i++) {
        vga_put_char_at(vga, x, y + i, '|', color, VGA_COLOR_BLACK);
        vga_put_char_at(vga, x + width - 1, y + i, '|', color, VGA_COLOR_BLACK);
    }
}
EOF_PART1
```

这个函数画一个这样的矩形：

```
+----------+
|          |
|          |
+----------+
```

⚠️ **注意**
- `width` 和 `height` 必须 ≥ 2，否则没有空间画边框
- 四个角用 `+`，横边用 `-`，竖边用 `|`

### 填充矩形

```bash
cat >> kernel/driver/vga/gui_helper/gui_helper.c << 'EOF_PART2'

// Draw a filled rectangle with specified color
void vga_draw_fill_rect(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                        vga_sz_t width, vga_sz_t height,
                        char fill_char, vga_color_t font, vga_color_t bg) {
    if (vga == NULL)
        return;

    for (vga_sz_t j = 0; j < height && (y + j) < vga->height; j++) {
        for (vga_sz_t i = 0; i < width && (x + i) < vga->width; i++) {
            vga_put_char_at(vga, x + i, y + j, fill_char, font, bg);
        }
    }
}
EOF_PART2
```

这个函数用指定字符填充一个矩形区域。

注意循环中的边界检查：`(y + j) < vga->height` 和 `(x + i) < vga->width`。这确保即使矩形超过屏幕边界，也不会越界访问。

### 水平线和垂直线

```bash
cat >> kernel/driver/vga/gui_helper/gui_helper.c << 'EOF_PART3'

// Draw a horizontal line
void vga_draw_hline(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                    vga_sz_t length, char line_char, vga_color_t color) {
    if (vga == NULL)
        return;

    for (vga_sz_t i = 0; i < length && (x + i) < vga->width; i++) {
        vga_put_char_at(vga, x + i, y, line_char, color, VGA_COLOR_BLACK);
    }
}

// Draw a vertical line
void vga_draw_vline(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                    vga_sz_t length, char line_char, vga_color_t color) {
    if (vga == NULL)
        return;

    for (vga_sz_t i = 0; i < length && (y + i) < vga->height; i++) {
        vga_put_char_at(vga, x, y + i, line_char, color, VGA_COLOR_BLACK);
    }
}
EOF_PART3
```

这两个函数很简单，就是沿着水平或垂直方向画一串字符。

---

## 第四步 —— 实现面板系统

面板是一个带标题的矩形边框，这是我们 UI 的基础组件。

```bash
cat >> kernel/driver/vga/gui_helper/gui_helper.c << 'EOF_PART4'

// Draw a panel with border and optional title
void vga_draw_panel(CCOS_VGA* vga, const vga_panel_t* panel) {
    if (vga == NULL || panel == NULL)
        return;

    // Draw border
    vga_draw_rect(vga, panel->x, panel->y, panel->width, panel->height,
                  panel->border_color);

    // Draw title if provided
    if (panel->title != NULL) {
        // Calculate title length (manually, since we don't have strlen)
        vga_sz_t title_len = 0;
        const char* p = panel->title;
        while (*p != '\0') {
            title_len++;
            p++;
        }

        if (title_len > 0 && title_len < panel->width - 2) {
            // Calculate centered position
            vga_sz_t title_x = panel->x + (panel->width - title_len) / 2;

            // Draw title with brackets
            vga_put_char_at(vga, panel->x, panel->y, ' ',
                           panel->border_color, VGA_COLOR_BLACK);
            vga_put_char_at(vga, panel->x + 1, panel->y, '[',
                           panel->border_color, VGA_COLOR_BLACK);

            for (vga_sz_t i = 0; i < title_len && (title_x + i + 2) < (panel->x + panel->width - 1); i++) {
                vga_put_char_at(vga, title_x + i + 2, panel->y, panel->title[i],
                               panel->text_color, VGA_COLOR_BLACK);
            }

            vga_put_char_at(vga, title_x + title_len + 2, panel->y, ']',
                           panel->border_color, VGA_COLOR_BLACK);
        }
    }
}
EOF_PART4
```

面板效果如下：

```
[  Title  ]
+----------+
|          |
|          |
+----------+
```

标题会显示在顶部边框中央，用方括号包裹。

⚠️ **标题宽度不能超过面板宽度减 2**
因为要留出两个字符放 `[` 和 `]`，所以标题长度必须 ≤ `width - 2`。

---

## 第五步 —— 实现布局工具

### 居中文本

```bash
cat >> kernel/driver/vga/gui_helper/gui_helper.c << 'EOF_PART5'

// Draw text centered in a rectangular area
void vga_draw_text_centered(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                            vga_sz_t width, const char* text,
                            vga_color_t font, vga_color_t bg) {
    if (vga == NULL || text == NULL)
        return;

    // Calculate text length
    vga_sz_t text_len = 0;
    const char* p = text;
    while (*p != '\0') {
        text_len++;
        p++;
    }

    // Calculate start position for centering
    vga_sz_t start_x = x;
    if (text_len < width) {
        start_x = x + (width - text_len) / 2;
    }

    // Draw each character
    for (vga_sz_t i = 0; i < text_len && (start_x + i) < vga->width; i++) {
        vga_put_char_at(vga, start_x + i, y, text[i], font, bg);
    }
}
EOF_PART5
```

居中算法很简单：`start_x = x + (width - text_len) / 2`

如果文本长度超过区域宽度，就从左边界开始画（不截断）。

### 进度条

```bash
cat >> kernel/driver/vga/gui_helper/gui_helper.c << 'EOF_PART6'

// Draw a horizontal bar (useful for progress bars)
void vga_draw_bar(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y,
                  vga_sz_t width, vga_sz_t filled,
                  char fill_char, vga_color_t fill_color,
                  vga_color_t empty_color) {
    if (vga == NULL)
        return;

    for (vga_sz_t i = 0; i < width; i++) {
        vga_color_t color = (i < filled) ? fill_color : empty_color;
        vga_put_char_at(vga, x + i, y, fill_char, color, VGA_COLOR_BLACK);
    }
}
EOF_PART6
```

进度条效果：

```
filled = 10, width = 20:
==========          (一半填充，一半空)
```

---

## 第六步 —— 更新构建配置

### 修改 CMakeLists.txt

添加新文件到构建系统：

```cmake
# GUI Helper
target_sources(kernel PRIVATE
    kernel/driver/vga/gui_helper/gui_helper.c
    # ... 其他文件
)

# 确保头文件路径正确
target_include_directories(kernel PRIVATE
    kernel/driver/vga
    kernel/driver/vga/gui_helper
    # ... 其他路径
)
```

### 验证编译

```bash
# 清理并重新构建
rm -rf build/
cmake -DCMAKE_BUILD_TYPE=Debug -B build
cmake --build build
```

---

## 第七步 —— 测试 GUI 函数

让我们写个测试程序，看看这些函数的效果：

```c
// kernel_main.c 或专门的测试文件
#include "driver/vga/vga.h"
#include "driver/vga/gui_helper/gui_helper.h"

void test_gui_functions(void) {
    CCOS_VGA* vga = vga_instance();
    vga_clear(vga, VGA_COLOR_BLACK);

    // 测试 1: 画一个矩形
    vga_draw_rect(vga, 5, 5, 30, 10, VGA_COLOR_BRIGHT_CYAN);

    // 测试 2: 画一个填充矩形
    vga_draw_fill_rect(vga, 40, 5, 20, 5, '#', VGA_COLOR_BRIGHT_GREEN, VGA_COLOR_BLACK);

    // 测试 3: 画一个面板
    vga_panel_t panel = {
        .x = 10, .y = 18,
        .width = 40, .height = 5,
        .border_color = VGA_COLOR_YELLOW,
        .bg_color = VGA_COLOR_BLACK,
        .text_color = VGA_COLOR_WHITE,
        .title = "Test Panel"
    };
    vga_draw_panel(vga, &panel);

    // 测试 4: 居中文本
    vga_draw_text_centered(vga, 10, 20, 40, "Centered Text",
                          VGA_COLOR_BRIGHT_MAGENTA, VGA_COLOR_BLACK);

    // 测试 5: 进度条
    vga_draw_bar(vga, 10, 23, 40, 30, '=', VGA_COLOR_BRIGHT_GREEN, VGA_COLOR_DARK_GREY);

    // 测试 6: 水平线和垂直线
    vga_draw_hline(vga, 55, 5, 20, '-', VGA_COLOR_RED);
    vga_draw_vline(vga, 65, 7, 10, '|', VGA_COLOR_RED);

    // 延迟以便观察
    vga_delay(50000000);
}
```

### 运行测试

```bash
# 重新构建
cmake --build build

# VGA 模式运行
cmake --build build --target vga-run
```

在 VNC 中查看，你应该能看到各种图形元素：

```
     +------------------------------+   ####################
     |                              |   Centered Text
     |                              |   [Test Panel]
     +------------------------------+   ========================
```

---

## 常见问题

### 问题 1：面板标题不显示

**原因**：标题长度超过面板宽度

**解决**：
```c
// 确保标题长度足够小
.title = "Short"  // 不要太长
```

### 问题 2：矩形显示不全

**原因**：矩形超出屏幕边界

**解决**：
```c
// 确保矩形在屏幕内
// 最大宽度: 80 - x
// 最大高度: 25 - y
```

### 问题 3：颜色显示错误

**原因**：忘记设置 `bg_color` 参数

**检查**：
```c
// 正确
vga_put_char_at(vga, x, y, c, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
//                                                  ^前景  ^背景
```

---

## 总结

现在我们有了一个功能完整的 GUI 绘图库！

| 类别 | 函数 | 用途 |
|------|------|------|
| 基础图形 | `vga_draw_rect()` | 空心矩形 |
| | `vga_draw_fill_rect()` | 填充矩形 |
| | `vga_draw_hline()` | 水平线 |
| | `vga_draw_vline()` | 垂直线 |
| 高级组件 | `vga_draw_panel()` | 带标题的面板 |
| | `vga_draw_text_centered()` | 居中文本 |
| | `vga_draw_bar()` | 进度条 |

这些函数将成为我们构建精美启动界面的"积木"。

下一章，我们将用这些"积木"组装出一个令人惊艳的启动欢迎界面！

准备好了吗？让我们开始创作！


---

<div align="center">

## 文档导航

[← vga_helpers辅助函数](02_vga_helpers_辅助函数.md)  | [欢迎界面动画效果 →](04_欢迎界面动画效果.md)

</div>
