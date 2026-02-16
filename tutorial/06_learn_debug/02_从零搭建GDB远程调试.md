# 从零搭建 GDB 远程调试

现在我们开始动手。这篇文章的目标是让你能够用 GDB 调试我们的内核。

---

## 第一步：CMake Debug 配置

### 我们要做什么

GDB 需要调试符号才能进行源码级调试。这些符号信息包含：
- 函数和变量的内存地址
- 源代码行号与指令的对应关系
- 类型信息

默认情况下，我们的构建系统生成的是 Release 版本，编译器会优化掉很多信息，也不包含调试符号。我们需要告诉 CMake 生成 Debug 版本。

### 修改 CMakeLists.txt

找到项目根目录的 `CMakeLists.txt`，找到构建类型相关的配置：

```cmake
# 设置默认构建类型为 Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# 根据 CMAKE_BUILD_TYPE 设置编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
else()
    add_compile_options(-O2 -DNDEBUG)
endif()
```

如果你看到类似的配置，说明已经有 Debug 支持了。让我们验证一下：

### 验证配置

```bash
# 清理旧的构建
rm -rf build

# 配置为 Debug 模式
cmake -DCMAKE_BUILD_TYPE=Debug -B build

# 编译
cmake --build build
```

你应该看到类似这样的输出：

```
-- Configuring done
-- Generating done
-- Build files have been written to: /path/to/build
[ 10%] Building ASM object boot/CMakeFiles/bootloader.dir/bootloader.asm.o
...
[100%] Built target boot_image
```

### 检查调试符号

```bash
# 查看生成的文件
ls -lh build/kernel.elf build/kernel.bin

# 查看 kernel.elf 的信息
file build/kernel.elf
```

你应该看到：

```
build/kernel.elf: ELF 64-bit LSB executable, x86-64, version 1 (SYSV), with debug_info
```

注意那个 **with debug_info** —— 这说明我们的 ELF 文件包含了调试符号。

⚠️ **注意**：`kernel.bin` 是纯二进制文件，没有符号信息，它是用来启动的。`kernel.elf` 才是用来调试的。

---

## 第二步：创建 debug.sh 脚本

每次调试都要手动启动 QEMU、记住参数、再启动 GDB —— 这太麻烦了。我们来写一个脚本一键搞定。

### 目标

这个脚本要做什么：
1. 检查必要的文件是否存在
2. 启动 QEMU 调试模式（后台运行）
3. 启动 GDB 并加载配置
4. 退出时自动清理 QEMU 进程

### 创建脚本

在 `scripts/` 目录下创建 `debug.sh`：

```bash
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
```

### 赋予执行权限

```bash
chmod +x scripts/debug.sh
```

### 解释关键参数

QEMU 的两个参数非常重要：

| 参数 | 说明 |
|------|------|
| `-s` | 在 `localhost:1234` 启动 GDB 服务器（等价于 `-gdb tcp::1234`） |
| `-S` | 启动时暂停 CPU，等待 GDB 连接 |

如果没有 `-S`，QEMU 会直接开始运行，你可能来不及设置断点。

---

## 第三步：配置 .gdbinit

`.gdbinit` 是 GDB 的初始化配置文件，GDB 启动时会自动执行里面的命令。

### 创建配置文件

在 `scripts/` 目录下创建 `.gdbinit`：

```
# CCOS GDB 初始化配置文件
# 使用方法: gdb -x .gdbinit kernel.elf

target remote :1234
set architecture i386:x86-64:intel
set print pretty on
set print array on
set print elements 0
# 使用 Intel 语法显示汇编（更易读）
set disassembly-flavor intel

# ==================== 调试模式开关 ====================
# 取消下面那行的注释，就会在 kernel_main 自动停下来
# break kernel_main
# echo \n已在 kernel_main 设置断点\n
# =====================================================

echo "Load Symbol file...\n"
symbol-file build/kernel.elf

# 常用调试命令
# ================
# c/continue      - 继续执行
# si/stepi        - 单步执行（汇编指令级）
# ni/nexti        - 单步执行（不进入函数调用）
# s/step          - 单步执行（源代码级）
# n/next          - 单步执行（不进入函数）
# info registers  - 显示寄存器状态
# info registers rip - 显示特定寄存器
# x/10i $pc       - 显示当前指令及后续 9 条
# x/10x 0x address - 以十六进制显示内存
# backtrace/bt    - 显示调用栈
# info breakpoints - 显示所有断点
# delete breakpoints n - 删除断点 n
# print variable   - 打印变量值
# disassemble      - 反汇编当前函数

display/i $pc

# 欢迎信息
echo \n
echo =======================================\n
echo CCOS Kernel GDB 调试环境已就绪\n
echo =======================================\n
echo 输入 'c' 开始执行，或 'si' 单步调试\n
echo 输入 'help' 查看更多命令\n
echo =======================================\n
echo \n
```

