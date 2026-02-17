# VSCode 图形化调试配置

命令行调试很强大，但图形化界面更直观。这篇文章我们来配置 VSCode 调试。

---

## 第一步：创建 launch.json

### 我们要做什么

让 VSCode 能够通过 F5 键启动调试，支持断点设置、变量监视、单步执行、调用栈查看。这些功能在现代 IDE 里都是天经地义的，但在内核调试环境里，我们需要手动配置。

### 创建 .vscode 目录

```bash
mkdir -p .vscode
```

### 创建 launch.json

在 `.vscode` 目录下创建 `launch.json`：

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Attach QEMU GDB",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/kernel.elf",
            "cwd": "${workspaceFolder}",
            "MIMode": "gdb",
            "miDebuggerPath": "/usr/bin/gdb",
            "miDebuggerServerAddress": "localhost:1234",
            "setupCommands": [
                {
                    "text": "-gdb-set architecture i386:x86-64"
                },
                {
                    "text": "-gdb-set disassembly-flavor intel"
                },
                {
                    "text": "-gdb-set pagination off"
                }
            ]
        }
    ]
}
```

这个配置文件定义了 VSCode 如何连接到 GDB。让我们拆开来看每个字段的含义。

`type: "cppdbg"` 表示这是 C/C++ 调试类型，会使用 cpptools 扩展。`request: "launch"` 表示启动调试（而不是 attach 到已有进程）。`program` 指向带符号的 ELF 文件，这是关键，我们调试的是 `kernel.elf` 而不是 `kernel.bin`。

`miDebuggerPath` 是 GDB 的路径，`miDebuggerServerAddress` 告诉 VSCode QEMU 的 GDB 服务器监听在 localhost:1234。

`setupCommands` 是 GDB 启动时执行的命令。我们设置架构为 x86-64，使用 Intel 汇编语法（比 AT&T 语法更易读），关闭分页输出（让 GDB 输出不被分页打断）。

---

## 第二步：智能调试脚本

### 设计目标

VSCode 的调试配置需要 QEMU 已经在运行。但每次都手动启动 QEMU 很麻烦，而且调试结束后还得手动 kill 进程。我们想要一个更智能的脚本：

1. 自动清理并重新构建（Debug 模式）
2. 监控 GDB 连接状态
3. GDB 断开时自动停止 QEMU
4. 支持 VNC 显示
5. 串口输出到文件供监控

这个脚本会比之前的 `debug.sh` 更强大。

### 脚本头部和帮助信息

```bash
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

BUILD_DIR="$PROJECT_ROOT/build"
KERNEL_ELF="$BUILD_DIR/kernel.elf"
BOOT_IMG="$BUILD_DIR/boot.img"
PID_FILE="$SCRIPT_DIR/.qemu_debug.pid"
SERIAL_LOG="$SCRIPT_DIR/.qemu_serial.log"
```

这些路径变量的作用和 `debug.sh` 类似，但多了 `PID_FILE` 和 `SERIAL_LOG`。`PID_FILE` 保存 QEMU 进程的 PID，方便后续清理。`SERIAL_LOG` 是串口输出文件，我们可以用 `tail -f` 实时查看。

帮助函数让脚本支持多种用法：

```bash
show_help() {
    echo "CCOS QEMU 调试服务器启动脚本"
    echo ""
    echo "用法:"
    echo "  $0                          启动 QEMU（自动监控 GDB 连接）"
    echo "  $0 --stop                   停止 QEMU 调试服务器"
    echo "  $0 --status                 查看 QEMU 状态"
    echo "  $0 --help                   显示此帮助信息"
    echo ""
    echo "VSCode 调试流程:"
    echo "  1. 运行 '$0' 启动 QEMU"
    echo "  2. 在 VSCode 中按 F5 启动调试"
    echo "  3. 停止 VSCode 调试时，QEMU 会自动停止"
}
```

### 检查函数

我们有两个检查函数，一个检查文件是否存在，一个检查工具是否安装。

```bash
check_files() {
    if [ ! -f "$KERNEL_ELF" ]; then
        echo -e "${RED}错误: $KERNEL_ELF 不存在${NC}"
        echo -e "${YELLOW}请先构建项目: cmake -DCMAKE_BUILD_TYPE=Debug -B build && cmake --build build${NC}"
        exit 1
    fi

    if [ ! -f "$BOOT_IMG" ]; then
        echo -e "${RED}错误: $BOOT_IMG 不存在${NC}"
        exit 1
    fi
}

