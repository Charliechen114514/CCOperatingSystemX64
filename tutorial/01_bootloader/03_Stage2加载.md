# Stage 2 加载 —— 突破 512 字节的限制

## 前言

说实话，写到这个时候我才发现：**MBR 的 512 字节限制真的太痛苦了**。

你可能也发现了，我们的 VGA 打印代码、各种初始化代码... 全部塞进 512 字节里，空间已经非常紧张了。

更糟糕的是，我们还没开始写保护模式切换、页表设置这些更复杂的东西呢。

是时候引入两阶段 Bootloader 了。

---

## 什么是两阶段 Bootloader？

两阶段 Bootloader 的思想很简单：

```
┌─────────────────────────────────────────────────────┐
│ BIOS 读取 MBR (512 字节)                              │
│    ↓                                                 │
│ Stage 1 (MBR) 从磁盘加载 Stage 2                     │
│    ↓                                                 │
│ 跳转到 Stage 2 执行                                   │
│    ↓                                                 │
│ Stage 2 可以更大（几个扇区）                          │
│    ↓                                                 │
│ Stage 2 加载内核并切换到保护模式/长模式                │
└─────────────────────────────────────────────────────┘
```

**为什么这样设计？**

1. **突破空间限制**：Stage 2 可以占用多个扇区，有更大的代码空间
2. **职责分离**：Stage 1 只负责加载 Stage 2，Stage 2 负责复杂的初始化
3. **易于维护**：代码分开后更容易理解和修改

⚠️ **关键点**

Stage 1 和 Stage 2 都在**同一个二进制文件**里，只是放在不同的位置：
- MBR：扇区 0（偏移 0-511）
- Stage 2：扇区 1-N（偏移 512 开始）

---

## 第一步：了解磁盘布局

我们的磁盘镜像布局是这样的：

```
扇区      偏移        内容
0         0x0000      MBR (Stage 1)
1         0x0200      Stage 2 代码
2         0x0400      Stage 2 代码（继续）
3         0x0600      Stage 2 代码（继续）
...       ...         ...
N                     内核（从某个扇区开始）
```

对于我们的 CCOS Bootloader：
- Stage 1 = 1 个扇区（512 字节）
- Stage 2 = 3 个扇区（1536 字节）
- 总共 = 4 个扇区（2048 字节）

内核从扇区 4 开始。

---

## 第二步：修改代码结构

现在我们需要重新组织 `bootloader.asm`，把它分成 Stage 1 和 Stage 2 两部分。

创建新的 `boot/bootloader.asm`：

```asm
; ==============================================================================
; CCOS Unified Bootloader
; ==============================================================================
; 这个文件包含 Stage 1 (MBR) 和 Stage 2
; - Stage 1 (0x7C00): 从磁盘加载 Stage 2 到 0x7E00
; - Stage 2 (0x7E00): 切换到 64 位长模式并加载内核
; ==============================================================================

; ==============================================================================
; Bootloader 配置
; ==============================================================================
%define BOOTLOADER_SECTORS  4   ; Stage 1 + Stage 2 总扇区数

; ==============================================================================
; Stage 1: MBR (0x7C00)
; ==============================================================================
section .mbr
    org 0x7c00
    bits 16

start:
    ; ========== 环境初始化 ==========
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    ; ========== 清屏 ==========
    mov ah, 0x00
    mov al, 0x03
    int 0x10

    ; ========== 打印启动消息 ==========
    mov si, stage1_start_msg
    call print_string

    ; ========== 加载 Stage 2 ==========
    ; 使用 BIOS INT 13h AH=02h 读取磁盘
    mov ax, 0x7E0       ; ES = 0x7E0 (目标地址 = 0x7E00)
    mov es, ax
    xor bx, bx          ; BX = 0 (偏移 = 0)

    mov ah, 0x02        ; BIOS 读取功能
    mov al, BOOTLOADER_SECTORS - 1  ; 读取扇区数（不包括 MBR）
    mov ch, 0x00        ; 柱面 = 0
    mov cl, 0x02        ; 扇区 = 2 (LBA 1，从第二个扇区开始)
    mov dh, 0x00        ; 磁头 = 0
    mov dl, 0x80        ; 驱动器 = 第一个硬盘

    int 0x13            ; 调用 BIOS
    jc load_error       ; CF=1 表示错误
    cmp al, BOOTLOADER_SECTORS - 1  ; 验证读取的扇区数
    jne load_error

    ; ========== 跳转到 Stage 2 ==========
    mov si, stage1_success_msg
    call print_string

    jmp 0x7E00          ; 跳转到 Stage 2

load_error:
    mov si, load_error_msg
    call print_string
.hang:
    hlt
    jmp .hang

; ========== VGA 打印字符串 ==========
print_string:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0e
    mov bh, 0
    mov bl, 0x07
    int 0x10
    jmp .loop
.done:
    popa
    ret

; ========== Stage 1 消息 ==========
stage1_start_msg:
    db "[1] Stage 1: Loading Stage 2...", 0x0d, 0x0a, 0

stage1_success_msg:
    db "[1] Stage 2 loaded successfully!", 0x0d, 0x0a, 0

load_error_msg:
    db "[E1] Failed to load Stage 2", 0x0d, 0x0a, 0

; ========== MBR 填充和签名 ==========
times 510-($-$$) db 0
dw 0xaa55
```

