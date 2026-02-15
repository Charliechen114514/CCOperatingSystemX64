# 调试准备

## 前提条件

在开始调试之前，请确保已完成以下准备工作：

### 1. 安装必要工具

```bash
# Ubuntu/Debian
sudo apt install qemu-system-x86 gdb

# Fedora/RHEL
sudo dnf install qemu-system-x86 gdb

# Arch Linux
sudo pacman -S qemu-system-x86 gdb
```

### 2. 使用 Debug 模式构建内核

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -B build
cmake --build build
```

### 3. 验证构建产物

确认以下文件存在：

```bash
ls -la build/kernel.elf    # 带调试符号的内核 ELF 文件
ls -la build/boot.img      # 启动镜像
```

`kernel.elf` 包含调试符号，是 GDB 调试所必需的。

### 4. 检查调试配置文件

确认项目存在以下文件：

- `scripts/.gdbinit` - GDB 初始化配置
- `scripts/debug.sh` - 调试启动脚本

## 下一步

准备好了吗？进入 [02_启动调试环境.md](02_启动调试环境.md)
