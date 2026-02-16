# VSCode 图形化调试配置

命令行调试很强大，但图形化界面更直观。这篇文章我们来配置 VSCode 调试。

---

## 第一步：创建 launch.json

### 目标

让 VSCode 能够通过 F5 键启动调试，支持：
- 断点设置
- 变量监视
- 单步执行
- 调用栈查看

### 创建 .vscode 目录

```bash
# 在项目根目录执行
mkdir -p .vscode
```

### 创建 launch.json

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
                    "text": "-gdb-set architecture i386:x86-64",
                    "description": "设置 x86-64 架构"
                },
                {
                    "text": "-gdb-set disassembly-flavor intel",
                    "description": "使用 Intel 汇编语法"
                },
                {
                    "text": "-gdb-set pagination off",
                    "description": "关闭分页"
                }
            ]
        }
    ]
}
```

### 解释配置项

| 配置项 | 说明 |
|--------|------|
| `name` | 在调试面板显示的名称 |
| `type` | 调试类型，`cppdbg` 表示 C/C++ |
| `request` | `launch` 表示启动调试 |
| `program` | 要调试的程序（带符号的 ELF 文件） |
| `miDebuggerPath` | GDB 的路径 |
| `miDebuggerServerAddress` | GDB 服务器地址（QEMU 监听的端口） |
| `setupCommands` | GDB 启动时执行的命令 |

⚠️ **注意**：`program` 指向 `kernel.elf` 而不是 `kernel.bin`，因为我们需要符号信息。

---

## 第二步：launch_qemu_for_vscode_debug.sh

VSCode 的调试配置需要 QEMU 已经在运行，并且监听 1234 端口。我们来写一个更智能的脚本：

### 脚本功能

这个脚本要比之前的 `debug.sh` 更智能：

1. **自动清理并重新构建**（Debug 模式）
2. **监控 GDB 连接状态**
3. **GDB 断开时自动停止 QEMU**
4. **支持 VNC 显示**
5. **串口输出到文件供监控**

### 创建脚本

```bash
#!/bin/bash
# CCOS QEMU Launch Script for VSCode Debug
# 用于启动 QEMU 调试服务器供 VSCode attach 调试
# 当 GDB 断开连接时自动停止 QEMU

set -e

# 获取脚本所在目录的父目录（项目根目录）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

BUILD_DIR="$PROJECT_ROOT/build"
KERNEL_ELF="$BUILD_DIR/kernel.elf"
BOOT_IMG="$BUILD_DIR/boot.img"
PID_FILE="$SCRIPT_DIR/.qemu_debug.pid"
SERIAL_LOG="$SCRIPT_DIR/.qemu_serial.log"

# 颜色输出
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
GRAY='\033[0;90m'
NC='\033[0m' # No Color

# 显示帮助信息
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

# 检查构建文件是否存在
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

# 检查是否已安装必要的工具
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

# 停止 QEMU 进程
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

    # 尝试查找并停止任何监听 1234 端口的 QEMU 进程
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

    if [ "$stopped" = true ]; then
        echo -e "${GREEN}清理完成${NC}"
    else
        echo -e "${YELLOW}没有发现运行中的 QEMU 调试服务器${NC}"
    fi
}

# 显示 QEMU 状态
show_status() {
    local running=false

    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo -e "${GREEN}QEMU 正在运行 (PID: $PID)${NC}"
            echo -e "${BLUE}监听端口: localhost:1234${NC}"
            echo -e "${BLUE}符号文件: $KERNEL_ELF${NC}"
            running=true
        else
            echo -e "${YELLOW}PID 文件存在但进程不在运行，清理中...${NC}"
            rm -f "$PID_FILE"
        fi
    fi

    if [ "$running" = false ]; then
        # 检查端口 1234 是否被占用
        PORT_PID=$(lsof -ti:1234 2>/dev/null || true)
        if [ -n "$PORT_PID" ]; then
            if ps -p "$PORT_PID" -o comm= 2>/dev/null | grep -q qemu-system; then
                echo -e "${GREEN}QEMU 正在运行 (PID: $PORT_PID)${NC}"
                echo -e "${YELLOW}注意: 未由本脚本管理${NC}"
            else
                echo -e "${YELLOW}端口 1234 被其他进程占用 (PID: $PORT_PID)${NC}"
            fi
        else
            echo -e "${GRAY}QEMU 调试服务器未运行${NC}"
        fi
    fi
}

