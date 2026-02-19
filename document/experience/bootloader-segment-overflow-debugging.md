# Bootloader 段边界溢出导致大内核加载失败排查报告

## 问题描述

**症状**: 当内核大小超过 64KB 时，内核在启动过程中出现 `s_kernel_virt_hint` 未初始化（值为 0）的错误，导致系统无法正常启动。

**内核大小**: 152984 字节 ≈ 149KB（远超 64KB）

**错误表现**: VMM 在尝试访问 `s_kernel_virt_hint` 符号时发现该值为 0，而该符号应该在 BSS 段初始化时被设置为正确的虚拟地址。

## 问题根源

**核心原因**: Bootloader 在加载大内核时，只更新偏移寄存器 `BX`，从不更新段寄存器 `ES`，导致当 `BX` 溢出后重新从 0 开始，数据被重复写入同一物理地址范围。

### 技术背景

在 16 位实模式下，内存地址通过 `段:偏移`（Segment:Offset）方式表示：

```
物理地址 = 段值 × 16 + 偏移值
```

- 段寄存器（ES、DS、CS 等）是 16 位的
- 偏移寄存器（BX、SI 等）也是 16 位的
- 单个段的最大可寻址范围：64KB（0x0000 ~ 0xFFFF）

## 问题代码分析

### 1. LBA 加载函数（第 934-953 行）

**修复前**:
```asm
; Advance buffer pointer (ES:BX += BP * 512)
push ax
push dx
mov ax, bp
xor dx, dx
mov cx, 512
mul cx                          ; DX:AX = bytes read
add bx, ax                      ; ← 只更新 BX！
pop dx
pop ax
```

**修复后**:
```asm
; Advance buffer pointer (ES:BX += BP * 512)
; Handle segment overflow for kernels > 64KB
push ax
push dx
mov ax, bp
xor dx, dx
mov cx, 512
mul cx                          ; DX:AX = bytes read

; Add to BX, check for overflow into segment
add bx, ax
jnc .no_seg_update              ; Jump if no carry

; BX overflowed - advance ES by 0x1000 (64KB paragraph)
mov ax, es
add ax, 0x1000
mov es, ax
.no_seg_update:
pop dx
pop ax
```

### 2. CHS 加载函数（第 1135-1156 行）

**修复前**:
```asm
; Advance buffer pointer (ES:BX += BP * 512)
; For kernel < 64KB, we can ignore segment overflow  ← 注释已经暗示了问题！
push ax
push dx
mov ax, bp
xor dx, dx
mov cx, 512
mul cx                          ; DX:AX = bytes read
; Add to BX
add bx, ax                      ; ← 只更新 BX！
pop dx
pop ax
```

**修复后**:
```asm
; Advance buffer pointer (ES:BX += BP * 512)
; Handle segment overflow for kernels > 64KB
push ax
push dx
mov ax, bp
xor dx, dx
mov cx, 512
mul cx                          ; DX:AX = bytes read

; Add to BX, check for overflow into segment
add bx, ax
jnc .no_seg_update_chs          ; Jump if no carry

; BX overflowed - advance ES by 0x1000 (64KB paragraph)
push ax
mov ax, es
add ax, 0x1000
mov es, ax
pop ax
.no_seg_update_chs:
pop dx
pop ax
```

## 溢出分析

假设内核大小为 152984 字节，初始 `ES=0x1000`, `BX=0x0000`：

| 字节范围 | 操作后 BX 值 | ES:BX 物理地址 | 实际写入位置 | 状态 |
|---------|-------------|---------------|-------------|------|
| 0 ~ 65535 | 0x0000 ~ 0xFFFF | 0x10000:0x0000 ~ 0x10000:0xFFFF | 0x10000 ~ 0x1FFFF | ✓ 正确 |
| 65536 ~ 131071 | 0x0000 ~ 0xFFFF（溢出归零）| 0x10000:0x0000 ~ 0x10000:0xFFFF | 0x10000 ~ 0x1FFFF | ✗ 覆盖第一遍！ |
| 131072 ~ 152983 | 0x0000 ~ 0x5590 | 0x10000:0x0000 ~ 0x10000:0x5590 | 0x10000 ~ 0x15590 | ✗ 再次覆盖 |

### 数据覆盖示意图