### 解释配置项

| 配置 | 说明 |
|------|------|
| `target remote :1234` | 连接到 QEMU 的 GDB 服务器 |
| `set architecture i386:x86-64:intel` | 设置架构为 x86-64，使用 Intel 语法 |
| `set print pretty on` | 漂亮打印结构体 |
| `symbol-file build/kernel.elf` | 加载调试符号 |
| `display/i $pc` | 每次停顿后显示当前指令 |

⚠️ **注意**：`symbol-file` 的路径是相对于项目根目录的，因为我们会在脚本里 `cd` 到项目根目录。

---

## 第四步：第一次调试体验

现在让我们来试试调试环境是否正常工作。

### 启动调试

```bash
./scripts/debug.sh
```

你应该看到这样的输出：

```
========================================
CCOS Kernel 调试环境启动脚本
========================================

正在启动 QEMU 调试模式...
QEMU 将在端口 1234 等待 GDB 连接

QEMU 参数:
  -drive format=raw,file=build/boot.img,if=ide
  -nographic
  -s (GDB server on :1234)
  -S (暂停启动)

QEMU 已启动 (PID: 12345)

等待 1 秒让 QEMU 完全启动...

========================================
正在启动 GDB...
========================================

...
Load Symbol file...

Reading symbols from build/kernel.elf...
```

然后你会看到 GDB 的提示符：

```
(gdb)
```

### 设置断点并运行

现在让我们在内核入口点设置断点：

```
(gdb) b *0x10000
Breakpoint 1 at 0x10000
```

然后继续执行：

```
(gdb) c
Continuing.

Breakpoint 1, 0x0000000000010000 in ?? ()
```

太棒了！我们成功在内核入口点停下来了。

### 查看当前指令

```
(gdb) x/10i $pc
=> 0x10000:     mov     eax, 0x11111111
   0x10005:     mov     qword ptr [rsp - 8], rbp
   0x1000a:     mov     rbp, rsp
   ...
```

### 查看寄存器

```
(gdb) info registers
rax            0x11111111   286331153
rbx            0x0          0
rcx            0x0          0
...
```

### 单步执行

```
(gdb) si
0x0000000000010005 in ?? ()
```

每执行一次 `si`，CPU 就会执行一条指令。

### 退出调试

```
(gdb) quit

正在关闭 QEMU...
调试会话结束
```

脚本会自动清理 QEMU 进程，你不需要手动 kill。

---

## 常见问题

### QEMU 启动失败

如果你看到：

```
错误: 未安装 qemu-system-x86_64
```

安装 QEMU：

```bash
# Ubuntu/Debian
sudo apt install qemu-system-x86

# Fedora/RHEL
sudo dnf install qemu-system-x86

# Arch Linux
sudo pacman -S qemu-system-x86
```

### GDB 找不到符号

如果你看到：

```
(no debugging symbols found)
```

检查你是否使用 Debug 模式编译：

```bash
# 重新配置和编译
rm -rf build
cmake -DCMAKE_BUILD_TYPE=Debug -B build
cmake --build build

# 验证符号文件
file build/kernel.elf
```

应该看到 `with debug_info`。

### 端口被占用

如果你看到：

```
Remote communication error. Target disconnected.: Connection refused
```

可能是 1234 端口被占用了：

```bash
# 查看端口占用
lsof -ti:1234

# 停止占用进程
kill -9 $(lsof -ti:1234)
```

---

## 总结

到这里，我们已经成功搭建了基础的 GDB 调试环境：

- ✅ CMake Debug 配置完成
- ✅ debug.sh 脚本可以一键启动调试
- ✅ .gdbinit 配置自动连接和设置
- ✅ 能够在内核入口点设置断点并单步执行

但这只是开始。在下一篇文章中，我们会实现 VGA 文本模式驱动，这样调试时就能看到可视化的输出，而不是一片漆黑。

→ [下一篇：VGA 文本模式驱动实战](./03_VGA文本模式驱动实战.md)


---

<div align="center">

## 文档导航

[← 为什么我们需要调试设施](01_为什么我们需要调试设施.md)  | [VGA文本模式驱动实战 →](03_VGA文本模式驱动实战.md)

</div>
