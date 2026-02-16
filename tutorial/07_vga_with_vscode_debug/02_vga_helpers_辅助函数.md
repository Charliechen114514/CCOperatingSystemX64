# VGA 辅助函数 —— 从定位输出开始

我们现在要做的，是给 VGA 驱动添加一些实用的小工具函数。

说实话，之前的 VGA 驱动只能像老式打字机一样，从左到右、从上到下顺序输出字符。想在屏幕指定位置画个东西？抱歉，不行。

所以我们要解决这个问题。

---

## 我们的起点

先看看现有的 VGA 驱动长什么样：

```c
// kernel/driver/vga/vga.h
typedef struct CCOS_VGA {
    volatile char* base_addr;    // 显存基地址：0xB8000
    vga_sz_t width;              // 屏幕宽度：80
    vga_sz_t height;             // 屏幕高度：25
    vga_cursor_t native_cursor_pos;  // 当前光标位置
    vga_color_t font_color;      // 前景色
    vga_color_t background_color; // 背景色
} CCOS_VGA;

// 基础接口
void vga_clear(CCOS_VGA* vga, vga_color_t background);
void vga_print_string(CCOS_VGA* vga, const char* string);
void vga_set_cursor(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y);
```

这些功能可以用，但要做图形界面就有点力不从心了。

---

## 第一步 —— 创建辅助函数文件

我们首先创建两个新文件：

- `kernel/driver/vga/vga_helpers.h` —— 头文件
- `kernel/driver/vga/vga_helpers.c` —— 实现文件

### 创建头文件

```bash
# 创建 vga_helpers.h
cat > kernel/driver/vga/vga_helpers.h << 'EOF'
/**
 * @file vga_helpers.h
 * @author Charliechen114514
 * @brief VGA Helper Functions
 * @version 0.1
 * @date 2026-02-16
 */

#pragma once
#include "defines/types.h"
#include "vga.h"

// Get the X coordinate from cursor position
static inline vga_sz_t vga_cursor_x(const CCOS_VGA* vga) {
    return (vga_sz_t)((vga->native_cursor_pos & 0xFF00) >> 8);
}

// Get the Y coordinate from cursor position
static inline vga_cursor_y(const CCOS_VGA* vga) {
    return (vga_sz_t)(vga->native_cursor_pos & 0x00FF);
}

// Make VGA entry (character with color attribute)
static inline uint16_t vga_make_entry(char c, vga_color_t font, vga_color_t background) {
    return (uint16_t)c | ((uint16_t)(background << 4 | font) << 8);
}

// Simple delay loop
void vga_delay(uint32_t count);

// Put a single character at specified position with colors
void vga_put_char_at(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, char c,
                     vga_color_t font, vga_color_t bg);
EOF
```

⚠️ **注意**
我这里是直接用 `cat` 命令创建文件的，你也可以用编辑器创建。关键是内容要一致。

---

## 第二步 —— 理解光标位置编码

这里有个小坑需要先解释一下。

我们的 VGA 结构体用 `native_cursor_pos` 来存储光标位置，但它不是简单的 `(x, y)` 坐标。它的编码方式是：

```
[15:8] - X 坐标 (高字节)
[7:0]  - Y 坐标 (低字节)
```

所以解码的时候：

```c
// 获取 X 坐标
static inline vga_sz_t vga_cursor_x(const CCOS_VGA* vga) {
    return (vga_sz_t)((vga->native_cursor_pos & 0xFF00) >> 8);
}

// 获取 Y 坐标
static inline vga_sz_t vga_cursor_y(const CCOS_VGA* vga) {
    return (vga_sz_t)(vga->native_cursor_pos & 0x00FF);
}
```

为什么要这样编码？说实话，这有点任性。可能当时觉得 `x << 8 | y` 这样编码比较方便。反正现在已经这样了，我们就按照这个规则来解码。

---

## 第三步 —— 实现 vga_put_char_at()

这是最核心的函数，让我们在指定位置画一个字符。

### 实现代码

