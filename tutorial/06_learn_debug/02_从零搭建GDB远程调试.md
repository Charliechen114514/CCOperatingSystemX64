# 从零搭建 GDB 远程调试

现在我们开始动手。这篇文章的目标是让你能够用 GDB 调试我们的内核。

---

## 第一步：CMake Debug 配置

### 我们要做什么

GDB 需要调试符号才能进行源码级调试。这些符号信息包含了函数和变量的内存地址、源代码行号与指令的对应关系，以及类型信息。没有调试符号，GDB 只能在汇编层面工作，你看到的是一堆地址和指令，而不是熟悉的函数名和变量名。

默认情况下，构建系统生成的是 Release 版本。编译器会开启各种优化，可能把你的代码改得面目全非，而且不会生成调试符号。这对调试来说是灾难性的。你需要告诉 CMake 生成 Debug 版本。

### 检查当前配置

让我们先看看项目的 CMakeLists.txt 中关于构建类型的配置。打开项目根目录的 `CMakeLists.txt`，找到构建类型相关的部分。

```cmake
# 设置默认构建类型为 Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 根据 CMAKE_BUILD_TYPE 设置编译选项
if(NOT CMAKE_C_FLAGS_DEBUG)
    set(CMAKE_C_FLAGS_DEBUG "-g -O0")
endif()
if(NOT CMAKE_C_FLAGS_RELEASE)
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()
```

这段代码的含义是：如果没有指定构建类型，默认使用 Release。Debug 模式使用 `-g -O0` 标志，Release 模式使用 `-O3 -DNDEBUG`。`-g` 告诉编译器生成调试符号，`-O0` 关闭优化，`-O3` 是最高级别的优化。

### 配置并编译 Debug 版本

现在让我们来配置和编译 Debug 版本。首先清理旧的构建产物，然后重新配置。

```bash
# 清理旧的构建
rm -rf build

# 配置为 Debug 模式
cmake -DCMAKE_BUILD_TYPE=Debug -B build

# 编译
cmake --build build
```

你应该能看到类似这样的输出：

```
-- Found NASM: /usr/bin/nasm
-- Found QEMU: /usr/bin/qemu-system-x86_64
-- Found Python: /usr/bin/python3
-- Build type: Debug
C flags for Debug build: -g -O0
...
[ 10%] Building ASM object boot/CMakeFiles/bootloader.dir/bootloader.asm.o
...
[100%] Built target boot_img
```

注意 `Build type: Debug` 这一行，这确认了我们是按 Debug 模式构建的。同时能看到 `C flags for Debug build: -g -O0`，说明调试符号已经开启，优化已经关闭。

### 检查生成的文件

编译完成后，让我们看看生成的文件是否包含调试符号。

```bash
# 查看生成的文件
ls -lh build/kernel.elf build/kernel.bin

# 查看 kernel.elf 的信息
file build/kernel.elf
```

你应该看到类似这样的输出：

```
-rwxr-xr-x 1 user user 2.0K Feb 17 10:00 build/bootloader.bin
-rwxr-xr-x 1 user user 50K  Feb 17 10:00 build/kernel.elf
-rwxr-xr-x 1 user user 16K  Feb 17 10:00 build/kernel.bin

build/kernel.elf: ELF 64-bit LSB executable, x86-64, version 1 (SYSV), with debug_info
```

注意那个 **with debug_info** —— 这说明我们的 ELF 文件包含了调试符号。

这里有个关键的区别：`kernel.bin` 是纯二进制文件，没有符号信息，它是用来启动的。而 `kernel.elf` 是 ELF 格式文件，包含了调试符号，是用来调试的。我们调试的时候要加载 `kernel.elf`，而不是 `kernel.bin`。

如果你看到 `stripped` 而不是 `with debug_info`，说明调试符号没有正确生成，需要检查 CMake 配置是否正确。

---

## 第二步：创建 debug.sh 脚本

### 目标

每次调试都要手动启动 QEMU、记住一堆参数、再启动 GDB，这太麻烦了。我们来写一个脚本一键搞定整个流程。

这个脚本要做几件事：检查必要的文件是否存在，启动 QEMU 调试模式（在后台运行），启动 GDB 并加载配置，退出时自动清理 QEMU 进程。这样我们就不用每次都手动 kill QEMU 了。

### 脚本结构概览

我们先看看脚本的整体结构，然后逐步实现每个部分。

```bash
#!/bin/bash
# CCOS Debug Script
# 用于启动 QEMU 调试模式和 GDB 的便捷脚本

set -e  # 遇到错误立即退出

# 获取脚本所在目录的父目录（项目根目录）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

BUILD_DIR="$PROJECT_ROOT/build"
KERNEL_ELF="$BUILD_DIR/kernel.elf"
BOOT_IMG="$BUILD_DIR/boot.img"
GDBINIT="$SCRIPT_DIR/.gdbinit"
```

脚本开头先定义各种路径变量。`SCRIPT_DIR` 是脚本所在目录，`PROJECT_ROOT` 是项目根目录。`KERNEL_ELF` 是带符号的内核文件，`BOOT_IMG` 是启动镜像，`GDBINIT` 是 GDB 配置文件。

