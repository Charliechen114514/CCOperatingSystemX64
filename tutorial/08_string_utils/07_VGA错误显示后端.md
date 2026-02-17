# VGA 错误显示后端 —— 白字红底的死亡屏幕

上一篇我们搭好了断言系统的框架，但错误显示部分还是空的。今天我们要实现一个漂亮的 VGA 错误界面 —— 白字红底，让断言失败一目了然。说实话，当你第一次看到这个红屏幕的时候，可能会有点震惊，但你会感谢这个清晰的错误提示。

## 我们要实现的效果

当断言失败时，VGA 屏幕应该显示类似这样的信息：屏幕背景变成红色，文字变成白色，中间有一个框显示断言失败的详细信息 —— 表达式、文件名、行号、函数名。整个界面非常显眼，你不可能错过它。

## 第一步：查看 VGA 驱动接口

首先，我们需要知道 VGA 驱动提供了哪些函数。在 stage 07 中，我们实现了基础的 VGA 文本驱动。典型的 VGA 驱动接口应该包括设置颜色、清空屏幕、在指定位置打印字符等功能。

如果你的 VGA 驱动接口不同，请相应调整下面的代码。但一般来说，VGA 文本模式是标准化的，大部分接口都类似。

## 第二步：实现 VGA 错误显示函数

VGA 错误显示的核心是绘制一个框，然后在框里显示错误信息。这个框不需要太复杂，但需要足够清晰。

```c
/* 在 kernel/assert/assert_action_backend.c 中 */

#include "assert_action_backend.h"
#include "../driver/vga/vga.h"

#define VGA_COLOR_RED      4
#define VGA_COLOR_WHITE    15

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

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

    // 绘制盒子边框
    vga_put_char_at('╔', box_row, box_col);
    for (int i = 1; i < box_width - 1; i++) {
        vga_put_char_at('═', box_row, box_col + i);
    }
    vga_put_char_at('╗', box_row, box_col + box_width - 1);

    for (int i = 1; i < box_height - 1; i++) {
        vga_put_char_at('║', box_row + i, box_col);
        vga_put_char_at('║', box_row + i, box_col + box_width - 1);
    }

    vga_put_char_at('╚', box_row + box_height - 1, box_col);
    for (int i = 1; i < box_width - 1; i++) {
        vga_put_char_at('═', box_row + box_height - 1, box_col + i);
    }
    vga_put_char_at('╝', box_row + box_height - 1, box_col + box_width - 1);

    // 显示标题
    int title_col = box_col + (box_width - 16) / 2;
    vga_move_cursor(box_row + 1, title_col);
    vga_puts("ASSERTION FAILED");

    // 显示断言信息
    int text_col = box_col + 4;
    int text_row = box_row + 4;

    vga_move_cursor(text_row++, text_col);
    vga_puts("Expression: ");
    vga_puts(ctx->expr);

    vga_move_cursor(text_row++, text_col);
    vga_puts("File: ");
    vga_puts(ctx->file);

    // 转换行号
    char line_str[16];
    int_to_str(ctx->line, line_str);
    vga_move_cursor(text_row++, text_col);
    vga_puts("Line: ");
    vga_puts(line_str);

    vga_move_cursor(text_row++, text_col);
    vga_puts("Function: ");
    vga_puts(ctx->func);

    // 显示提示信息
    text_row += 2;
    int msg_col = box_col + (box_width - 30) / 2;
    vga_move_cursor(text_row, msg_col);
    vga_puts("System halted. Attach debugger.");
}
```

这里用了一个简单的 `int_to_str` 函数来把行号转换成字符串。因为我们还没有 `sprintf`，所以只能自己实现。这个函数虽然简单，但够用了。

## 第三步：实现系统停止函数

当断言失败时，我们需要停止系统。在 Debug 模式下，我们还要触发调试器断点。

```c
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

这段代码做了几件事。`cli` 禁用所有中断，这确保不会有任何干扰。在 Debug 模式下，`int3` 触发软件断点，这会让 GDB 停止执行，你可以检查变量、调用栈等。最后，`hlt` 让 CPU 进入低功耗状态，如果没有中断到来，CPU 会一直停在这里。

## 第四步：完整的 assert_failed_action

现在把两部分组合起来：

```c
void assert_failed_action(const struct ccos_assert_context* ctx)
{
    // 1. 显示错误信息
    assert_failed_display(ctx);

    // 2. 停止系统
    assert_failed_halt();

    // Never returns - the system is halted
}
```

## 循环依赖问题

你可能会遇到一个问题：断言系统需要 VGA 驱动，但 VGA 驱动可能也想使用断言。这会产生循环依赖。

解决方案是分层设计。基础 VGA 函数不使用断言，它们是"原子"操作。包装函数可以使用断言，而断言后端只使用基础函数。这样就避免了循环依赖。

或者更简单的方法：让断言后端完全不使用断言。因为我们已经在处理断言失败了，再触发断言没有意义。你可以直接调用 VGA 基础函数，不用担心。

## 使用 GDB 验证

如果你用 GDB 调试，断言失败时会触发断点。你可以用 `backtrace` 命令查看调用栈，用 `print` 命令检查变量。这比单纯看红屏幕要有用得多。

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
```

## 常见问题

有人可能会问，为什么不直接用 `printf` 打印错误？因为 `printf` 本身可能有 Bug，也可能依赖还没初始化的组件。直接操作 VGA 是最可靠的方式。

另一个问题是，可以用串口输出错误信息吗？当然可以！你可以同时输出到 VGA 和串口，这样即使没有显示器也能看到错误信息。我们后续会扩展断言后端来支持这个。

关于 Release 构建是否应该保留断言，这取决于你的需求。`CCOS_ASSERT` 始终启用，用于关键检查；`CCOS_DEBUG_ASSERT` 仅 Debug 启用，用于调试辅助。这种设计让你可以在发布版本保持一定的错误检测能力，同时不影响性能。

## 测试断言系统

让我们创建一个简单的测试来验证断言系统是否工作。

```c
void test_assertion(void) {
    vga_puts("Testing assertion system...\n");
    vga_puts("About to trigger a failing assertion.\n");
    vga_puts("You should see a red error screen.\n");

    // 这个断言会失败
    CCOS_ASSERT(1 == 2);

    // 下面的代码永远不会执行
    vga_puts("This should not appear.\n");
}
```

编译运行，你应该看到：正常的启动信息，然后屏幕突然变红，显示断言失败的详细信息，系统停止。测试完后记得删除这行，否则每次启动都会失败！

说实话，实现这个 VGA 错误显示的过程让我对内核调试有了更深的理解。以前我觉得调试就是打印信息，但真正做内核开发的时候才发现，一个清晰的错误提示可以节省很多时间。

断言系统完成了！现在我们有了一个可靠的错误检测和报告机制。但在内核开发中，每次修改代码都要重新编译、启动 QEMU，这套流程太慢了。在下一篇文档中，我们会实现一个主机环境的测试框架，让代码验证变得快速而简单。


---

<div align="center">

## 文档导航

[← 实现断言系统](06_实现断言系统.md)  | [主机环境测试框架 →](08_主机环境测试框架.md)

</div>
