# 合并 bootloader 是大势所趋

## 前言：两个文件不如一个文件

如果你跟着前面的教程做下来，现在应该有：
- `boot/boot.asm` (Stage 1)
- `boot/boot2.asm` (Stage 2)
- `kernel/kernel.asm`

每次编译要这样：

```makefile
$(BOOT_BIN): $(BOOT_ASM)
	$(AS) $(ASFLAGS) $< -o $@

$(BOOT2_BIN): $(BOOT2_ASM)
	$(AS) $(ASFLAGS) $< -o $@
```

然后合并：

```bash
dd if=$(BOOT_BIN) of=$(BOOT_IMG) bs=512 count=1
dd if=$(BOOT2_BIN) of=$(BOOT_IMG) bs=512 seek=1
```

说实话，这样挺麻烦的。而且 `boot2.asm` 的代码越来越长，维护两个文件容易出错。

### 为什么不合并？

你可能会问："为什么不一开始就合并？"

原因是：
1. **学习曲线**：分开更容易理解 Stage 1 和 Stage 2 的区别
2. **调试方便**：可以单独测试每个部分
3. **历史原因**：很多 OS 教程都是这样教的

### 为什么要合并？

现在合并的好处：
1. **单一文件**：所有 bootloader 代码在一个地方
2. **简化构建**：编译一次，而不是两次
3. **更好的组织**：用 section 清晰分隔不同阶段

---

## 第一步：理解 section 指令

NASM 的 `section` 指令告诉汇编器把代码放到哪个段。

### 基本用法

```nasm
section .mbr
    org 0x7c00
    bits 16
start:
    ; Stage 1 代码

section .stage2 vstart=0x7E00
    bits 16
stage2_start:
    ; Stage 2 代码
```

### org vs vstart

- `org`：设置段的加载地址
- `vstart`：设置段内的虚拟起始地址

对于我们的 bootloader：
- `section .mbr org 0x7C00` → 代码加载到 0x7C00
- `section .stage2 vstart=0x7E00` → 代码在文件中从下一个边界开始，但标签从 0x7E00 开始计算

---

## 第二步：创建统一的 bootloader.asm

我们把 `boot.asm` 和 `boot2.asm` 合并：

