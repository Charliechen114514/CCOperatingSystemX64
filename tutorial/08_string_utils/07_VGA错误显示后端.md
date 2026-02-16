# VGA 错误显示后端

上一篇我们搭好了断言系统的框架，但错误显示部分还是空的。今天我们要实现一个漂亮的 VGA 错误界面 —— 白字红底，让断言失败一目了然。

---

## 我们要实现的效果

当断言失败时，VGA 屏幕应该显示类似这样的信息：

```
╔════════════════════════════════════════════════════════════╗
║                    ASSERTION FAILED                        ║
╠════════════════════════════════════════════════════════════╣
║                                                            ║
║  Expression: ptr != NULL                                   ║
║  File: kernel/base/string.c                                ║
║  Line: 42                                                  ║
║  Function: strlen                                          ║
║                                                            ║
║  System halted. Please attach debugger.                    ║
║                                                            ║
╚════════════════════════════════════════════════════════════╝
```

整个界面是红底白字，非常显眼。

---

## 第一步：查看 VGA 驱动接口

首先，我们需要知道 VGA 驱动提供了哪些函数。在 stage 07 中，我们实现了基础的 VGA 文本驱动。

典型的 VGA 驱动接口应该包括：

```c
// 设置颜色
void vga_set_color(uint8_t fg_color, uint8_t bg_color);

// 清空屏幕
void vga_clear(void);

// 在指定位置打印字符
void vga_put_char_at(char c, int row, int col);

// 打印字符串
void vga_puts(const char* str);
```

如果你的 VGA 驱动接口不同，请相应调整下面的代码。

---

## 第二步：实现 VGA 错误显示函数

更新 `kernel/assert/assert_action_backend.c`：

```c
/**
 * @file assert_action_backend.c
 * @brief Assertion failure action backend implementation.
 */

#include "assert_action_backend.h"
#include "../driver/vga/vga.h"

// VGA 颜色定义
#define VGA_COLOR_RED      4
#define VGA_COLOR_WHITE    15
#define VGA_COLOR_BLACK    0

// 屏幕尺寸
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/**
 * @brief Display a box on VGA screen.
 *
 * @param row Top row of the box
 * @param col Left column of the box
 * @param width Width of the box
 * @param height Height of the box
 */
static void draw_box(int row, int col, int width, int height)
{
    // 绘制上边框
    vga_put_char_at('╔', row, col);
    for (int i = 1; i < width - 1; i++) {
        vga_put_char_at('═', row, col + i);
    }
    vga_put_char_at('╗', row, col + width - 1);

    // 绘制两边和下边框
    for (int i = 1; i < height - 1; i++) {
        vga_put_char_at('║', row + i, col);
        vga_put_char_at('║', row + i, col + width - 1);
    }

    // 绘制下边框
    vga_put_char_at('╚', row + height - 1, col);
    for (int i = 1; i < width - 1; i++) {
        vga_put_char_at('═', row + height - 1, col + i);
    }
    vga_put_char_at('╝', row + height - 1, col + width - 1);
}

/**
 * @brief Display text centered in a box.
 *
 * @param row Starting row
 * @param col Starting column
 * @param width Width of the display area
 * @param text Text to display
 */
static void display_line_centered(int row, int col, int width, const char* text)
{
    size_t len = 0;
    while (text[len] != '\0') {
        len++;
    }

    // 计算居中位置
    int start_col = col + (width - len) / 2;
    if (start_col < col) {
        start_col = col;
    }

    // 移动光标并打印
    // （假设 VGA 驱动有光标移动函数）
    vga_move_cursor(row, start_col);
    vga_puts(text);
}

/**
 * @brief Display text left-aligned with padding.
 *
 * @param row Starting row
 * @param col Starting column
 * @param label Label text
 * @param value Value text
 */
static void display_label_value(int row, int col, const char* label, const char* value)
{
    vga_move_cursor(row, col);
    vga_puts(label);
    vga_puts(value);
}

/**
 * @brief Convert integer to string.
 *
 * Simple utility since we don't have sprintf yet.
 */
static void int_to_str(int value, char* buffer)
{
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    int i = 0;
    bool negative = false;

    if (value < 0) {
        negative = true;
        value = -value;
    }

    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    if (negative) {
        buffer[i++] = '-';
    }

    buffer[i] = '\0';

    // 反转字符串
    int len = i;
    for (int j = 0; j < len / 2; j++) {
        char tmp = buffer[j];
        buffer[j] = buffer[len - 1 - j];
        buffer[len - 1 - j] = tmp;
    }
}

/**
 * @brief Display assertion failure information on VGA.
 */
static void assert_failed_display(const struct ccos_assert_context* ctx)
{
    // 设置红底白字
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);

    // 清空屏幕
    vga_clear();

    // 盒子尺寸
    const int box_width = 60;
    const int box_height = 12;
    const int box_row = (VGA_HEIGHT - box_height) / 2;
    const int box_col = (VGA_WIDTH - box_width) / 2;

    // 绘制盒子
    draw_box(box_row, box_col, box_width, box_height);

    // 显示标题
    display_line_centered(box_row + 1, box_col, box_width,
                          "ASSERTION FAILED");

    // 绘制分隔线
    int sep_row = box_row + 2;
    vga_put_char_at('╠', sep_row, box_col);
    for (int i = 1; i < box_width - 1; i++) {
        vga_put_char_at('═', sep_row, box_col + i);
    }
    vga_put_char_at('╣', sep_row, box_col + box_width - 1);

    // 显示断言信息
    int text_col = box_col + 4;
    int text_row = box_row + 4;

    display_label_value(text_row++, text_col, "  Expression: ", ctx->expr);
    display_label_value(text_row++, text_col, "  File:       ", ctx->file);

    // 转换行号
    char line_str[16];
    int_to_str(ctx->line, line_str);
    display_label_value(text_row++, text_col, "  Line:       ", line_str);

    display_label_value(text_row++, text_col, "  Function:   ", ctx->func);

    // 显示提示信息
    text_row += 2;
    display_line_centered(text_row, box_col, box_width,
                          "System halted. Please attach debugger.");
}
```