# 监控 GDB 连接状态，当 GDB 断开时自动停止 QEMU
monitor_gdb_connection() {
    local qemu_pid=$1

    echo -e "${GRAY}[监控] 正在监控 GDB 连接状态...${NC}"

    # 等待 QEMU 完全启动
    sleep 0.5

    local gdb_connected=false

    while true; do
        # 检查 QEMU 是否还在运行
        if ! ps -p "$qemu_pid" > /dev/null 2>&1; then
            echo -e "\n${GRAY}[监控] QEMU 已停止${NC}"
            rm -f "$PID_FILE"
            break
        fi

        # 检查端口 1234 是否有 ESTABLISHED 连接
        local has_connection=false

        if command -v ss &> /dev/null; then
            if ss -tnH state established '( sport = 1234 or dport = 1234 )' 2>/dev/null | grep -q .; then
                has_connection=true
            fi
        else
            # 回退到 lsof
            if lsof -ti:1234 -sTCP:ESTABLISHED 2>/dev/null | grep -q .; then
                has_connection=true
            fi
        fi

        if $has_connection; then
            if [ "$gdb_connected" = false ]; then
                gdb_connected=true
                echo -e "\n${GREEN}[监控] GDB 已连接${NC}"
            fi
        else
            if [ "$gdb_connected" = true ]; then
                # GDB 断开了
                sleep 2  # 等待 2 秒确认是真的断开了
                if ! lsof -ti:1234 -sTCP:ESTABLISHED 2>/dev/null | grep -q .; then
                    echo -e "\n${YELLOW}[监控] 检测到 GDB 已断开连接${NC}"
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

        sleep 0.5
    done
}

# 启动 QEMU
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

    # 检查文件和工具
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

    # 清理可能的孤立进程
    local orphan_pid=$(lsof -ti:1234 2>/dev/null || true)
    if [ -n "$orphan_pid" ]; then
        if ps -p "$orphan_pid" -o comm= 2>/dev/null | grep -q qemu-system; then
            echo -e "${YELLOW}清理孤立的 QEMU 进程 (PID: $orphan_pid)...${NC}"
            kill "$orphan_pid" 2>/dev/null || true
            sleep 0.5
        fi
    fi

    echo -e "${BLUE}正在启动 QEMU 调试模式...${NC}"

    # 启动 QEMU（在后台运行）
    qemu-system-x86_64 \
        -drive format=raw,file="$BOOT_IMG",if=ide \
        -vga std -display vnc=:0 \
        -serial file:"$SERIAL_LOG" \
        -s \
        -S \
        > /dev/null 2>&1 &

    QEMU_PID=$!

    # 保存 PID
    echo "$QEMU_PID" > "$PID_FILE"

    echo -e "${GREEN}QEMU 已启动 (PID: $QEMU_PID)${NC}"
    echo -e "${BLUE}等待 QEMU 完全启动...${NC}"
    sleep 1

    # 验证 QEMU 是否还在运行
    if ! ps -p "$QEMU_PID" > /dev/null 2>&1; then
        echo -e "${RED}错误: QEMU 启动失败${NC}"
        rm -f "$PID_FILE"
        exit 1
    fi

    # 验证端口是否在监听
    if ! lsof -ti:1234 -sTCP:LISTEN > /dev/null 2>&1; then
        echo -e "${RED}错误: QEMU 未监听端口 1234${NC}"
        stop_qemu
        exit 1
    fi

    echo
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}QEMU GDB Server 已就绪${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "${BLUE}监听端口: ${YELLOW}localhost:1234${NC}"
    echo -e "${BLUE}符号文件: ${YELLOW}$KERNEL_ELF${NC}"
    echo
    echo -e "${GREEN}现在可以在 VSCode 中启动调试了！${NC}"
    echo -e "${YELLOW}停止调试时 QEMU 会自动停止${NC}"
    echo
    echo -e "${GRAY}按 Ctrl+C 可手动停止监控（QEMU 会继续运行）${NC}"
    echo -e "${GRAY}如需完全停止，运行: $0 --stop${NC}"
    echo

    # 启动监控（后台运行）
    monitor_gdb_connection "$QEMU_PID" &
    MONITOR_PID=$!

    # 等待监控进程或用户中断
    wait $MONITOR_PID 2>/dev/null || true
}

# 处理命令行参数
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

### 赋予执行权限

```bash
chmod +x scripts/launch_qemu_for_vscode_debug.sh
```

### 脚本创新点

这个脚本最厉害的地方是 **GDB 断开检测**：

1. 使用 `lsof` 或 `ss` 检查端口 1234 的 TCP 连接状态
2. 当检测到 GDB 断开后，自动停止 QEMU
3. 这样你在 VSCode 中按 Shift+F5 停止调试时，QEMU 也会自动清理

---

## 第三步：配置 clangd

### 为什么需要 clangd

VSCode 默认的 C/C++ 扩展（cpptools）对 freestanding 环境支持不太好。clangd 是更好的选择：
- 更好的代码补全
- 更准确的跳转
- 静态分析

### 创建 .clangd

