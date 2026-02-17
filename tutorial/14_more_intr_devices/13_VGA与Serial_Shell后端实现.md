# 13 - VGA 与 Serial Shell 后端实现

上一节我们设计了 Shell 的后端抽象架构，这一节让我们把 VGA 和 Serial 两个后端完整实现出来。包括软件光标、回显控制、提示符定制等细节，这些是让 Shell 真正可用的关键。

---

## VGA 后端的软件光标

### 为什么需要软件光标

VGA 硬件确实有光标功能，通过 CRTC 寄存器可以设置光标位置和形状。但说实话，硬件光标在某些环境下表现不太稳定，而且在某些模拟器中可能不工作。所以我们决定实现一个软件光标，完全在自己的控制之下。

### 软件光标的实现

软件光标的思路很简单：维护一个当前光标位置（cursor_x, cursor_y），在输出前先擦除光标，输出后再重新绘制。绘制的时候用一个下划线字符覆盖原位置，这样看起来就像一个光标。

需要注意的细节是：在擦除光标时要保存原位置的字符，这样才能恢复。另外，在清屏的时候要重置光标位置到左上角。

```c
// vga_shell.c
#include "shell/shell.h"
#include "driver/vga/vga.h"
#include "driver/keyboard/keyboard.h"

static vga_sz_t cursor_x = 0;
static vga_sz_t cursor_y = 0;
static bool cursor_visible = true;

static void update_cursor(void) {
    if (cursor_visible) {
        // 绘制光标（使用下划线样式）
        uint16_t attr = vga_entry('_', VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        vga_write_cell(cursor_x, cursor_y, attr);
    }
}

static void erase_cursor(void) {
    // 恢复原字符
    uint16_t cell = vga_read_cell(cursor_x, cursor_y);
    vga_write_cell_raw(cursor_x, cursor_y, cell);
}
```

### 输出函数的实现

输出函数需要处理光标的更新和屏幕滚动。每次输出前擦除光标，输出后更新光标位置，然后重新绘制光标。

遇到换行符时，光标移动到下一行开头。如果超出屏幕底部，需要滚动屏幕。遇到普通字符时，在当前位置输出，然后光标右移。如果超出屏幕右边缘，自动换行。

```c
static void vga_shell_puts(const char* str) {
    erase_cursor();

    while (*str) {
        if (*str == '\n') {
            cursor_x = 0;
            if (++cursor_y >= VGA_HEIGHT) {
                vga_scroll();
                cursor_y = VGA_HEIGHT - 1;
            }
        } else {
            vga_putc_at(cursor_x, cursor_y, *str);
            if (++cursor_x >= VGA_WIDTH) {
                cursor_x = 0;
                if (++cursor_y >= VGA_HEIGHT) {
                    vga_scroll();
                    cursor_y = VGA_HEIGHT - 1;
                }
            }
        }
        str++;
    }

    update_cursor();
}
```

---

## Serial 后端的回显控制

### 回显的必要性

串口输入和 VGA 键盘输入有一个重要的区别：串口数据发送出去后，发送方看不到是否到达接收端。所以通常需要在接收到数据后把它发回去，这就是"回显"。

回显不仅让用户知道输入被正确接收，还使得退格键等控制字符能够正常工作——当我们收到退格键时，需要发送"\b \b"序列来擦除终端上的字符。

### 回显控制的实现

我们实现一个回显开关，允许在需要时禁用回显（比如输入密码时）。在 getchar 函数中，如果回显启用，就根据接收到的字符发送相应的回显序列。

```c
// serial_shell.c
#include "shell/shell.h"
#include "driver/serial/serial_intr.h"

static bool serial_echo_enabled = true;

void serial_shell_set_echo(bool enable) {
    serial_echo_enabled = enable;
}

static char serial_shell_getchar(void) {
    char c = uart_getchar();

    // 回显
    if (serial_echo_enabled) {
        if (c == '\n' || c == '\r') {
            async_serial_puts("\r\n");
        } else if (c == '\b' || c == 127) {
            async_serial_puts("\b \b");
        } else {
            async_serial_putc(c);
        }
    }

    return c;
}
```

---

## Shell 主循环的完善

### 主循环结构

主循环是 Shell 的核心，它负责显示提示符、读取用户输入、解析命令、执行命令。使用后端抽象后，主循环不需要关心底层设备，所有 I/O 操作都通过后端接口完成。

首先我们创建 Shell 上下文，设置后端指针，初始化命令缓冲区和提示符。然后显示欢迎消息和初始提示符。

### 字符处理逻辑

在主循环中，我们逐个读取字符并处理。回车键表示命令结束，我们解析并执行命令。退格键删除前一个字符，并输出"\b \b"来更新显示。可打印字符直接添加到缓冲区并回显。