check_tools() {
    if ! command -v qemu-system-x86_64 &> /dev/null; then
        echo -e "${RED}错误: 未安装 qemu-system-x86_64${NC}"
        exit 1
    fi

    if ! command -v lsof &> /dev/null; then
        echo -e "${RED}错误: 未安装 lsof${NC}"
        exit 1
    fi
}
```

`lsof` 是用来查看端口占用的工具，我们的 GDB 连接检测需要它。如果系统没有 `lsof`，脚本会报错退出。

### 停止 QEMU 函数

这个函数负责清理 QEMU 进程：

```bash
stop_qemu() {
    local stopped=false

    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo -e "${BLUE}正在停止 QEMU (PID: $PID)...${NC}"
            kill "$PID" 2>/dev/null || true
            # 等待进程结束
            local count=0
            while ps -p "$PID" > /dev/null 2>&1 && [ $count -lt 20 ]; do
                sleep 0.25
                count=$((count + 1))
            done
            # 如果还在运行，强制杀死
            if ps -p "$PID" > /dev/null 2>&1; then
                kill -9 "$PID" 2>/dev/null || true
                sleep 0.5
            fi
            echo -e "${GREEN}QEMU 已停止${NC}"
            stopped=true
        else
            echo -e "${YELLOW}QEMU 进程 (PID: $PID) 不存在${NC}"
        fi
        rm -f "$PID_FILE"
    fi
```

首先读取 PID 文件，检查进程是否还在运行。如果在运行，先发送 TERM 信号（`kill` 默认发送 TERM），然后等待进程结束。如果 5 秒后还在运行，就用 KILL 信号（`kill -9`）强制杀死。

这个"先礼后兵"的策略是个好习惯。TERM 信号给进程一个清理资源的机会，KILL 信号是立即终止。

然后检查端口 1234 是否有其他 QEMU 进程：

```bash
    QEMU_PID=$(lsof -ti:1234 2>/dev/null || true)
    if [ -n "$QEMU_PID" ]; then
        if ps -p "$QEMU_PID" -o comm= 2>/dev/null | grep -q qemu-system; then
            echo -e "${BLUE}停止端口 1234 上的 QEMU (PID: $QEMU_PID)...${NC}"
            kill "$QEMU_PID" 2>/dev/null || true
            sleep 0.5
            if ps -p "$QEMU_PID" > /dev/null 2>&1; then
                kill -9 "$QEMU_PID" 2>/dev/null || true
            fi
            stopped=true
        fi
    fi
```

`lsof -ti:1234` 返回占用 1234 端口的进程 PID。我们检查一下这确实是 QEMU 进程，然后停止它。这样可以清理那些没有 PID 文件的"孤儿" QEMU 进程。

### GDB 连接监控

这是脚本的核心创新点：

```bash
monitor_gdb_connection() {
    local qemu_pid=$1
    local tail_pid=""

    echo -e "${GRAY}[监控] 正在监控 GDB 连接状态...${NC}"

    sleep 0.5

    local gdb_connected=false
    local last_connected_time=0
    local connection_count=0

    > "$SERIAL_LOG"

    while true; do
        # 检查 QEMU 是否还在运行
        if ! ps -p "$qemu_pid" > /dev/null 2>&1; then
            if [ -n "$tail_pid" ]; then
                kill "$tail_pid" 2>/dev/null || true
            fi
            echo -e "\n${GRAY}[监控] QEMU 已停止${NC}"
            rm -f "$PID_FILE"
            break
        fi
```

监控函数是一个无限循环，不断检查 QEMU 和 GDB 的状态。如果 QEMU 停止了，就退出监控。

接下来检查 GDB 连接：

```bash
        local has_connection=false

        if command -v ss &> /dev/null; then
            if ss -tnH state established '( sport = 1234 or dport = 1234 )' 2>/dev/null | grep -q .; then
                has_connection=true
            fi
        else
            if lsof -ti:1234 -sTCP:ESTABLISHED 2>/dev/null | grep -q .; then
                has_connection=true
            fi
        fi
```

我们使用 `ss` 或 `lsof` 检查端口 1234 是否有 ESTABLISHED 状态的连接。`ss` 是现代 Linux 系统的标准工具，输出更简洁。如果 `ss` 不可用，回退到 `lsof`。

连接状态检测逻辑：

```bash
        local current_time=$(date +%s)

        if $has_connection; then
            if [ "$gdb_connected" = false ]; then
                gdb_connected=true
                connection_count=$((connection_count + 1))
                echo -e "\n${GREEN}[监控] GDB 已连接 (${connection_count})${NC}"
                tail -f "$SERIAL_LOG" 2>/dev/null &
                tail_pid=$!
            fi
            last_connected_time=$current_time
        else
            if [ "$gdb_connected" = true ]; then
                if [ $((current_time - last_connected_time)) -ge 2 ]; then
                    if [ -n "$tail_pid" ]; then
                        kill "$tail_pid" 2>/dev/null || true
                        tail_pid=""
                    fi
                    echo -e "\n${YELLOW}[监控] 检测到 GDB 已断开连接${NC}"

                    sleep 1
                    if ! lsof -ti:1234 -sTCP:ESTABLISHED 2>/dev/null | grep -q .; then
                        echo -e "${YELLOW}[监控] 正在自动停止 QEMU...${NC}"
                        kill "$qemu_pid" 2>/dev/null || true
                        # 等待 QEMU 退出
                        local count=0
                        while ps -p "$qemu_pid" > /dev/null 2>&1 && [ $count -lt 20 ]; do
                            sleep 0.25
                            count=$((count + 1))
                        done
                        if ps -p "$qemu_pid" > /dev/null 2>&1; then
                            kill -9 "$qemu_pid" 2>/dev/null || true
                        fi
                        rm -f "$PID_FILE"
                        echo -e "${GREEN}[监控] QEMU 已自动停止${NC}"
                        break
                    fi
                fi
            fi
        fi

        sleep 0.5
    done
}
```

当检测到 GDB 连接时，我们启动一个 `tail -f` 进程实时显示串口日志。当检测到 GDB 断开时，等待 2 秒确认是真的断开了（而不是正在重连），然后自动停止 QEMU。

这个设计让调试体验非常流畅。你在 VSCode 里按 F5 开始调试，按 Shift+F5 停止调试，QEMU 会自动管理。不用手动 kill 进程，也不用担心忘记关 QEMU。

### 自动构建和启动

```bash
start_qemu() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}CCOS QEMU 调试服务器启动脚本${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo

    echo -e "${GREEN}为确保调试的是最新的文件，正在清理build目录:${BUILD_DIR}中${NC}"
    rm -rf ${BUILD_DIR}
    echo -e "${GREEN}清理完成，使用CMake重新构建中...${NC}"
    cmake -DCMAKE_BUILD_TYPE=Debug -B ${BUILD_DIR} -S ${PROJECT_ROOT} || {
        echo -e "${RED}错误: CMake 配置失败${NC}"
        exit 1
    }

    cmake --build ${BUILD_DIR} || {
        echo -e "${RED}错误: CMake 构建失败${NC}"
        exit 1
    }

    echo -e "${GREEN}CMake构建完成！${NC}"
```

脚本启动时会自动清理并重新构建，确保你调试的是最新代码。这一点真的很重要，我之前就遇到过改了代码但忘记重新构建，然后一直在调试旧代码的尴尬情况。

然后启动 QEMU：

```bash
    check_files
    check_tools

    # 检查是否已经在运行
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo -e "${YELLOW}QEMU 已经在运行 (PID: $PID)${NC}"
            echo -e "${YELLOW}如需重启，请先运行: $0 --stop${NC}"
            exit 0
        else
            rm -f "$PID_FILE"
        fi
    fi

    qemu-system-x86_64 \
        -drive format=raw,file="$BOOT_IMG",if=ide \
        -vga std -display vnc=:0 \
        -serial file:"$SERIAL_LOG" \
        -s \
        -S \
        > /dev/null 2>&1 &
```

QEMU 的参数有点不一样：`-vga std -display vnc=:0` 启用 VGA 输出并通过 VNC 协议显示（可以用 vncviewer 连接）。`-serial file:"$SERIAL_LOG"` 把串口输出重定向到文件。

最后启动监控函数：

```bash
    monitor_gdb_connection "$QEMU_PID" &
    MONITOR_PID=$!

    wait $MONITOR_PID 2>/dev/null || true
}
```

我们把监控函数放在后台运行，然后用 `wait` 等待它结束。这样当 GDB 断开、QEMU 停止后，脚本才会退出。

### 命令行参数处理

```bash
case "${1:-}" in
    --stop|-s)
        stop_qemu
        ;;
    --status|--st)
        show_status
        ;;
    --help|-h)
        show_help
        ;;
    "")
        start_qemu
        ;;
    *)
        echo -e "${RED}未知选项: $1${NC}"
        echo
        show_help
        exit 1
        ;;
