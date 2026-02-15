#!/bin/bash
# CCOS Debug Script
# 用于启动 QEMU 调试模式和 GDB 的便捷脚本

set -e

# 获取脚本所在目录的父目录（项目根目录）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

BUILD_DIR="$PROJECT_ROOT/build"
KERNEL_ELF="$BUILD_DIR/kernel.elf"
BOOT_IMG="$BUILD_DIR/boot.img"
GDBINIT="$SCRIPT_DIR/.gdbinit"

# 颜色输出
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}CCOS Kernel 调试环境启动脚本${NC}"
echo -e "${GREEN}========================================${NC}"
echo

# 检查构建文件是否存在
if [ ! -f "$KERNEL_ELF" ]; then
    echo -e "${YELLOW}警告: $KERNEL_ELF 不存在${NC}"
    echo -e "${YELLOW}请先构建项目: cmake -DCMAKE_BUILD_TYPE=Debug -B build && cmake --build build${NC}"
    exit 1
fi

if [ ! -f "$BOOT_IMG" ]; then
    echo -e "${YELLOW}警告: $BOOT_IMG 不存在${NC}"
    exit 1
fi

# 检查是否已安装必要的工具
if ! command -v qemu-system-x86_64 &> /dev/null; then
    echo -e "${YELLOW}错误: 未安装 qemu-system-x86_64${NC}"
    exit 1
fi

if ! command -v gdb &> /dev/null; then
    echo -e "${YELLOW}错误: 未安装 gdb${NC}"
    exit 1
fi

echo -e "${BLUE}正在启动 QEMU 调试模式...${NC}"
echo -e "${BLUE}QEMU 将在端口 1234 等待 GDB 连接${NC}"
echo
echo -e "${YELLOW}QEMU 参数:${NC}"
echo "  -drive format=raw,file=$BOOT_IMG,if=ide"
echo "  -nographic"
echo "  -s (GDB server on :1234)"
echo "  -S (暂停启动)"
echo

# 启动 QEMU（在后台运行）
qemu-system-x86_64 \
    -drive format=raw,file="$BOOT_IMG",if=ide \
    -nographic \
    -s \
    -S &

QEMU_PID=$!
echo -e "${GREEN}QEMU 已启动 (PID: $QEMU_PID)${NC}"
echo
echo -e "${BLUE}等待 1 秒让 QEMU 完全启动...${NC}"
sleep 1

echo
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}正在启动 GDB...${NC}"
echo -e "${GREEN}========================================${NC}"
echo
echo -e "${YELLOW}GDB 调试命令:${NC}"
echo "  target remote :1234    - 连接到 QEMU"
echo "  b kernel_main         - 在 kernel_main 设置断点"
echo "  c                     - 继续执行"
echo "  si                    - 单步执行（指令级）"
echo "  info registers        - 查看寄存器"
echo "  x/10i \$pc             - 查看当前指令"
echo "  quit                  - 退出 GDB（会自动关闭 QEMU）"
echo

# 启动 GDB 并加载 .gdbinit
cd "$PROJECT_ROOT"  # 切换到项目根目录
gdb -x "$GDBINIT" "$KERNEL_ELF"

# GDB 退出后，关闭 QEMU
echo
echo -e "${BLUE}正在关闭 QEMU...${NC}"
kill $QEMU_PID 2>/dev/null || true
wait $QEMU_PID 2>/dev/null || true

echo -e "${GREEN}调试会话结束${NC}"