```yaml
# ==============================================================================
# clangd 配置文件 - CCOS x64 Freestanding Kernel
# ==============================================================================
#

CompileFlags:
  # 添加/覆盖编译标志
  Add:
    # 目标架构设置
    - -m64
    - -march=x86-64
    - -mtune=generic
    - -DNDEBUG=1

    # 警告选项
    - -Wall
    - -Wextra
    - -Wpedantic

  # 移除可能与 freestanding 冲突的标志
  Remove:
    - -msse
    - -msse2

# ==============================================================================
# 诊断选项
# ==============================================================================
Diagnostics:
  # 未使用的头文件警告
  UnusedIncludes: Strict

  # ClangTidy 检查配置
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
      # 禁用不适合 freestanding 的检查
      - modernize-use-auto
      - modernize-avoid-c-arrays
      - cppcoreguidelines-avoid-c-arrays
      - cppcoreguidelines-pro-bounds-array-to-pointer-decay
      - cppcoreguidelines-pro-type-vararg
      - google-*
      - fuchsia-*
      - llvm-*

# ==============================================================================
# 索引选项
# ==============================================================================
Index:
  # 后台索引
  Background: Build

  # 不索引标准库（freestanding 环境）
  StandardLibrary: No

# ==============================================================================
# 补全选项
# ==============================================================================
Completion:
  # 所有作用域的补全
  AllScopes: Yes

# ==============================================================================
# 悬浮信息
# ==============================================================================
Hover:
  # 显示 AKA (All Known) 信息
  ShowAKA: Yes

# ==============================================================================
# Inlay Hints（内联提示）
# ==============================================================================
InlayHints:
  Enabled: Yes
  ParameterNames: Yes
  DeducedTypes: Yes
  Designators: Yes
  BlockEnd: Yes
```

### 安装 clangd 扩展

在 VSCode 中安装：
1. **llvm-vs-code-extensions.vscode-clangd** - clangd 语言服务器

⚠️ **注意**：安装 clangd 后，建议禁用 cpptools 的 IntelliSense：

```json
// .vscode/settings.json
{
    "C_Cpp.intelliSenseEngine": "disabled"
}
```

---

## 第四步：验证 VSCode 调试

### 启动 QEMU

在一个终端运行：

```bash
./scripts/launch_qemu_for_vscode_debug.sh
```

你应该看到：

```
========================================
CCOS QEMU 调试服务器启动脚本
========================================

为确保调试的是最新的文件，正在清理build目录:build中
清理完成，使用CMake重新构建中...
...
CMake构建完成！

正在启动 QEMU 调试模式...
QEMU 已启动 (PID: 12345)

========================================
QEMU GDB Server 已就绪
========================================
监听端口: localhost:1234
符号文件: /path/to/build/kernel.elf

现在可以在 VSCode 中启动调试了！
停止调试时 QEMU 会自动停止

按 Ctrl+C 可手动停止监控（QEMU 会继续运行）
```

### 在 VSCode 中启动调试

1. 打开 `kernel_main.c`
2. 在你想要停下来的地方按 **F9** 设置断点
3. 按 **F5** 启动调试

你应该看到：
- 程序在断点处停下来
- 左边显示变量和监视面板
- 顶部显示调试工具栏
- 终端显示 GDB 输出

### 调试操作

| 操作 | 快捷键 | 说明 |
|------|--------|------|
| 继续执行 | F5 | 继续执行到下一个断点 |
| 单步跳过 | F10 | 下一行（不进入函数） |
| 单步进入 | F11 | 单步（进入函数） |
| 单步跳出 | Shift+F11 | 跳出当前函数 |
| 重启调试 | Ctrl+Shift+F5 | 重启调试 |
| 停止调试 | Shift+F5 | 停止调试 |

### 验证自动停止

1. 在调试过程中，按 **Shift+F5** 停止调试
2. 观察启动 QEMU 的终端

你应该看到：

```
[监控] 检测到 GDB 已断开连接
[监控] 正在自动停止 QEMU...
[监控] QEMU 已自动停止
```

脚本会自动清理 QEMU 进程，你不需要手动 kill。

---

## 常见问题

### VSCode 找不到 cpptools

安装 **ms-vscode.cpptools** 扩展。

### clangd 报告找不到头文件

检查 `compile_commands.json` 是否存在：

```bash
ls build/compile_commands.json
```

如果不存在，在 `CMakeLists.txt` 中添加：

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

### GDB 连接失败

确保 QEMU 已经在运行：

```bash
./scripts/launch_qemu_for_vscode_debug.sh --status
```

### 符号文件不匹配

重新构建项目：

```bash
./scripts/launch_qemu_for_vscode_debug.sh
```

脚本会自动清理并重新构建。

---

## 总结

现在我们有了完整的 VSCode 调体验：

- ✅ launch.json 配置完成
- ✅ launch_qemu_for_vscode_debug.sh 自动管理 QEMU 生命周期
- ✅ clangd 配置完成
- ✅ 可以在 VSCode 中图形化调试

体验比命令行好了很多。最后一篇文章我们会总结整个调试流程，并介绍一些高级技巧。

→ [下一篇：完整调试验证与总结](./05_完整调试验证与总结.md)


---

<div align="center">

## 文档导航

[← VGA文本模式驱动实战](03_VGA文本模式驱动实战.md)  | [完整调试验证与总结 →](05_完整调试验证与总结.md)

</div>
