# VSCode 调试内核指南

本文档介绍如何使用 VSCode 调试 CCOS 内核。

## 工作原理

VSCode 调试内核的原理是通过 GDB 远程调试连接到 QEMU：

```
┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐
│   VSCode        │ ─────── │   GDB (MI)      │ ─────── │   QEMU          │
│ (Debugger UI)   │ attach  │ (gdb miDebugger)│ remote  │ (GDB Server)    │
│                 │         │                 │ :1234   │                 │
└─────────────────┘         └─────────────────┘         └─────────────────┘
```

1. **QEMU** 以调试模式启动 (`-s -S`)，在端口 1234 监听 GDB 连接
2. **VSCode** 通过 GDB MI 协议连接到 QEMU
3. **GDB** 加载 `kernel.elf` 符号文件，提供源代码级调试
4. **监控脚本** 自动检测 GDB 断开连接，自动停止 QEMU

## 快速开始

```bash
# 1. 启动 QEMU（带自动监控）
./scripts/launch_qemu_for_vscode_debug.sh

# 2. 在 VSCode 中按 F5，选择 "Attach QEMU GDB"

# 3. 停止调试时，QEMU 会自动停止！
```

## 调试流程

### 第一步：启动 QEMU

```bash
./scripts/launch_qemu_for_vscode_debug.sh
```

输出示例：

```
========================================
CCOS QEMU 调试服务器启动脚本
========================================

正在启动 QEMU 调试模式...

QEMU 参数:
  -drive format=raw,file=build/boot.img,if=ide
  -nographic
  -s (GDB server on :1234)
  -S (暂停启动)

QEMU 已启动 (PID: 12345)
等待 QEMU 完全启动...

========================================
QEMU GDB Server 已就绪
========================================
监听端口: localhost:1234
符号文件: build/kernel.elf

现在可以在 VSCode 中启动调试了！
停止调试时 QEMU 会自动停止

[监控] 正在监控 GDB 连接状态...
```

脚本会：
- 检查构建文件是否存在
- 启动 QEMU（后台运行）
- **自动监控** GDB 连接状态
- **当检测到 GDB 断开时，自动停止 QEMU**
- 显示连接状态信息

### 第二步：在 VSCode 中启动调试

1. 在 VSCode 中打开项目
2. 按 `F5` 或点击「运行和调试」面板
3. 选择 **"Attach QEMU GDB"** 配置

调试器会自动连接到 QEMU 并加载内核符号。

脚本会显示：

```
[监控] GDB 已连接 (1)
```

### 第三步：调试操作

VSCode 提供的调试功能：

| 操作 | 快捷键 | 说明 |
|------|--------|------|
| 继续 | `F5` | 继续执行程序 |
| 单步跳过 | `F10` | 单步执行（不进入函数） |
| 单步进入 | `F11` | 单步执行（进入函数） |
| 单步跳出 | `Shift+F11` | 跳出当前函数 |
| 重启 | `Ctrl+Shift+F5` | 重启调试 |
| 停止 | `Shift+F5` | 停止调试 |

### 第四步：停止调试

**停止 VSCode 调试时，QEMU 会自动停止！**

脚本会检测到 GDB 断开连接：

```
[监控] 检测到 GDB 已断开连接
[监控] 正在自动停止 QEMU...
[监控] QEMU 已自动停止
```

无需手动停止 QEMU，脚本会自动清理。

## 手动管理

如果需要手动管理 QEMU 进程：

```bash
# 查看状态
./scripts/launch_qemu_for_vscode_debug.sh --status

# 手动停止
./scripts/launch_qemu_for_vscode_debug.sh --stop

# 查看帮助
./scripts/launch_qemu_for_vscode_debug.sh --help
```

## VSCode 调试配置

项目的 `.vscode/launch.json` 已预配置好调试选项：

```json
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
        }
    ]
}
```

配置说明：
- `program`: 符号文件路径（`kernel.elf`）
- `miDebuggerServerAddress`: QEMU GDB 服务器地址
- `setupCommands`: 设置 x86-64 架构和 Intel 汇编语法

## 设置断点

在 VSCode 中设置断点非常简单：

1. **源代码断点**：点击代码行号左侧
2. **条件断点**：右键行号 → 「添加条件断点」
3. **日志点**：右键行号 → 「添加日志点」（不中断，只输出消息）

示例：

```c
// 在 kernel/kernel_main.c 中点击第 42 行设置断点
void kernel_main(void) {
    int x = 42;  // ← 在这里设置断点
    // ...
}
```

## 查看变量和内存

### 变量查看

调试时，VSCode 会自动显示：
- **变量**面板：当前作用域的所有变量
- **监视**面板：添加自定义表达式监视
- **调用堆栈**面板：函数调用链

### 内存查看

在「调试控制台」中使用 GDB 命令：

```
-exec x/10x 0x100000    # 查看内存（十六进制）
-exec x/20i $pc          # 查看指令
```

## 脚本功能

`launch_qemu_for_vscode_debug.sh` 脚本功能：

```bash
./scripts/launch_qemu_for_vscode_debug.sh          # 启动 QEMU
./scripts/launch_qemu_for_vscode_debug.sh --stop   # 停止 QEMU
./scripts/launch_qemu_for_vscode_debug.sh --status # 查看状态
./scripts/launch_qemu_for_vscode_debug.sh --help   # 帮助
```

**自动特性：**
- 检测构建文件和依赖工具
- 监控 GDB 连接状态
- GDB 断开时自动停止 QEMU

## 常见问题

### Q: 无法连接到 GDB 服务器

**A:** 确保 QEMU 已经启动：

```bash
# 检查端口是否在监听
./scripts/launch_qemu_for_vscode_debug.sh --status

# 或手动检查
netstat -tlnp | grep 1234
ss -tlnp | grep 1234
```

### Q: 调试器显示符号文件找不到

**A:** 确保已构建项目：

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -B build
cmake --build build
```

### Q: QEMU 进程残留

**A:** 使用脚本停止：

```bash
./scripts/launch_qemu_for_vscode_debug.sh --stop
```

### Q: 断点不起作用

**A:** 确保使用 Debug 构建类型：

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -B build
cmake --build build --clean-first
```

### Q: 监控脚本不工作

**A:** 确保系统已安装 `lsof` 和 `ss` 工具：

```bash
# Debian/Ubuntu
sudo apt install lsof iproute2

# Fedora/RHEL
sudo dnf install lsof iproute
```

## 自动化调试

### 预设断点

在 `.vscode/launch.json` 中添加初始化命令：

```json
"setupCommands": [
    {
        "text": "-gdb-set architecture i386:x86-64"
    },
    {
        "text": "-gdb-set disassembly-flavor intel"
    },
    {
        "text": "-break-insert kernel_main",
        "description": "在 kernel_main 设置断点"
    }
]
```

## 与命令行调试对比

| 特性 | VSCode | 命令行 (debug.sh) |
|------|--------|-------------------|
| 源代码视图 | 图形化，直观 | 命令行 |
| 断点管理 | 鼠标点击 | 命令输入 |
| 变量查看 | 自动显示 | 手动输入命令 |
| 步骤调试 | 快捷键 | 命令输入 |
| 自动清理 | ✅ 脚本自动停止 | ✅ 脚本自动停止 |
| 学习曲线 | 较低 | 较高 |
| 灵活性 | 中等 | 高 |

## 下一步

- 学习 [04_设置断点.md](04_设置断点.md) 了解更多断点技巧
- 学习 [05_运行到内核.md](05_运行到内核.md) 了解程序控制
- 学习 [06_单步调试.md](06_单步调试.md) 了解单步执行