```c
int shell_run(const shell_backend_t* backend) {
    shell_context_t ctx;
    ctx.backend = backend;
    ctx.cmd_pos = 0;
    ctx.running = true;
    snprintf(ctx.prompt, sizeof(ctx.prompt), "> ");

    // 显示初始提示符
    backend->puts("\n=== CCOS Shell ===\n");
    backend->puts("Type 'help' for available commands\n");
    backend->puts(ctx.prompt);

    while (ctx.running) {
        char c = backend->getchar();

        if (c == '\n' || c == '\r') {
            // 回车：执行命令
            backend->putc('\n');
            ctx.cmd_buffer[ctx.cmd_pos] = '\0';

            if (ctx.cmd_pos > 0) {
                char* argv[SHELL_MAX_ARGS];
                int argc = parse_command(ctx.cmd_buffer, argv);

                if (argc > 0) {
                    int result = execute_command(&ctx, argc, argv);

                    // 处理退出命令
                    if (result == 1) {
                        ctx.running = false;
                    }
                }
            }

            // 显示下一个提示符
            if (ctx.running) {
                backend->puts(ctx.prompt);
                ctx.cmd_pos = 0;
            }

        } else if (c == '\b' || c == 127) {
            // 退格：删除字符
            if (ctx.cmd_pos > 0) {
                ctx.cmd_pos--;
                backend->putc('\b');
                backend->putc(' ');
                backend->putc('\b');
            }

        } else if (c >= 32 && c < 127) {
            // 可打印字符
            if (ctx.cmd_pos < SHELL_CMD_BUFFER_SIZE - 1) {
                ctx.cmd_buffer[ctx.cmd_pos++] = c;
                backend->putc(c);
            }
        }
    }

    backend->puts("\nShell exited.\n");
    return 0;
}
```

---

## 内置命令的完整实现

### exit 命令

exit 命令是最简单的内置命令，它设置 running 标志为 false，让主循环退出。返回值 1 是我们的约定，表示这个命令会退出 Shell。

```c
static int cmd_exit(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    shell_context_t* ctx = get_current_shell_context();
    ctx->running = false;
    return 1;  // 返回 1 表示退出 Shell
}
```

### echo 命令

echo 命令把它的参数输出到后端，参数之间用空格分隔。这个命令虽然简单，但在调试和脚本中很有用。

```c
static int cmd_echo(int argc, char* argv[]) {
    shell_context_t* ctx = get_current_shell_context();

    for (int i = 1; i < argc; i++) {
        if (i > 1) ctx->backend->puts(" ");
        ctx->backend->puts(argv[i]);
    }
    ctx->backend->puts("\n");

    return 0;
}
```

### ticks 命令

ticks 命令显示系统启动以来的定时器滴答数。这对于性能测试和调试很有帮助，可以让我们了解程序的运行时间。

```c
static int cmd_ticks(int argc, char* argv[]) {
    (void)argc;
    (void)argv[];

    shell_context_t* ctx = get_current_shell_context();

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Timer ticks: %llu\n", timer_get_ticks());
    ctx->backend->puts(buffer);

    return 0;
}
```

### help 命令

help 命令列出所有已注册的命令及其描述。它遍历命令表，格式化输出每个命令的信息。为了让输出更整齐，我们会对命令名进行对齐。

```c
static int cmd_help(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    shell_context_t* ctx = get_current_shell_context();

    ctx->backend->puts("Available commands:\n");
    for (int i = 0; i < g_command_count; i++) {
        ctx->backend->puts("  ");
        ctx->backend->puts(g_commands[i].name);

        // 对齐描述
        int len = strlen(g_commands[i].name);
        for (int j = len; j < 12; j++) {
            ctx->backend->puts(" ");
        }

        ctx->backend->puts("- ");
        ctx->backend->puts(g_commands[i].description);
        ctx->backend->puts("\n");
    }

    return 0;
}
```

---

## 在内核中初始化 Shell

### 注册内置命令

在内核初始化的最后阶段，我们注册所有内置命令，然后启动 Shell。首先注册基础命令如 help、clear、exit，然后注册功能命令如 time、ticks、echo。

### 启动 Shell

我们先启动 VGA Shell，如果用户从 VGA Shell 退出，再启动 Serial Shell。这样设计的好处是，用户可以通过 VGA 正常使用系统，如果 VGA 出问题或者需要远程调试，可以通过串口连接。

```c
void kernel_main(void) {
    // ... 其他初始化

    // 注册内置命令
    shell_register_command("help", "Show available commands", cmd_help);
    shell_register_command("clear", "Clear screen", cmd_clear);
    shell_register_command("time", "Show current time", cmd_time);
    shell_register_command("exit", "Exit shell", cmd_exit);
    shell_register_command("echo", "Echo arguments", cmd_echo);
    shell_register_command("ticks", "Show timer ticks", cmd_ticks);

    klog_info("Shell commands registered\n");

    // 运行 VGA Shell
    klog_info("Starting VGA shell...\n");
    shell_run(vga_shell_backend());

    // 如果 VGA Shell 退出，运行 Serial Shell
    klog_info("Starting Serial shell...\n");
    shell_run(serial_shell_backend());

    // 不应该到这里
    klog_error("Shells exited, halting...\n");
    while (1) {
        __asm__ volatile("hlt");
    }
}
```

---

## 接下来

Shell 后端已经实现完成，我们的内核现在有完整的用户交互能力了！通过 VGA 或者串口，用户可以输入命令、查看时间、调试系统。这个 Shell 虽然简单，但架构清晰，易于扩展。接下来我们会优化 kprintf，实现 ksnprintf，让格式化输出更灵活。

→ [下一篇：kprintf优化与snprintf实现](./14_kprintf优化与snprintf实现.md)

---

<div align="center">

## 文档导航

[← Shell系统设计——后端抽象架构](./12_Shell系统设计——后端抽象架构.md) | [kprintf优化与snprintf实现 →](./14_kprintf优化与snprintf实现.md)

</div>
