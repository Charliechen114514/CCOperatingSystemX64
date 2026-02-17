#!/bin/bash
set -e
# 颜色输出
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
GRAY='\033[0;90m'
NC='\033[0m' # No Color

echo -e "${GREEN} You are selecting Release typical builds...${NC}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="${PROJECT_ROOT}/build"

echo -e "${GREEN} Where is the Project? ${PROJECT_ROOT} ${NC}"
echo -e "${GREEN} Where is the Build Dir? ${BUILD_DIR} ${NC}"

echo -e "${YELLOW} Cleaning build directory...${NC}"
rm -rf ${BUILD_DIR}

echo -e "${BLUE} Configuring CMake...${NC}"
cmake -B ${BUILD_DIR} -S ${PROJECT_ROOT} -DCMAKE_BUILD_TYPE=Release

echo -e "${BLUE} Building project (Release mode)...${NC}"
cmake --build ${BUILD_DIR} -j$(nproc) --target vga-run

echo -e "${GREEN} Build completed successfully!${NC}"