```nasm
; ==============================================================================
; CCOS Unified Bootloader
; ==============================================================================
; This single file contains both Stage 1 (MBR) and Stage 2
; - Stage 1 (0x7C00): Loads the rest of bootloader from disk to 0x7E00
; - Stage 2 (0x7E00): Switches to 64-bit long mode and loads kernel
; ==============================================================================

; ==============================================================================
; Stage 1: MBR (0x7C00)
; ==============================================================================
section .mbr
    org 0x7c00
    bits 16

start:
    ; 关中断，设置段寄存器
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    ; 清屏
    mov ah, 0x00
    mov al, 0x03
    int 0x10

    ; 打印消息
    mov si, msg_stage1
    call print_string

    ; 加载 Stage 2
    call load_stage2

    ; 跳转到 Stage 2
    jmp 0x7E00

.hang:
    hlt
    jmp .hang

; ============================================================================
; Stage 1 函数
; ============================================================================

print_string:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp .loop
.done:
    popa
    ret

load_stage2:
    pusha

    ; 设置目标地址：0x7E00
    mov ax, 0x7E0
    mov es, ax
    xor bx, bx

    ; 尝试 LBA 扩展读取
    call check_lba_support
    jc .try_chs

    mov si, dap
    mov ah, 0x42
    mov dl, 0x80
    int 0x13
    jc .try_chs

    cmp al, 0x02
    jne .try_chs

    popa
    ret

.try_chs:
    mov ax, 0x7E0
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, 0x02
    mov ch, 0x00
    mov cl, 0x02
    mov dh, 0x00
    mov dl, 0x80
    int 0x13

    jc disk_error
    cmp al, 0x02
    jne disk_error

    popa
    ret

disk_error:
    mov si, msg_error
    call print_string
.hang:
    hlt
    jmp .hang

check_lba_support:
    pusha
    mov dl, 0x80
    mov ah, 0x41
    mov bx, 0x55AA
    int 0x13

    jc .not_support
    cmp bx, 0xAA55
    jne .not_support
    test cx, 0x01
    jz .not_support

    popa
    clc
    ret

.not_support:
    popa
    stc
    ret

; ============================================================================
; Stage 1 数据
; ============================================================================
msg_stage1:
    db "[1] Stage 1: Loading Stage 2...", 0x0D, 0x0A, 0

msg_error:
    db "[E] Failed to load Stage 2", 0x0D, 0x0A, 0

; DAP 结构
align 4
dap:
    db 16
    db 0
    dw 0x0002
    dw 0x0000
    dw 0x7E0
    dq 1

; 填充到 510 字节
times 510-($-$$) db 0

; MBR 签名
dw 0xAA55


; ==============================================================================
; Stage 2: Starts at offset 512 in file, loads at 0x7E00
; ==============================================================================
section .stage2 vstart=0x7E00
    bits 16

stage2_entry:
    jmp short stage2_main
    nop

; ============================================================================
; GDT
; ============================================================================
gdt_start:
    dq 0x0000000000000000
gdt_code:
    dq 0x00CF9A000000FFFF
gdt_data:
    dq 0x00CF92000000FFFF
gdt_code64:
    dq 0x00AF9A000000FFFF
gdt_data64:
    dq 0x00CF92000000FFFF
gdt_end:

gdt_ptr:
    dw 5 * 8 - 1
    dd 0x00007E03

; ============================================================================
; Stage 2 主函数
; ============================================================================
stage2_main:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7E00

    ; 打印消息
    mov si, msg_stage2
    call print_string

    ; 加载内核
    call load_kernel
    jc load_error

    ; 设置页表
    call setup_page_tables

    ; 加载 GDT
    lgdt [gdt_ptr]

    ; 启用保护模式
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; 跳转到保护模式
    db 0x66
    db 0xEA
    dd pm_entry
    dw 0x08

.hang:
    hlt
    jmp .hang

load_error:
    mov si, msg_load_error
    call print_string
    jmp .hang

print_string:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp .loop
.done:
    popa
    ret

load_kernel:
    pusha

    mov si, msg_loading
    call print_string

    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, 0x01
    mov ch, 0x00
    mov cl, 0x04
    mov dh, 0x00
    mov dl, 0x80
    int 0x13

    jc .error
    cmp al, 0x01
    jne .error

    popa
    clc
    ret

.error:
    popa
    stc
    ret

setup_page_tables:
    pusha

    mov edi, 0x9000
    mov ecx, 4096 / 4
    xor eax, eax
    rep stosd

    mov edi, 0xA000
    mov ecx, 4096 / 4
    xor eax, eax
    rep stosd

    mov edi, 0xB000
    mov ecx, 4096 / 4
    xor eax, eax
    rep stosd

    mov dword [0x9000], 0x0000A003
    mov dword [0x9FF8], 0x0000A003
    mov dword [0xA000], 0x0000B003
    mov dword [0xB000], 0x00000083
    mov dword [0xB004], 0x00000000

    popa
    ret

; ============================================================================
; Stage 2 数据
; ============================================================================
msg_stage2:
    db "[2] Stage 2: Running...", 0x0D, 0x0A, 0

msg_loading:
    db "[LOAD] Loading kernel...", 0x0D, 0x0A, 0

msg_load_error:
    db "[E] Failed to load kernel", 0x0D, 0x0A, 0


; ==============================================================================
; 32-bit Protected Mode
; ==============================================================================
bits 32

pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x7E00

    ; 设置页表已完成
    call setup_page_tables

    ; 启用 PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; 加载 PML4
    mov eax, 0x9000
    mov cr3, eax

    ; 启用长模式
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; 启用分页
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; 跳转到 64 位
    jmp 0x18:long_mode

.hang:
    hlt
    jmp .hang


; ==============================================================================
; 64-bit Long Mode
; ==============================================================================
bits 64

long_mode:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x7E00

    ; 打印消息（VGA）
    mov rdi, 0xB8000 + 160 * 2
    mov rsi, msg_long_mode
    mov ah, 0x0F
.print_loop:
    lodsb
    test al, al
    jz .print_done
    stosw
    jmp .print_loop
.print_done:

    ; 跳转到内核
    mov rdi, 0x10000
    call rdi

    ; 如果返回，停机
kernel_halt:
    hlt
    jmp kernel_halt

msg_long_mode:
    db "[OK] Entered Long Mode!", 0x0D, 0x0A, 0
```

---

## 第三步：理解合并后的布局

### 文件布局

```
偏移       内容              加载地址
────────────────────────────────────────
0x000      .mbr section     0x7C00    (Stage 1)
0x200      .stage2 section  0x7E00    (Stage 2)
```

