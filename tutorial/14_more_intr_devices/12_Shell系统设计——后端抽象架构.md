# 12 - Shell 系统设计——后端抽象架构

现在我们已经有了键盘输入、串口输入/输出、VGA 显示这些基础组件，是时候把它们整合成一个完整的交互系统了。Shell 是操作系统与用户交互的核心接口，但设计一个只能用 VGA 或者只能用串口的 Shell 显然不够灵活。我们需要一个能够支持多种 I/O 方式的架构，这就是后端抽象设计要解决的问题。

---

## 为什么需要后端抽象

### 直接绑定的问题

假设我们一开始写了一个直接使用 VGA 和键盘的 Shell，代码写起来很直接：

```c
void shell_vga(void) {
    char buffer[128];
    int pos = 0;

    while (true) {
        vga_puts("> ");
        char c = keyboard_getchar();
        // 处理输入...
    }
}
```

现在问题来了：如果我们想支持串口，怎么办？最直接的想法是复制一份代码，把 vga_puts 换成 serial_puts，keyboard_getchar 换成 uart_getchar。但这显然不是好主意，代码重复了，以后要修改功能得改两个地方。

另一种方案是用条件判断，在代码中到处都是 if-else 来区分不同的设备。这样代码会变得很乱，而且添加新设备的时候要修改核心逻辑。

### 抽象接口的思路

更好的做法是定义一套统一的接口，让不同的 I/O 方式实现这套接口。Shell 核心代码只需要调用接口函数，不需要关心底层到底用的是 VGA 还是串口。这样当我们添加新的设备支持时，只需要实现对应的接口，Shell 核心代码完全不用动。

---

## 后端接口的设计

### 接口结构体

我们定义一个结构体，包含所有需要的函数指针。这样设计的好处是可以在运行时选择使用哪个后端，甚至可以在不同后端之间切换。

```c
typedef struct shell_backend {
    const char* name;              // 后端名称（如 "VGA", "Serial"）

    // 输出函数
    void (*puts)(const char* str);  // 输出字符串
    void (*putc)(char c);            // 输出单个字符

    // 输入函数
    bool (*haschar)(void);          // 检查是否有输入
    char (*getchar)(void);          // 读取字符（阻塞）

    // 控制函数
    void (*clear)(void);            // 清屏（可选）
} shell_backend_t;
```

### 接口设计的考虑

puts 和 putc 分离是因为某些设备（比如 VGA）实现 puts 可以批量处理字符，效率更高。haschar 和 getchar 分离是为了支持非阻塞检查，这样主程序可以轮询多个输入源。

clear 函数是可选的，因为不是所有设备都支持清屏操作。在实现中，如果设备不支持清屏，可以把这个函数指针设为 NULL，Shell 核心代码需要检查这个指针是否为空再调用。

---

## VGA 后端的实现

VGA 后端是最简单的，它直接调用我们已经实现的 VGA 和键盘驱动。输出函数调用 vga_puts 和 vga_putc，输入函数调用 keyboard_haschar 和 keyboard_getchar，清屏函数调用 vga_clear。

整个实现就是一层薄薄的包装，把 Shell 的接口映射到具体的设备驱动上。代码量不大，但很清晰地展示了后端抽象的工作方式。

```c
// vga_shell.c
#include "shell/shell.h"
#include "driver/vga/vga.h"
#include "driver/keyboard/keyboard.h"

static void vga_shell_puts(const char* str) {
    vga_puts(str);
}

static void vga_shell_putc(char c) {
    vga_putc(c);
}

static bool vga_shell_haschar(void) {
    return keyboard_haschar();
}

static char vga_shell_getchar(void) {
    return keyboard_getchar();
}

static void vga_shell_clear(void) {
    vga_clear();
}

const shell_backend_t* vga_shell_backend(void) {
    static const shell_backend_t backend = {
        .name = "VGA",
        .puts = vga_shell_puts,
        .putc = vga_shell_putc,
        .haschar = vga_shell_haschar,
        .getchar = vga_shell_getchar,
        .clear = vga_shell_clear
    };
    return &backend;
}
```

---

## Serial 后端的实现

串口后端与 VGA 后端类似，但有一些细微差别。输出函数调用我们之前实现的异步串口函数 async_serial_puts 和 async_serial_putc，输入函数调用 uart_haschar 和 uart_getchar。