```
磁盘镜像中的内核数据（正确）:
┌────────────────────────────────────────────────┐
│ 0x00000 ~ 0x0FFFF: 第一遍数据                  │
│ 0x10000 ~ 0x1FFFF: 第二遍数据                  │
│ 0x20000 ~ 0x25590: 第三遍数据（含 s_kernel...）│
└────────────────────────────────────────────────┘

实际加载到内存（错误）:
┌────────────────────────────────────────────────┐
│ 物理地址 0x10000 ~ 0x1FFFF:                    │
│   ← 第一遍数据写入（覆盖前 65536 字节）         │
│   ← 第二遍数据写入（覆盖相同位置！）            │
│   ← 第三遍数据写入（再次覆盖！）                │
│                                                │
│ 物理地址 0x35590:                               │
│   ← 从未被写入！保留上电后的 0                 │
└────────────────────────────────────────────────┘
```

## 为什么问题被隐藏

1. **早期内核很小**: 最初的内核代码不到 64KB，问题不会触发
2. **注释误导**: CHS 加载函数中有注释 `; For kernel < 64KB, we can ignore segment overflow`，让人以为这是正常行为
3. **静默失败**: 数据覆盖不会产生任何异常，只是某些数据被错误地写入，某些位置从未被写入

## 修复原理

### 段推进计算

当 `BX` 溢出时（`add bx, ax` 产生进位标志）：

```
新 ES 值 = 旧 ES 值 + 0x1000
```

为什么是 `0x1000`：
- 1 个段 = 65536 字节 = 64KB
- 段寄存器存储的是"段号"，需要乘以 16 才是物理地址
- 65536 / 16 = 4096 = 0x1000
- 所以增加 1 个段相当于段寄存器值增加 `0x1000`

### 修复后内存布局

```
修复后的内存布局（正确）:
┌────────────────────────────────────────────────┐
│ ES=0x1000, BX=0x0000 ~ 0xFFFF:                │
│   → 物理地址 0x10000 ~ 0x1FFFF                 │
│                                                │
│ ES=0x2000, BX=0x0000 ~ 0xFFFF:                │
│   → 物理地址 0x20000 ~ 0x2FFFF                 │
│                                                │
│ ES=0x3000, BX=0x0000 ~ 0x5590:                │
│   → 物理地址 0x30000 ~ 0x35590                 │
│   → s_kernel_virt_hint 现在能被正确写入！      │
└────────────────────────────────────────────────┘
```

## 验证方法

### 1. 检查内核大小

```bash
# 查看内核二进制大小
ls -l build/Release/kernel.bin

# 查看内核 ELF 大小和段信息
readelf -S build/Release/kernel.elf
size build/Release/kernel.elf
```

### 2. 添加调试输出

在 bootloader 中添加段值输出：

```asm
; 在更新 ES 后
mov ax, es
call print_word_hex     ; 打印当前段值
mov al, ':'
call serial_write_char_blocking
mov ax, bx
call print_word_hex     ; 打印当前偏移值
```

### 3. QEMU/GDB 验证

```bash
# 启动调试
qemu-system-x86_64 -drive format=raw,file=boot.img,if=ide -s -S

# 在 GDB 中检查内存
(gdb) x/50bx 0x10000    # 检查第一段
(gdb) x/50bx 0x20000    # 检查第二段
(gdb) x/50bx 0x30000    # 检查第三段
```

## 相关文件

| 文件 | 修改内容 |
|-----|---------|
| `boot/bootloader.asm` | 修复 `load_kernel_lba` 和 `load_kernel_chs` 函数中的段溢出处理 |

## 技术要点总结

### 1. 16 位实模式寻址

```
物理地址 = 段值 × 16 + 偏移
```

- 段值每增加 1，物理地址增加 16 字节（1 paragraph）
- 偏移最大 0xFFFF = 65535
- 单个段范围：65536 字节 = 64KB

### 2. 段边界跨越策略

**策略 A**: 固定段，累加偏移（简单但有问题）
- 适用于：< 64KB 的数据
- 问题：BX 溢出后回绕

**策略 B**: 动态更新段和偏移（正确）
```asm
add bx, ax
jnc .no_overflow
mov ax, es
add ax, 0x1000    ; 推进一个段
mov es, ax
.no_overflow:
```

### 3. BIOS 磁盘读取限制

- CHS 模式（INT 13h/AH=02h）: 每次最多 127 扇区
- LBA 模式（INT 13h/AH=42h）: 每次最多 127 扇区
- 每扇区 512 字节

## 经验教训

1. **始终考虑边界情况**: 即使当前内核很小，代码也应该能处理更大的内核
2. **16 位寄存器溢出**: 在 16 位代码中，任何累加操作都要考虑溢出
3. **注释不是代码**: 注释说"可以忽略"不代表代码正确
4. **大型内核是必然**: 随着功能增加，内核一定会超过 64KB
5. **静默失败最危险**: 数据覆盖不产生异常，难以调试

## 参考资料

- Intel 64 and IA-32 Architectures Software Developer's Manual
- BIOS Interrupt 13h - Disk Services
- x86 Real Mode Memory Addressing