### 添加颜色输出

为了让脚本的输出更友好，我们定义一些颜色变量。

```bash
# 颜色输出
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color
```

这些 ANSI 颜色码可以让终端输出带上颜色。绿色表示成功，黄色表示警告，蓝色表示信息。`NC` 是 No Color，用来重置颜色。

### 显示欢迎信息

```bash
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}CCOS Kernel 调试环境启动脚本${NC}"
echo -e "${GREEN}========================================${NC}"
echo
```

`-e` 参数让 echo 解析转义字符，这样颜色代码才能生效。

### 检查文件和工具

接下来我们检查必要的文件和工具是否存在。

```bash
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
```

这段代码依次检查 kernel.elf、boot.img、qemu-system-x86_64 和 gdb 是否存在。如果任何一个缺失，脚本会给出提示并退出。

### 启动 QEMU

检查通过后，我们启动 QEMU。

```bash
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
```

这里有几个关键的 QEMU 参数。`-drive` 指定磁盘镜像，`-nographic` 禁用图形输出，`-s` 在 1234 端口启动 GDB 服务器，`-S` 让 CPU 在启动时暂停等待 GDB 连接。

`$!` 是 bash 的特殊变量，表示最后一个后台进程的 PID。我们把它保存下来，后面用来清理进程。

### 启动 GDB

QEMU 启动后，我们给它一点时间完全启动，然后启动 GDB。

```bash
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
```

这里我们切换到项目根目录，然后启动 GDB。`-x` 参数指定初始化文件，后面跟着内核 ELF 文件的路径。

### 清理进程

GDB 退出后，我们关闭 QEMU。

```bash
# GDB 退出后，关闭 QEMU
echo
echo -e "${BLUE}正在关闭 QEMU...${NC}"
kill $QEMU_PID 2>/dev/null || true
wait $QEMU_PID 2>/dev/null || true

echo -e "${GREEN}调试会话结束${NC}"
```

`kill $QEMU_PID` 发送 TERM 信号给 QEMU，让它正常退出。`2>/dev/null` 忽略错误输出，`|| true` 确保即使 kill 失败脚本也不会退出。`wait` 等待进程真正结束。

### 完整脚本

把这些部分组合起来，完整的 `debug.sh` 脚本已经在 `scripts/` 目录下，你可以直接使用。赋予执行权限：

```bash
chmod +x scripts/debug.sh
```

---

## 第三步：配置 .gdbinit

### .gdbinit 的作用

`.gdbinit` 是 GDB 的初始化配置文件，GDB 启动时会自动执行里面的命令。有了这个文件，我们就不用每次启动 GDB 时都手动输入一堆初始化命令了。

### 连接和架构设置

首先我们需要让 GDB 连接到 QEMU，并设置正确的架构。

```
# CCOS GDB 初始化配置文件
# 使用方法: gdb -x .gdbinit kernel.elf

target remote :1234
set architecture i386:x86-64:intel
```

`target remote :1234` 告诉 GDB 连接到本地的 1234 端口，也就是 QEMU 的 GDB 服务器。`set architecture` 设置 CPU 架构为 x86-64，并使用 Intel 汇编语法（相比 AT&T 语法更易读）。

### 配置输出格式

接下来我们配置 GDB 的输出格式，让调试信息更友好。

```
set print pretty on
set print array on
set print elements 0
set disassembly-flavor intel
```

`set print pretty on` 让结构体以易读的格式打印，每个字段一行。`set print array on` 让数组完整显示，而不是省略中间元素。`set print elements 0` 取消元素数量限制，显示完整的数组和结构体。最后一个命令我们已经见过了，设置汇编语法为 Intel 格式。

### 调试模式开关

我们可以在 `.gdbinit` 中添加一个可选的断点，这样每次启动时都会在内核入口停下来。

```
# ==================== 调试模式开关 ====================
# 取消下面那行的注释，就会在 kernel_main 自动停下来
# break kernel_main
# echo \n已在 kernel_main 设置断点\n
# =====================================================
```

如果你想在 `kernel_main` 自动停下，就把注释去掉。平时可以先保持注释状态，需要的时候再打开。

### 加载符号文件

接下来我们加载调试符号。

```
echo "Load Symbol file...\n"
symbol-file build/kernel.elf
```

`symbol-file` 命令加载带调试符号的内核文件。注意路径是相对于项目根目录的，因为我们在 debug.sh 中 `cd` 到了项目根目录。

### 显示当前指令

```
display/i $pc
```

`display` 命令让 GDB 每次停顿后自动显示某个表达式。`/i` 表示以指令格式显示，`$pc` 是程序计数器，即当前指令地址。这样每次程序停下来，你都能看到当前要执行的指令。

### 欢迎信息和命令提示

最后我们添加一个欢迎信息，提醒用户常用的调试命令。

```
echo \n
echo =======================================\n
echo CCOS Kernel GDB 调试环境已就绪\n
echo =======================================\n
echo 输入 'c' 开始执行，或 'si' 单步调试\n
echo 输入 'help' 查看更多命令\n
echo =======================================\n
echo \n
```

