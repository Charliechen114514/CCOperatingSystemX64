# 连接到 QEMU

## 连接命令

GDB 启动后，QEMU 已经在等待连接。输入：

```gdb
target remote :1234
```

## 成功连接的输出

```
Remote debugging using :1234
0x000000000000fff0 in ?? ()
1: x/i $pc
=> 0xfff0:  add    BYTE PTR [rax],al
(gdb)
```

## 当前状态说明

- **地址 `0xfff0** - 这是 CPU 复位后的第一行指令位置（BIOS 入口）
- **`??`** - GDB 没有此地址的符号信息（这是正常的，这是 BIOS 代码）
- **display 自动显示** - `.gdbinit` 配置了 `display/i $pc`，会自动显示当前指令

## 下一步

现在需要设置断点并运行到内核代码，进入 [04_设置断点.md](04_设置断点.md)