---

## 第三步：实现系统停止函数

当断言失败时，我们需要停止系统。在 Debug 模式下，我们还要触发调试器断点。

```c
/**
 * @brief Notify the debugger and halt the system.
 */
static void assert_failed_halt(void)
{
    // 禁用中断
    __asm__ volatile ("cli");

#ifndef NDEBUG
    // Debug 构建：触发断点
    // 这会让 GDB 停在这里，方便检查变量状态
    __asm__ volatile ("int3");
#endif

    // 停止 CPU
    while (1) {
        __asm__ volatile ("hlt");
    }
}
```

**这段代码做了什么？**

1. **`cli`** - Clear Interrupts，禁用所有中断。这确保不会有任何干扰。

2. **`int3`** - 软件断点中断。在 Debug 模式下，这会通知 GDB 停止执行。你可以检查变量、调用栈等。

3. **`hlt`** - Halt CPU，让 CPU 进入低功耗状态。如果没有中断到来，CPU 会一直停在这里。

---

## 第四步：完整的 assert_failed_action

现在把两部分组合起来：

```c
/**
 * @brief Assertion failure action implementation.
 *
 * This function is called when an assertion fails. It displays
 * error information on VGA and halts the system.
 */
void assert_failed_action(const struct ccos_assert_context* ctx)
{
    // 1. 显示错误信息
    assert_failed_display(ctx);

    // 2. 停止系统
    assert_failed_halt();

    // Never returns - the system is halted
}
```

---

## 第五步：处理 VGA 驱动依赖

⚠️ **重要：循环依赖问题**

你可能会遇到一个问题：断言系统需要 VGA 驱动，但 VGA 驱动可能也想使用断言！

