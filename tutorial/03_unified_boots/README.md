# CCOS 统一 Bootloader 教程

本教程手把手带你完成 CCOS Bootloader 从两阶段架构到统一架构的重构。

---

## 教程概述

### 什么是统一 Bootloader？

在之前的阶段，bootloader 由两个独立文件组成：
- `boot.asm` - Stage 1 (MBR)
- `boot2.asm` - Stage 2 (加载器)

在本阶段，我们将两个阶段合并为一个文件：
- `bootloader.asm` - 包含 Stage 1 + Stage 2 的统一文件

### 你将学到什么

1. **NASM 高级特性** - section、vstart、org 的区别和使用
2. **串口编程** - 在 16/32/64 位模式下实现串口输出
3. **LBA 磁盘读取** - 使用 BIOS INT 13h 扩展读取功能
4. **CHS 回退机制** - 确保老机器也能正常启动
5. **构建系统** - 使用 CMake 自动生成配置

---

## 教程目录

请按顺序阅读以下教程：

| 序号 | 教程 | 内容 |
|-----|------|------|
| 01 | [从0开始重构Bootloader](./01_从0开始重构Bootloader.md) | 为什么要重构，对比新旧架构 |
| 02 | [第一步_合并两个文件](./02_第一步_合并两个文件.md) | 创建 bootloader.asm，使用 section 分离代码 |
| 03 | [搞懂vstart这个坑](./03_搞懂vstart这个坑.md) | 深入理解 org vs vstart，跨 section 引用问题 |
| 04 | [加上串口输出](./04_加上串口输出.md) | 实现 16/32/64 位串口输出，方便调试 |
| 05 | [实现LBA磁盘读取](./05_实现LBA磁盘读取.md) | 使用 INT 13h AH=42h 扩展读取 |
| 06 | [加上CHS兜底](./06_加上CHS兜底.md) | LBA 到 CHS 转换，自动回退机制 |
| 07 | [更新构建系统](./07_更新构建系统.md) | 扇区布局变化，CMake 自动配置 |
| 08 | [上板测试](./08_上板测试.md) | 完整的测试和验证流程 |

---

## 环境要求

### 硬件
- x86_64 架构
- 至少 4MB 内存（QEMU 默认配置足够）

### 软件
- NASM 2.x 汇编器
- QEMU 模拟器
- GDB 调试器（可选）
- GCC/Make（用于构建）

### 操作系统
- Linux（推荐）
- WSL（Windows Subsystem for Linux）
- macOS（可能需要调整）

---

## 快速开始

### 获取代码

```bash
git clone https://github.com/your-repo/CCOperatingSystemX64.git
cd CCOperatingSystemX64
```

### 构建项目

```bash
make clean
make
```

### 运行测试

```bash
# 使用 QEMU 运行
qemu-system-x86_64 -drive format=raw,file=build/boot.img -serial stdio

# 或者使用 make 命令
make run
```

### 调试

```bash
# 启动 QEMU GDB 服务器
make debug

# 在另一个终端启动 GDB
make gdb
```

---

## 预期输出

如果一切正常，你应该能看到以下输出：

```
=== BootLoader Stage Start ===
READY TO BOOT CCOS
[1] Stage 1: Loading second stage...
[2] Stage 2: Loading kernel...
[MODE] Using LBA extended read
[LOAD] LBA: loading 31 sectors
READY TO BOOT KERNEL
=== BootLoader Stage End ===
```

---

## 代码结构

### 重构前

```
boot/
├── boot.asm      # Stage 1 (MBR, 512 bytes)
├── boot2.asm     # Stage 2 (加载器, ~824 bytes)
└── lib/          # 库函数
```

### 重构后

```
boot/
├── bootloader.asm    # 统一 bootloader (Stage 1 + Stage 2)
└── boot_config.inc   # CMake 自动生成的配置
```

---

## 常见问题

### Q: 编译后 bootloader 太大怎么办？

A: 检查是否有多余的代码或数据，用 `strip` 或手动优化。bootloader 应该在 2-4KB 范围内。

### Q: 串口没有输出怎么办？

A: 检查：
1. 是否调用了 `serial_init`
2. QEMU 是否加了 `-serial stdio` 参数
3. 端口地址是否正确（COM1 是 0x3F8）

### Q: LBA 检测失败怎么办？

A: 确认 BIOS 支持 LBA（QEMU 默认支持）。可以暂时使用 CHS 模式。

### Q: GDB 无法连接怎么办？

A: 确认：
1. QEMU 使用了 `-s -S` 参数
2. GDB 连接到 localhost:1234
3. 防火墙没有阻止连接

---

## 相关文档

### 项目文档
- [../../document/03_unified_boots/](../../document/03_unified_boots/) - 完整技术文档
- [../../document/02_load_asm_kernel/](../../document/02_load_asm_kernel/) - 内核加载文档

### 参考资料外部链接
- [NASM 文档](https://www.nasm.us/docs.php)
- [Intel 64 and IA-32 架构手册](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [OSDev Wiki](https://wiki.osdev.org/)
- [BIOS INT 13h 规范](https://en.wikipedia.org/wiki/INT_13H)

---

## 下一步

完成本教程后，你可以继续学习：

1. **内存管理** - 实现分页和内存分配
2. **中断处理** - 设置 IDT 和中断处理函数
3. **键盘驱动** - 处理键盘输入
4. **定时器** - 实现系统时钟
5. **文件系统** - 支持 FAT32 或 ext2

---

## 贡献

欢迎改进和补充教程：

1. 发现错误直接修改并提交 PR
2. 补充案例在相应文档添加
3. 新增章节遵循现有格式

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-16
**许可**: MIT License


---

<div align="center">

## 文档导航

[← 支持大内核加载](../02_load_asm_kernel/07_支持大内核加载.md)  | [从0开始重构Bootloader →](01_从0开始重构Bootloader.md)

</div>