```bash
cat > kernel/driver/vga/vga_helpers.c << 'EOF'
/**
 * @file vga_helpers.c
 * @author Charliechen114514
 * @brief VGA Helper Functions
 * @version 0.1
 * @date 2026-02-16
 */

#include "vga_helpers.h"
#include "vga.h"

// Simple delay loop (no precise timing in kernel)
void vga_delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm__ volatile("nop");
    }
}

// Put a single character at specified position with colors
void vga_put_char_at(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, char c,
                     vga_color_t font, vga_color_t bg) {
    if (vga == NULL || x >= vga->width || y >= vga->height)
        return;

    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;
    uint16_t entry = (uint16_t)c | ((uint16_t)(bg << 4 | font) << 8);
    video[y * vga->width + x] = entry;
}
EOF
```

### 代码解析

这里有几个关键点：

1. **边界检查**：
   ```c
   if (vga == NULL || x >= vga->width || y >= vga->height)
       return;
   ```
   这一步非常重要！如果 `x` 或 `y` 超出屏幕范围，直接写入会导致访问无效内存，系统可能会崩溃。

2. **显存地址转换**：
   ```c
   volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;
   ```
   VGA 显存基地址是 `0xB8000`，每个字符占用 2 字节（1 字节字符 + 1 字节颜色属性）。

3. **VGA 条目编码**：
   ```c
   uint16_t entry = (uint16_t)c | ((uint16_t)(bg << 4 | font) << 8);
   ```
   VGA 的颜色编码格式是：
   - `[15:8]` —— 属性字节
     - `[7:4]` —— 背景色
     - `[3:0]` —— 前景色
   - `[7:0]` —— ASCII 字符

   所以正确的编码是：`(bg << 4 | font) << 8 | c`

4. **计算偏移**：
   ```c
   video[y * vga->width + x] = entry;
   ```
   在 80x25 的文本模式下，偏移 = `y * 80 + x`

⚠️ **千万别搞错颜色编码！**
常见错误是写成 `(font << 4 | bg) << 8`，这样前景色和背景色就颠倒了，显示出来颜色完全不对。

---

## 第四步 —— 理解 vga_delay()

延迟函数看起来很简单，但有几个值得注意的点：

```c
void vga_delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm__ volatile("nop");
    }
}
```

### 为什么用 `volatile`？

`volatile` 关键字告诉编译器："不要优化这个循环"。

如果没有 `volatile`，编译器可能会把这个空循环优化掉，延迟就失效了。

### 为什么用 `nop` 指令？

`nop` 是 "No Operation" 的缩写，也就是"什么都不做"。它在汇编层面占一个指令周期，保证循环体确实有操作。

### 延迟精度问题

说实话，这个延迟函数一点都不精确。CPU 速度快，延迟就短；CPU 速度慢，延迟就长。

但在我们这种没有操作系统的环境下，这就是唯一的选择了。后续我们会通过实验找到合适的延迟值。

---

## 第五步 —— 更新 CMakeLists.txt

新文件创建好了，现在要告诉构建系统把它们编译进去。

### 修改 CMakeLists.txt

找到 `kernel/driver/vga/CMakeLists.txt`（或者根目录的 CMakeLists.txt，取决于你的项目结构），添加新文件：

```cmake
# VGA 驱动相关
target_sources(kernel PRIVATE
    kernel/driver/vga/vga.c
    kernel/driver/vga/vga_helpers.c  # 新增
    # ... 其他文件
)

# 确保头文件路径正确
target_include_directories(kernel PRIVATE
    kernel/driver/vga
    kernel/defines
    # ... 其他路径
)
```

如果你的项目结构不同，请相应调整。关键是要把 `vga_helpers.c` 加入编译。

---

## 第六步 —— 验证编译

现在让我们构建一下，确保没有语法错误：

```bash
# 清理并重新构建
rm -rf build/
cmake -DCMAKE_BUILD_TYPE=Debug -B build
cmake --build build
```

如果一切正常，你应该看到类似这样的输出：

```
[ 25%] Building C object kernel/driver/vga/CMakeFiles/vga.dir/vga_helpers.c.o
[ 50%] Linking C static library libvga.a
[100%] Built target kernel.elf
```