esac
```

`"${1:-}"` 是 bash 的参数默认值语法。如果 `$1` 为空，就使用空字符串作为默认值。这样脚本默认执行启动操作，也可以通过 `--stop`、`--status`、`--help` 参数执行其他操作。

---

## 第三步：配置 clangd

### 为什么需要 clangd

VSCode 默认的 C/C++ 扩展（cpptools）对 freestanding 环境支持不太好。它会尝试加载标准库头文件，但我们的内核环境没有标准库。clangd 是更好的选择：代码补全更准确、跳转更可靠、静态分析更强大。

### 创建 .clangd

在项目根目录创建 `.clangd`：

```yaml
CompileFlags:
  Add:
    - -m64
    - -march=x86-64
    - -mtune=generic
    - -DNDEBUG=1
    - -Wall
    - -Wextra
    - -Wpedantic

  Remove:
    - -msse
    - -msse2
```

这里我们添加 x86-64 的编译标志，开启各种警告，然后移除 SSE 相关的标志。内核环境通常不需要 SSE，移除这些标志可以避免 clangd 报错。

```yaml
Diagnostics:
  UnusedIncludes: Strict

  ClangTidy:
    Add:
      - bugprone-*
      - clang-analyzer-*
      - modernize-use-nullptr
      - modernize-use-override
      - performance-*
      - readability-identifier-naming
      - readability-function-cognitive-complexity
      - cert-*

    Remove:
      - modernize-use-auto
      - modernize-avoid-c-arrays
      - cppcoreguidelines-avoid-c-arrays
      - cppcoreguidelines-pro-bounds-array-to-pointer-decay
      - cppcoreguidelines-pro-type-vararg
      - google-*
      - fuchsia-*
      - llvm-*