---

## 第三步：编写 Stage 2

现在在同一文件中继续添加 Stage 2 代码：

```asm
; ==============================================================================
; Stage 2: 从偏移 512 开始，加载到 0x7E00
; ==============================================================================
section .stage2 vstart=0x7E00
    bits 16

stage2_entry:
    jmp short stage2_main
    nop                 ; 填充对齐

stage2_main:
    ; ========== 重新初始化环境 ==========
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7E00
    sti

    ; ========== 打印 Stage 2 消息 ==========
    mov si, stage2_start_msg
    call print_string

    ; ========== TODO: 这里将添加保护模式切换 ==========
    ; 下一篇文章会实现这个

    ; ========== 暂时停在这里 ==========
.hang:
    hlt
    jmp .hang

; ========== VGA 打印字符串 ==========
print_string:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0e
    mov bh, 0
    mov bl, 0x07
    int 0x10
    jmp .loop
.done:
    popa
    ret

; ========== Stage 2 消息 ==========
stage2_start_msg:
    db "[2] Stage 2: Running from 0x7E00", 0x0d, 0x0a, 0
    db "[2] TODO: Switch to protected mode", 0x0d, 0x0a, 0
```

---

## 第四步：更新 Makefile

我们的 Makefile 需要更新，因为现在 bootloader 不再是 512 字节了。

修改 `boot/Makefile`：

```makefile
NASM = nasm
QEMU = qemu-system-x86_64

BOOT_SRC = bootloader.asm
BOOT_BIN = $(BUILD_DIR)/bootloader.bin
BOOT_IMG = $(BUILD_DIR)/boot.img

# Bootloader 扇区数（Stage 1 + Stage 2）
BOOTLOADER_SECTORS = 4

BUILD_DIR = ../build

.PHONY: all
all: $(BOOT_IMG)

# 编译 bootloader（不再限制为 512 字节）
$(BOOT_BIN): $(BOOT_SRC)
	@echo "==> Building bootloader..."
	@mkdir -p $(BUILD_DIR)
	$(NASM) -f bin $< -o $@
	@echo "==> Build complete: $@"
	@echo "==> Size: $$(stat -c%s $@) bytes"

# 创建磁盘镜像（足够大以容纳 bootloader + 内核）
$(BOOT_IMG): $(BOOT_BIN)
	@echo "==> Creating disk image..."
	dd if=/dev/zero of=$@ bs=512 count=2880 status=none
	dd if=$< of=$@ bs=512 conv=notrunc status=none
	@echo "==> Disk image created: $@"

.PHONY: run
run: $(BOOT_IMG)
	$(QEMU) -drive format=raw,file=$(BOOT_IMG),if=floppy -nographic

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	@echo "==> Clean complete"

.PHONY: check
check: $(BOOT_BIN)
	@echo "==> Bootloader size: $$(stat -c%s $<) bytes"
	@echo "==> Sectors: $$(($$(stat -c%s $<) / 512))"
	@echo "==> Expected: $(BOOTLOADER_SECTORS) sectors"
```