如果有错误，检查：

1. 头文件 `#include` 路径是否正确
2. 函数声明和实现是否匹配
3. CMakeLists.txt 是否正确添加了新文件

---

## 第七步 —— 测试一下

让我们写个简单的测试程序，验证 `vga_put_char_at()` 是否工作。

### 修改内核主函数

找到 `kernel/kernel_main.c`（或类似的入口文件），添加测试代码：

```c
#include "driver/vga/vga.h"
#include "driver/vga/vga_helpers.h"

void kernel_main(void) {
    CCOS_VGA* vga = vga_instance();

    // 清屏
    vga_clear(vga, VGA_COLOR_BLACK);

    // 测试：在屏幕中央画一个 'X'
    vga_put_char_at(vga, 40, 12, 'X', VGA_COLOR_BRIGHT_RED, VGA_COLOR_BLACK);

    // 测试：画一些彩色的字符
    for (int i = 0; i < 10; i++) {
        vga_put_char_at(vga, 35 + i, 14, 'A' + i,
                       (vga_color_t)(VGA_COLOR_BRIGHT_BLUE + i),
                       VGA_COLOR_BLACK);
    }

    // 测试：延迟函数
    vga_delay(10000000);  // 延迟一会儿

    // 在右上角画个标记
    vga_put_char_at(vga, 78, 0, '!', VGA_COLOR_BRIGHT_GREEN, VGA_COLOR_BLACK);

    // 主循环
    while (1) {
        __asm__ volatile("hlt");
    }
}
```

### 构建并运行

```bash
# 重新构建
cmake --build build

# VGA 模式运行
cmake --build build --target vga-run
```

在另一个终端连接 VNC：

```bash
vncviewer localhost:5900
```

你应该能看到：

```
                                     X
          ABCDEFGHIJ                  !
```

如果看到了，恭喜！VGA 辅助函数工作正常。

---

## 常见问题

### 问题 1：编译报错 "undefined reference to vga_put_char_at"

**原因**：链接时找不到 `vga_helpers.c` 编译的目标文件

**解决**：检查 CMakeLists.txt，确保 `vga_helpers.c` 被加入编译

### 问题 2：屏幕上什么都没有

**可能原因**：
1. VGA 没有初始化
2. 延迟太短，还没看清就结束了
3. VNC 没有连接

**解决**：
```c
// 确保先初始化 VGA
CCOS_VGA* vga = vga_instance();
vga_clear(vga, VGA_COLOR_BLACK);

// 增加延迟
vga_delay(100000000);  // 增加到 100M
```

### 问题 3：颜色显示不对

**原因**：颜色编码错误

**检查**：
```c
// 正确的编码格式
uint16_t entry = (uint16_t)c | ((uint16_t)(bg << 4 | font) << 8);

// 常见错误
uint16_t entry = (uint16_t)c | ((uint16_t)(font << 4 | bg) << 8);  // 错误！
```

---

## 总结

到这里，我们已经实现了 VGA 辅助函数的基础功能：

| 函数 | 功能 | 用途 |
|------|------|------|
| `vga_cursor_x()` | 获取光标 X 坐标 | 读取当前水平位置 |
| `vga_cursor_y()` | 获取光标 Y 坐标 | 读取当前垂直位置 |
| `vga_make_entry()` | 创建 VGA 条目 | 编码字符和颜色 |
| `vga_delay()` | 延迟函数 | 动画效果必需 |
| `vga_put_char_at()` | 定位字符输出 | 所有绘图功能的基础 |

这些函数看起来简单，但它们是所有后续图形功能的基础。没有 `vga_put_char_at()`，我们画不出矩形、面板、进度条 —— 任何图形都画不出来。

下一章，我们将基于这些辅助函数，构建完整的 GUI 绘图库。

准备好了吗？让我们继续！


---

<div align="center">

## 文档导航

[← 为什么要做这些](01_为什么要做这些.md)  | [gui_helper绘图库 →](03_gui_helper_绘图库.md)

</div>