清屏函数有点特殊，因为串口本身没有"清屏"的概念。我们使用 ANSI 转义序列 \033[2J\033[H 来实现清屏效果，这要求串口的另一端（比如终端模拟器）支持 ANSI 转义序列。

```c
// serial_shell.c
#include "shell/shell.h"
#include "driver/serial/serial_intr.h"

static void serial_shell_puts(const char* str) {
    async_serial_puts(str);
}

static void serial_shell_putc(char c) {
    async_serial_putc(c);
}

static bool serial_shell_haschar(void) {
    return uart_haschar();
}

static char serial_shell_getchar(void) {
    return uart_getchar();
}

static void serial_shell_clear(void) {
    async_serial_puts("\033[2J\033[H");  // ANSI 清屏
}

const shell_backend_t* serial_shell_backend(void) {
    static const shell_backend_t backend = {
        .name = "Serial",
        .puts = serial_shell_puts,
        .putc = serial_shell_putc,
        .haschar = serial_shell_haschar,
        .getchar = serial_shell_getchar,
        .clear = serial_shell_clear
    };
    return &backend;
}
```

---

## Shell 核心的数据结构

### 上下文结构

Shell 运行时需要维护一些状态，比如当前使用的后端、命令缓冲区、提示符字符串等。我们把这些状态封装在一个结构体中，这样可以同时支持多个 Shell 实例（虽然目前我们只用一个）。

```c
#define SHELL_CMD_BUFFER_SIZE 128
#define SHELL_MAX_ARGS 16

typedef struct shell_context {
    const shell_backend_t* backend;
    char cmd_buffer[SHELL_CMD_BUFFER_SIZE];
    int cmd_pos;
    char prompt[32];
    bool running;
} shell_context_t;
```

### 命令表

命令表是一个数组，存储所有已注册的命令及其处理函数。每个命令有一个名称、描述，以及一个处理函数指针。处理函数接收 argc 和 argv 参数，类似于标准的 main 函数签名。

```c
#define SHELL_MAX_COMMANDS 32

typedef struct {
    char name[32];
    char description[64];
    int (*handler)(int argc, char* argv[]);
} shell_command_t;

static shell_command_t g_commands[SHELL_MAX_COMMANDS];
static int g_command_count = 0;
```

---

## 命令解析逻辑

### 解析函数

命令解析函数把输入的字符串分解成多个 token，类似于 shell 的参数分割。我们跳过前导空白，然后逐个找到参数，遇到空白或字符串结束时停止。

每个参数以 null 终止，argv 数组存储指向这些参数的指针。这个函数返回解析出的参数数量。

```c
static int parse_command(char* cmd_line, char* argv[]) {
    int argc = 0;
    char* p = cmd_line;

    // 跳过前导空白
    while (*p == ' ' || *p == '\t') p++;

    while (*p != '\0' && argc < SHELL_MAX_ARGS) {
        argv[argc++] = p;

        // 找到 token 结尾
        while (*p != '\0' && *p != ' ' && *p != '\t') p++;

        if (*p == '\0') break;

        // Null 终止 token
        *p++ = '\0';

        // 跳过空白
        while (*p == ' ' || *p == '\t') p++;
    }

    return argc;
}
```

---

## 主循环的实现

### 主循环结构

主循环是 Shell 的核心，它负责显示提示符、读取用户输入、解析命令、执行命令。使用后端抽象后，主循环不需要关心底层设备，所有 I/O 操作都通过后端接口完成。

读取输入时，我们处理回车键（结束输入）、退格键（删除字符）和可打印字符。对于可打印字符，我们检查缓冲区是否还有空间，避免溢出。

### 命令执行

当用户按下回车后，我们解析命令并查找对应的处理函数。如果找到了就调用，如果没有找到就显示错误信息。处理函数可以返回 1 来表示退出 Shell。

---

## 命令注册机制

### 注册函数

命令注册函数允许模块化地添加命令。每个模块可以在初始化时调用 shell_register_command 来注册自己的命令，这样 Shell 核心不需要知道具体有哪些命令。

注册函数会检查命令表是否已满，以及是否已存在同名命令。如果一切正常，就把新命令添加到命令表中。

```c
int shell_register_command(const char* name, const char* description,
                           int (*handler)(int argc, char* argv[])) {
    // 检查是否已满
    if (g_command_count >= SHELL_MAX_COMMANDS) {
        return -1;
    }

    // 检查重复
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, name) == 0) {
            return -2;
        }
    }

    // 添加命令
    strncpy(g_commands[g_command_count].name, name, 31);
    g_commands[g_command_count].name[31] = '\0';

    if (description != NULL) {
        strncpy(g_commands[g_command_count].description, description, 63);
        g_commands[g_command_count].description[63] = '\0';
    }

    g_commands[g_command_count].handler = handler;
    g_command_count++;

    return 0;
}
```

---

## 内置命令

### help 命令

help 命令列出所有已注册的命令及其描述。它遍历命令表，调用后端的 puts 函数输出信息。注意这里我们需要访问当前 Shell 的上下文来获取后端指针。

### clear 命令

clear 命令调用后端的 clear 函数来清屏。由于 clear 是可选的，我们在调用前需要检查函数指针是否为 NULL。

### exit 命令

exit 命令通过返回 1 来告诉主循环退出 Shell。这是一个简单的约定，处理函数返回 1 表示退出，返回 0 表示继续运行。

---

## 接下来

后端抽象架构已经设计完成，我们定义了清晰的接口，实现了 VGA 和 Serial 两个后端，设计了命令注册机制。下一节，我们会完善 VGA 后端的软件光标、Serial 后端的回显控制等细节，然后实现完整的 Shell 主循环。

→ [下一篇：VGA与Serial Shell后端实现](./13_VGA与Serial_Shell后端实现.md)

---

<div align="center">

## 文档导航

[← 串口中断处理实现与优化](./11_串口中断处理实现与优化.md) | [VGA与Serial Shell后端实现 →](./13_VGA与Serial_Shell后端实现.md)

</div>