---

## 第五步：编译和测试

```bash
cd boot
make clean
make
```

你应该看到：

```
==> Building bootloader...
==> Build complete: ../build/bootloader.bin
==> Size: 856 bytes
==> Creating disk image...
==> Disk image created: ../build/boot.img
```

注意文件大小现在是 856 字节（不到 2 个扇区），不再是 512 字节了。

运行测试：

```bash
make run
```

你应该在屏幕上看到：

```
[1] Stage 1: Loading Stage 2...
[1] Stage 2 loaded successfully!
[2] Stage 2: Running from 0x7E00
[2] TODO: Switch to protected mode
```

🎉 **成功！**

---

## 第六步：理解内存布局

现在我们的内存布局是这样的：

```
地址         大小      内容
0x0000      -         IVT（中断向量表）
0x7C00      512B      Stage 1 (MBR)
0x7E00      1536B     Stage 2 代码
0x8400      -         栈区域（向下增长）
...
0x9000      4096B     页表（后面会用）
...
0x10000     -         内核加载地址
```

⚠️ **重要**

Stage 2 从 `0x7E00` 开始，紧接在 Stage 1 后面。Stage 1 在 `0x7C00`，占 512 字节，所以 Stage 1 结束于 `0x7E00`（`0x7C00 + 0x200 = 0x7E00`）。

---

## 第七步：常见问题

### 问题 1：Stage 2 没有执行

**症状**：只看到 Stage 1 的消息。

**原因**：
1. 磁盘读取失败
2. 跳转地址不正确
3. Stage 2 代码损坏

**调试方法**：

检查二进制文件：

```bash
# 查看文件大小
ls -lh ../build/bootloader.bin

# 反汇编检查
ndisasm -b 16 ../build/bootloader.bin | head -50
```

### 问题 2：跳转后崩溃

**症状**：看到 "Stage 2 loaded" 但之后卡住或重启。

**原因**：
1. Stage 2 代码没有正确初始化段寄存器
2. 栈指针设置不正确
3. `vstart` 设置不正确

**解决**：

确保 Stage 2 开始时重新初始化所有寄存器：

```asm
stage2_main:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7E00      ; 栈设在 Stage 2 起始处
    sti
```

---

## 完整的内存布局图

```
┌─────────────────────────────────────────────────────┐
│ 0x00000                                             │
│ ├─ IVT (中断向量表)                                  │
│ └─ BDA (BIOS 数据区)                                 │
├─────────────────────────────────────────────────────┤
│ 0x07C00                                             │
│ ├─ Stage 1 (MBR) - 512 字节                          │
│ │   • 环境初始化                                      │
│ │   • 磁盘读取                                        │
│ │   • 跳转到 Stage 2                                 │
├─────────────────────────────────────────────────────┤
│ 0x07E00                                             │
│ ├─ Stage 2 - 最多 1536 字节                          │
│ │   • 保护模式切换（下一篇文章）                       │
│ │   • 页表设置                                        │
│ │   • 长模式切换                                      │
│ │   • 内核加载                                        │
├─────────────────────────────────────────────────────┤
│ 0x08400                                             │
│ └─ 栈区域（向下增长）                                  │
├─────────────────────────────────────────────────────┤
│ 0x09000                                             │
│ └─ 页表（PML4, PDPT, PD）                            │
├─────────────────────────────────────────────────────┤
│ 0x10000                                             │
│ └─ 内核加载地址                                       │
└─────────────────────────────────────────────────────┘
```

---

## 下一步

现在我们有了两阶段 Bootloader，可以突破 512 字节的限制。

在下一篇文章中，我们会：
- 深入理解 x86 处理器的模式（实模式 → 保护模式 → 长模式）
- 设置 GDT（全局描述符表）
- 切换到保护模式
- 切换到 64 位长模式

这部分内容比较复杂，但也是最有意思的部分。继续吧！


---

<div align="center">

## 文档导航

[← 从零开始的MBR](02_从零开始的MBR.md)  | [保护模式切换 →](04_保护模式切换.md)

</div>
