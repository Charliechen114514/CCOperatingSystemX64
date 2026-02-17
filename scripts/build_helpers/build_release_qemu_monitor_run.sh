#!/bin/bash
set -e
# 颜色输出
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
GRAY='\033[0;90m'
NC='\033[0m' # No Color

echo -e "${GREEN} You are selecting Release build with QEMU Monitor...${NC}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="${PROJECT_ROOT}/build"

echo -e "${GREEN} Where is the Project? ${PROJECT_ROOT} ${NC}"
echo -e "${GREEN} Where is the Build Dir? ${BUILD_DIR} ${NC}"

# 解析额外的 CMake -D 选项
EXTRA_CMAKE_FLAGS=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        -D*)
            EXTRA_CMAKE_FLAGS="$EXTRA_CMAKE_FLAGS $1"
            shift
            ;;
        *)
            echo -e "${RED} Unknown option: $1${NC}"
            echo -e "${YELLOW} Usage: $0 [-D<FLAG>=<VALUE>]...${NC}"
            exit 1
            ;;
    esac
done

# 打印传递的额外 flags
if [ -n "$EXTRA_CMAKE_FLAGS" ]; then
    echo -e "${YELLOW} Extra CMake flags detected:${NC}"
    for flag in $EXTRA_CMAKE_FLAGS; do
        echo -e "${GRAY}  - $flag${NC}"
    done
else
    echo -e "${GRAY} No extra CMake flags provided${NC}"
fi

echo -e "${YELLOW} Cleaning build directory...${NC}"
rm -rf ${BUILD_DIR}

# 构建完整的 CMake 配置命令
CMAKE_CONFIG_CMD="cmake -B ${BUILD_DIR} -S ${PROJECT_ROOT} ${EXTRA_CMAKE_FLAGS}"

echo -e "${BLUE} Configuring CMake with command:${NC}"
echo -e "${GRAY}  $CMAKE_CONFIG_CMD${NC}"
eval $CMAKE_CONFIG_CMD

BUILD_CMD="cmake --build ${BUILD_DIR} -j$(nproc) --target qemu-monitor-run"
echo -e "${BLUE} Building project (Release mode) with command:${NC}"
echo -e "${GRAY}  $BUILD_CMD${NC}"
echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW} QEMU Monitor Controls:${NC}"
echo -e "${YELLOW}   Ctrl+A then C - Switch to monitor${NC}"
echo -e "${YELLOW}   Ctrl+A then X - Quit QEMU${NC}"
echo -e "${YELLOW}========================================${NC}"
eval $BUILD_CMD

echo -e "${GREEN} Build completed successfully!${NC}"