完整的 `.gdbinit` 文件已经在 `scripts/` 目录下，你可以直接使用或根据需要修改。

---

## 第四步：第一次调试体验

现在让我们来试试调试环境是否正常工作。

### 启动调试

在项目根目录执行：

```bash
./scripts/debug.sh
```

你应该能看到这样的输出：

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

GDB 调试命令:
  target remote :1234    - 连接到 QEMU
  b kernel_main         - 在 kernel_main 设置断点
  c                     - 继续执行
  si                    - 单步执行（指令级）
  info registers        - 查看寄存器
  x/10i $pc             - 查看当前指令
  quit                  - 退出 GDB（会自动关闭 QEMU）
```

然后 GDB 会加载符号并显示欢迎信息，最后停在 `(gdb)` 提示符等待你的命令。

### 设置断点并运行

现在让我们在内核入口点设置断点。内核的入口地址是 `0x10000`，我们可以直接在那里打断点。

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

太棒了，我们成功在内核入口点停下来了。虽然显示的是 `??` 因为这里还没有符号信息，但我们已经停在正确的位置了。

### 查看当前指令

让我们看看当前执行的指令是什么：

```
(gdb) x/10i $pc
=> 0x10000:     mov     eax, 0x11111111
   0x10005:     mov     qword ptr [rsp - 8], rbp
   0x1000a:     mov     rbp, rsp
   0x1000d:     sub     rsp, 0x10
   ...
```

`x/10i $pc` 命令从当前程序计数器开始显示 10 条指令。`=>` 表示当前要执行的那条指令。你可以看到内核的第一条指令是把 `0x11111111` 加载到 `eax` 寄存器，这是我们的初始化魔数。

### 查看寄存器

想看看 CPU 的状态吗？

```
(gdb) info registers
rax            0x11111111   286331153
rbx            0x0          0
rcx            0x0          0
rdx            0x0          0
...
rsp            0x7000       0x7000
rbp            0x7000       0x7000
...
rip            0x10000      0x10000
```

`info registers` 显示所有通用寄存器的值。你可以看到 `rax` 已经被设置为 `0x11111111`，这是内核初始化代码的第一条指令的执行结果。栈指针 `rsp` 和基址指针 `rbp` 都指向 `0x7000`，这是 bootloader 设置好的栈位置。

### 单步执行

单步执行可以让你一条指令一条指令地看程序是怎么跑的：

```
(gdb) si
0x0000000000010005 in ?? ()
```

每次执行 `si`，CPU 就执行一条指令。你可以持续输入 `si` 看着程序一步步执行。如果觉得每次都输入 `si` 太麻烦，可以按回车键，GDB 会重复上一条命令。

### 退出调试

调试完成后，输入 `quit` 退出：

```
(gdb) quit

正在关闭 QEMU...
调试会话结束
```

脚本会自动清理 QEMU 进程，你不需要手动 kill。这一点真的很方便，不用每次都去查进程然后杀掉它。

---

## 常见问题排查

### QEMU 启动失败

如果你看到"未安装 qemu-system-x86_64"的错误，说明系统没有安装 QEMU。

Ubuntu/Debian 系统安装：

```bash
sudo apt install qemu-system-x86
```

Fedora/RHEL 系统：

```bash
sudo dnf install qemu-system-x86
```

Arch Linux：

```bash
sudo pacman -S qemu-system-x86
```

### GDB 找不到符号

如果你在 GDB 中看到 "(no debugging symbols found)" 的提示，说明 ELF 文件没有包含调试符号。

检查你是否使用 Debug 模式编译：

```bash
# 重新配置和编译
rm -rf build
cmake -DCMAKE_BUILD_TYPE=Debug -B build
cmake --build build

# 验证符号文件
file build/kernel.elf
```

应该能看到 `with debug_info` 字样。如果还是显示 `stripped`，检查 CMakeLists.txt 中的 Debug 标志是否正确。

### 端口被占用

如果你看到"Connection refused"之类的错误，可能是 1234 端口被其他进程占用了。

查看端口占用：

```bash
lsof -ti:1234
```

如果有进程占用，可以停止它：

```bash
kill -9 $(lsof -ti:1234)
```

或者先停止可能正在运行的 QEMU 进程：

```bash
killall qemu-system-x86_64
```

---

## 总结

到这里，我们已经成功搭建了基础的 GDB 调试环境。我们配置了 CMake Debug 模式，创建了 debug.sh 脚本实现一键启动，配置了 .gdbinit 让 GDB 自动连接和设置，并且能够设置断点、单步执行、查看寄存器。

但这只是开始。有了调试符号和 GDB 连接，我们还缺少一个直观的输出方式。在下一篇文章中，我们会实现 VGA 文本模式驱动，这样调试时就能看到可视化的输出，而不是一片漆黑。

→ [下一篇：VGA 文本模式驱动实战](./03_VGA文本模式驱动实战.md)


---

<div align="center">

## 文档导航

[← 为什么我们需要调试设施](01_为什么我们需要调试设施.md)  | [VGA文本模式驱动实战 →](03_VGA文本模式驱动实战.md)

</div>