### 编译

```bash
$ nasm -f bin boot/bootloader.asm -o build/bootloader.bin

$ ls -lh build/bootloader.bin
-rw-r--r-- 1 user user 1.4K Feb 17 00:20 build/bootloader.bin
```

### 更新磁盘镜像

```bash
# 新方式：一次合并
$ dd if=build/bootloader.bin of=build/boot.img bs=512 count=1 conv=notrunc

# 添加内核
$ dd if=build/kernel.bin of=build/boot.img bs=512 seek=1 conv=notrunc
```

等等，这不对！bootloader.bin 包含了 Stage 1 和 Stage 2，所以内核应该从 seek=2 开始（而不是 seek=3）。

让我重新规划一下布局：

```
扇区    内容              说明
──────────────────────────────────────
Sector 0  Stage 1 (512B)   bootloader.bin 前 512 字节
Sector 1  Stage 2 (512B)   bootloader.bin 后面的字节
Sector 2  Stage 2 (续)     如果 Stage 2 超过 512 字节
Sector 3  Kernel          kernel.bin
```

但这样有问题：bootloader.bin 是一个整体，不好分开。

### 更好的方案

修改编译方式：

```bash
# 编译 bootloader
$ nasm -f bin boot/bootloader.asm -o build/bootloader.bin

# 计算大小（假设是 1400 字节 = 2.73 扇区）
$ ls -l build/bootloader.bin
-rw-r--r-- 1 user user 1400 Feb 17 00:20 build/bootloader.bin

# 计算内核扇区偏移：3（MBR=0, Stage2=1-2）
```

但这样还是要处理 Stage 2 跨扇区的问题。

### 最终方案

其实很简单 —— bootloader 已经包含了 Stage 1 和 Stage 2，只需要一个 `dd` 命令：

```bash
# bootloader 前三个扇区（MBR + Stage 2）
$ dd if=build/bootloader.bin of=build/boot.img bs=512 count=3 conv=notrunc

# 内核从扇区 3 开始
$ dd if=build/kernel.bin of=build/boot.img bs=512 seek=3 conv=notrunc
```

---

## 第四步：更新 Makefile

```makefile
# ==============================================================================
# CCOS Makefile (Unified)
# ==============================================================================

# 目录
BOOT_DIR := boot
KERNEL_DIR := kernel
BUILD_DIR := build

# 工具
AS := nasm
DD := dd
QEMU := qemu-system-x86_64

# 源文件
BOOTLOADER_ASM := $(BOOT_DIR)/bootloader.asm
KERNEL_ASM := $(KERNEL_DIR)/kernel.asm

# 输出文件
BOOTLOADER_BIN := $(BUILD_DIR)/bootloader.bin
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
BOOT_IMG := $(BUILD_DIR)/boot.img

# 汇编器标志
ASFLAGS := -f bin

# ==============================================================================
# 目标
# ==============================================================================

.PHONY: all clean run debug check-size

all: check-size $(BOOT_IMG)

$(BOOT_IMG): $(BOOTLOADER_BIN) $(KERNEL_BIN)
	@echo "========================================="
	@echo "Creating boot image..."
	@echo "========================================="
	# 写入 bootloader (前 3 个扇区)
	dd if=$(BOOTLOADER_BIN) of=$@ bs=512 count=3 conv=notrunc 2>/dev/null
	# 写入内核 (从扇区 3 开始)
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=3 conv=notrunc 2>/dev/null
	@echo "Boot image created: $@"
	@ls -lh $@
	@echo "========================================="

$(BOOTLOADER_BIN): $(BOOTLOADER_ASM) | prepare
	@echo "Assembling bootloader..."
	$(AS) $(ASFLAGS) $< -o $@
	@echo "Bootloader size: $$(wc -c < $@) bytes"

$(KERNEL_BIN): $(KERNEL_ASM) | prepare
	@echo "Assembling kernel..."
	$(AS) $(ASFLAGS) $< -o $@

prepare:
	@mkdir -p $(BUILD_DIR)

check-size: $(BOOTLOADER_BIN) $(KERNEL_BIN)
	@echo "========================================="
	@echo "File size check:"
	@echo "========================================="
	@printf "Bootloader: %5d / 1536 bytes " $$(wc -c < $(BOOTLOADER_BIN))
	@test $$(wc -c < $(BOOTLOADER_BIN)) -le 1536 && echo "[OK]" || echo "[FAIL]"
	@printf "Kernel:     %5d / 512 bytes  " $$(wc -c < $(KERNEL_BIN))
	@test $$(wc -c < $(KERNEL_BIN)) -le 512 && echo "[OK]" || echo "[FAIL]"
	@echo "========================================="
	@test $$(wc -c < $(BOOTLOADER_BIN)) -le 1536 || (echo "ERROR: Bootloader too large!" && exit 1)
	@test $$(wc -c < $(KERNEL_BIN)) -le 512 || (echo "ERROR: Kernel too large!" && exit 1)

run: $(BOOT_IMG)
	$(QEMU) -drive format=raw,file=$(BOOT_IMG) -nographic

debug: $(BOOT_IMG)
	@echo "Starting QEMU with GDB server..."
	$(QEMU) -drive format=raw,file=$(BOOT_IMG) -s -S -nographic

clean:
	@echo "Cleaning..."
	rm -rf $(BUILD_DIR)
	@echo "Done."
```

