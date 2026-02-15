# CCOS 内核 GDB 调试教程

本教程将一步步教你如何使用 GDB 调试 CCOS 操作系统内核。

## 教程目录

1. [调试准备](01_调试准备.md)
   - 安装必要工具
   - Debug 模式构建
   - 验证构建产物

2. [启动调试环境](02_启动调试环境.md)
   - 使用 debug.sh 脚本
   - 理解启动流程

3. [连接到 QEMU](03_连接到_QEMU.md)
   - target remote 命令
   - 理解初始状态

4. [设置断点](04_设置断点.md)
   - 在地址处设置断点
   - 在函数处设置断点
   - 管理断点

5. [运行到内核](05_运行到内核.md)
   - continue 命令
   - 命中断点
   - 进入 C 代码

6. [单步调试](06_单步调试.md)
   - next/step 命令
   - 汇编级单步

7. [查看变量](07_查看变量.md)
   - print 命令
   - 检查内存

8. [查看寄存器](08_查看寄存器.md)
   - info registers
   - 常用寄存器说明

9. [查看调用栈](09_查看调用栈.md)
   - backtrace 命令
   - 切换栈帧

10. [反汇编](10_反汇编.md)
    - disassemble 命令
    - 查看汇编代码

11. [退出调试](11_退出调试.md)
    - 正确退出流程
    - 清理资源

12. [常用技巧](12_常用技巧.md)
    - 条件断点
    - 观察点
    - TUI 模式

13. [常见问题](13_常见问题.md)
    - 故障排除

## 快速开始

```bash
# 1. 构建
cmake -DCMAKE_BUILD_TYPE=Debug -B build
cmake --build build

# 2. 启动调试
./scripts/debug.sh

# 3. 在 GDB 中
target remote :1234
b *0x10000
c
b kernel_main
c
```

## GDB 速查表

| 命令 | 缩写 | 说明 |
|------|------|------|
| `target remote :1234` | - | 连接到 QEMU |
| `break` | `b` | 设置断点 |
| `continue` | `c` | 继续执行 |
| `next` | `n` | 下一行（不进入函数） |
| `step` | `s` | 单步（进入函数） |
| `nexti` | `ni` | 下一条指令 |
| `stepi` | `si` | 单步指令 |
| `print` | `p` | 打印变量 |
| `info registers` | - | 显示寄存器 |
| `backtrace` | `bt` | 显示调用栈 |
| `disassemble` | - | 反汇编 |
| `quit` | `q` | 退出 |

## 参考资源

- [GDB 官方文档](https://www.gnu.org/software/gdb/documentation/)
- OSDev Wiki - [GDB](https://wiki.osdev.org/GDB)
- QEMU 文档 - [Debugging with GDB](https://www.qemu.org/docs/master/system/debugging.html)
