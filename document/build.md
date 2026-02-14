# CCOS x64 - CMake 构建指南

## 快速开始

```bash
# 配置
cmake -B build
cmake --build build --target build-and-vga-run

cmake --build build
cmake --build build --target vga-run

cmake --build build --target run
cmake --build build --target debug
```

## 构建类型

### Debug 构建（带调试信息，未优化）
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Release 构建（优化，体积更小）
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## 构建产物

构建完成后，`build/` 目录包含：
- `bootloader.bin` - Bootloader二进制 (1007字节)
- `kernel.bin` - 内核二进制
- `boot.img` - 完整启动镜像
- `kernel.elf` - 带符号的内核ELF文件（用于调试）

## 工具要求

- **NASM**: 0x86_64汇编器
- **GCC**: C编译器
- **LD**: GNU链接器
- **QEMU**: x86_64系统模拟器
