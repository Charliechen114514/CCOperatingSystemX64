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
    local last_connected_time=0
    local connection_count=0

    while true; do
        # 检查 QEMU 是否还在运行
        if ! ps -p "$qemu_pid" > /dev/null 2>&1; then
            echo -e "\n${GRAY}[监控] QEMU 已停止${NC}"
            rm -f "$PID_FILE"
            break
        fi

        # 检查端口 1234 是否有连接
        # lsof 输出格式: COMMAND PID USER FD TYPE DEVICE SIZE/OFF NODE NAME
        # 我们需要检查 ESTABLISHED 状态的连接
        local has_connection=false

        # 使用 ss 或 lsof 检查连接
        if command -v ss &> /dev/null; then
            # ss 输出包含 ESTAB 状态表示有连接
            if ss -tnH state established '( sport = 1234 or dport = 1234 )' 2>/dev/null | grep -q .; then
                has_connection=true
            fi
        else
            # 回退到 lsof
            if lsof -ti:1234 -sTCP:ESTABLISHED 2>/dev/null | grep -q .; then
                has_connection=true
            fi
        fi

        local current_time=$(date +%s)

        if $has_connection; then
            if [ "$gdb_connected" = false ]; then
                # 新连接
                gdb_connected=true
                connection_count=$((connection_count + 1))
                echo -e "\n${GREEN}[监控] GDB 已连接 (${connection_count})${NC}"
            fi
            last_connected_time=$current_time
        else
            if [ "$gdb_connected" = true ]; then
                # 连接断开
                if [ $((current_time - last_connected_time)) -ge 2 ]; then
                    # 等待 2 秒确认是真的断开了（不是重连中）
                    echo -e "\n${YELLOW}[监控] 检测到 GDB 已断开连接${NC}"

                    # 再检查一次，确保不是正在重连
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