```c
// VGA 驱动代码
void vga_put_char(char c) {
    CCOS_ASSERT(vga_initialized);  // ⚠️ 但 CCOS_ASSERT 需要 VGA 来显示错误！
    // ...
}
```

**解决方案：分层设计**

1. **基础 VGA 函数**（不使用断言）
   - `vga_put_char_at_raw`
   - `vga_set_color_raw`
   - `vga_clear_raw`

2. **包装函数**（使用断言）
   - `vga_put_char` → 调用 `vga_put_char_at_raw` + `CCOS_ASSERT`
   - `vga_set_color` → 调用 `vga_set_color_raw` + `CCOS_ASSERT`

3. **断言后端**（只使用基础函数）
   - `assert_failed_display` → 只调用 `_raw` 函数

这样断言系统就不会触发递归了。

**简化方案**

如果你不想这么复杂，可以让断言后端完全不使用断言：

```c
static void assert_failed_display(const struct ccos_assert_context* ctx)
{
    // 直接调用 VGA 基础函数，不使用断言
    // 因为这里已经在处理断言失败了
    vga_set_color_raw(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_clear_raw();
    // ...
}
```

---

## 第六步：测试断言系统

让我们创建一个简单的测试来验证断言系统是否工作：

```c
/* 在 kernel_main.c 或其他合适的地方 */

#include "assert/assert.h"

void test_assertion(void)
{
    vga_puts("Testing assertion system...\n");
    vga_puts("About to trigger a failing assertion.\n");
    vga_puts("You should see a red error screen.\n");

    // 这个断言会失败
    CCOS_ASSERT(1 == 2);  // 永远是 false！

    // 下面的代码永远不会执行
    vga_puts("This should not appear.\n");
}
```

编译运行，你应该看到：
1. 正常的启动信息
2. 测试信息
3. 屏幕突然变红
4. 显示断言失败的详细信息
5. 系统停止

**使用 GDB 验证**

如果你用 GDB 调试，断言失败时会触发断点：

```bash
$ gdb
(gdb) target remote :1234
(gdb) c
Continuing.

Program received signal SIGTRAP, Trace/breakpoint trap.
assert_failed_halt () at kernel/assert/assert_action_backend.c:XXX
(gdb) backtrace
#0  assert_failed_halt ()
#1  assert_failed_action (ctx=0x...)
#2  ccos_assert_impl (condition=false, ...)
#3  test_assertion ()
...
```

---

## 常见问题

### Q: 为什么不直接用 `printf` 打印错误？

A: 因为 `printf` 本身可能有 Bug，也可能依赖还没初始化的组件。直接操作 VGA 是最可靠的方式。

### Q: 可以用串口输出错误信息吗？

A: 当然可以！你可以同时输出到 VGA 和串口，这样即使没有显示器也能看到错误信息。我们后续会扩展断言后端来支持这个。

### Q: Release 构建应该保留断言吗？

A: 这取决于你的需求：
- `CCOS_ASSERT` 始终启用，用于关键检查
- `CCOS_DEBUG_ASSERT` 仅 Debug 启用，用于调试辅助

---

## 检查清单

在继续下一篇文章之前，请确认：

- [ ] 实现了 `assert_failed_display` 函数
- [ ] 实现了 `assert_failed_halt` 函数
- [ ] 实现了 `assert_failed_action` 函数
- [ ] 理解了 VGA 驱动的基础接口
- [ ] 理解了循环依赖问题和解决方案
- [ ] 测试了断言失败时的显示效果
- [ ] 用 GDB 验证了断点触发

---

## 接下来

断言系统完成了！现在我们有了一个可靠的错误检测和报告机制。

但在内核开发中，每次修改代码都要重新编译、启动 QEMU，这套流程太慢了。在下一篇文档中，我们会实现一个主机环境的测试框架，让代码验证变得快速而简单。

我们下次见！


---

<div align="center">

## 文档导航

[← 实现断言系统](06_实现断言系统.md)  | [主机环境测试框架 →](08_主机环境测试框架.md)

</div>