```

Diagnostics 配置 clangd 的诊断行为。`UnusedIncludes: Strict` 会把未使用的头文件标记为警告。ClangTidy 是静态分析工具，这里我们启用了一些有用的检查，禁用了一些不适合 freestanding 环境的检查。

```yaml
Index:
  Background: Build
  StandardLibrary: No

Completion:
  AllScopes: Yes

Hover:
  ShowAKA: Yes

InlayHints:
  Enabled: Yes
  ParameterNames: Yes
  DeducedTypes: Yes
  Designators: Yes
  BlockEnd: Yes
```

Index 配置告诉 clangd 不要索引标准库（因为我们的环境没有）。Completion 和 Hover 配置让代码补全和悬浮信息更友好。InlayHints 是内联提示，会在代码中显示类型信息。

### 安装 clangd 扩展

在 VSCode 中安装 **llvm-vs-code-extensions.vscode-clangd**。安装后建议禁用 cpptools 的 IntelliSense，在 `.vscode/settings.json` 中添加：

```json
{
    "C_Cpp.intelliSenseEngine": "disabled"
}
```

这样 clangd 就会接管 C/C++ 语言服务，提供更好的开发体验。

---

## 第四步：验证调试功能

### 启动调试

在一个终端运行：

```bash
./scripts/launch_qemu_for_vscode_debug.sh
```

你应该看到 QEMU 自动构建和启动，然后等待 VSCode 连接。

在 VSCode 中打开 `kernel_main.c`，按 F9 设置断点，按 F5 启动调试。程序应该在断点处停下来，左边显示变量面板，顶部显示调试工具栏。

### 调试快捷键

| 快捷键 | 功能 |
|--------|------|
| F5 | 继续执行到下一个断点 |
| F10 | 单步跳过（不进入函数） |
| F11 | 单步进入（进入函数） |
| Shift+F11 | 单步跳出（跳出当前函数） |
| Shift+F5 | 停止调试 |

### 验证自动停止

调试过程中按 Shift+F5 停止调试，观察启动 QEMU 的终端。你应该能看到监控函数检测到 GDB 断开，然后自动停止 QEMU。这个自动化真的很方便。

---

## 总结

我们配置了完整的 VSCode 图形化调试环境：launch.json 让 VSCode 能连接到 GDB，智能脚本自动管理 QEMU 生命周期，clangd 提供更好的代码体验。

现在调试体验比命令行好了很多。最后一篇文章我们会总结整个调试流程，并介绍一些高级技巧。

→ [下一篇：完整调试验证与总结](./05_完整调试验证与总结.md)


---

<div align="center">

## 文档导航

[← VGA文本模式驱动实战](03_VGA文本模式驱动实战.md)  | [完整调试验证与总结 →](05_完整调试验证与总结.md)

</div>