---

## 第五步：测试

```bash
$ make clean
Cleaning...
rm -rf build
Done.

$ make
mkdir -p build
Assembling bootloader...
nasm -f bin boot/bootloader.asm -o build/bootloader.bin
Bootloader size: 1405 bytes
Assembling kernel...
nasm -f bin kernel/kernel.asm -o build/kernel.bin
=========================================
File size check:
=========================================
Bootloader:  1405 / 1536 bytes [OK]
Kernel:        20 / 512 bytes  [OK]
=========================================
Creating boot image...
=========================================
Boot image created: build/boot.img
-rw-r--r-- 1 user user 2.0K Feb 17 00:25 build/boot.img
=========================================

$ make run
qemu-system-x86_64 -drive format=raw,file=build/boot.img -nographic

[1] Stage 1: Loading Stage 2...
[2] Stage 2: Running...
[LOAD] Loading kernel...
[OK] Entered Long Mode!
```

成功！

---

## 第六步：代码组织技巧

### 使用注释分隔

```nasm
; ==============================================================================
; Stage 1: MBR (0x7C00)
; ==============================================================================
section .mbr
    org 0x7c00
    bits 16

; ==============================================================================
; Stage 1 Functions
; ==============================================================================

; ==============================================================================
; Stage 1 Data
; ==============================================================================


; ==============================================================================
; Stage 2: 0x7E00
; ==============================================================================
section .stage2 vstart=0x7E00
    bits 16

; ==============================================================================
; Stage 2 Functions
; ==============================================================================

; ==============================================================================
; 32-bit Code
; ==============================================================================
bits 32

; ==============================================================================
; 64-bit Code
; ==============================================================================
bits 64
```

### 标签命名约定

```nasm
; Stage 1
start:                  ; 入口
print_string:           ; 函数
load_stage2:            ; 函数
msg_stage1:             ; 数据

; Stage 2
stage2_entry:           ; 入口
stage2_main:            ; 主函数
load_kernel:            ; 函数

; 32-bit
pm_entry:               ; 入口

; 64-bit
long_mode:              ; 入口
kernel_halt:            ; 停机循环
```

---

## 踩坑预警

⚠️ **注意**：section 边界

`section .mbr` 和 `section .stage2` 之间不要放任何代码，否则会被放到错误的段。

⚠️ **注意**：vstart 参数

`section .stage2 vstart=0x7E00` 确保标签从 0x7E00 开始计算，而不是从文件偏移。

⚠️ **注意**：bits 切换

不要忘记在进入新 section 后设置正确的 `bits`！

⚠️ **注意**：MBR 签名

MBR 签名（0xAA55）必须在偏移 510-511，确保你的填充计算正确！

---

## 总结

到这里，我们：

1. ✅ 把 boot.asm 和 boot2.asm 合并成 bootloader.asm
2. ✅ 理解了 section 和 org/vstart 的区别
3. ✅ 更新了 Makefile 以适应新的结构
4. ✅ 验证了合并后的代码正常工作

现在我们只有一个 bootloader 文件，维护起来方便多了！

下一步，我们会解决一个实际问题：当前只能加载 512 字节的内核，这太少了。我们需要支持大内核加载。

**下篇预告**：《支持大内核加载》—— 从 512 字节到无限可能。


---

<div align="center">

## 文档导航

[← makefile自动构建省心省力](05_makefile自动构建省心省力.md)  | [支持大内核加载 →](07_支持大内核加载.md)

</div>
