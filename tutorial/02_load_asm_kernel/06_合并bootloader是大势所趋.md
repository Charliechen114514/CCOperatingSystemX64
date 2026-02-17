# 合并 bootloader 是大势所趋

## 前言：两个文件不如一个文件

如果你跟着前面的教程做下来，现在应该有三个文件：`boot/boot.asm`（Stage 1）、`boot/boot2.asm`（Stage 2）、`kernel/kernel.asm`。每次编译要这样：

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

说实话，这样挺麻烦的。而且 `boot2.asm` 的代码越来越长，维护两个文件容易出错。如果改了一个地方忘了改另一个，或者两个文件的布局不一致，就会出问题。

你可能会问："为什么不一开始就合并？"原因是这样的：学习曲线上分开更容易理解 Stage 1 和 Stage 2 的区别，调试时可以单独测试每个部分，而且很多 OS 教程都是这样教的。但现在我们已经理解了整个过程，是时候合并了。

合并的好处很明显：单一文件让所有 bootloader 代码在一个地方，编译一次而不是两次，用 section 清晰分隔不同阶段，维护起来更方便。

---

## section 指令：代码怎么分组

NASM 的 `section` 指令告诉汇编器把代码放到哪个段。基本用法是这样的：

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

`org` 设置段的加载地址，`vstart` 设置段内的虚拟起始地址。这两个参数有点微妙，理解它们很重要。

对于我们的 bootloader，`section .mbr org 0x7C00` 意味着代码会被加载到 0x7C00，而 `section .stage2 vstart=0x7E00` 意味着代码在文件中从下一个边界开始，但标签从 0x7E00 开始计算。这个区别很重要——`org` 影响代码在内存中的位置，`vstart` 只影响标签的值。

---

## 创建统一的 bootloader.asm

我们把 `boot.asm` 和 `boot2.asm` 合并成一个文件。首先是文件头部和一些说明：

```nasm
; ==============================================================================
; CCOS Unified Bootloader
; ==============================================================================
; This single file contains both Stage 1 (MBR) and Stage 2
; - Stage 1 (0x7C00): Loads the rest of bootloader from disk to 0x7E00
; - Stage 2 (0x7E00): Switches to 64-bit long mode and loads kernel
; ==============================================================================
```

这些注释很重要，因为几个月后你可能忘了这个文件的结构，或者别人来看你的代码时需要快速理解。

---

## Stage 1：MBR 部分

首先是 Stage 1 的定义：

```nasm
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
```

`start` 是整个 bootloader 的入口点，BIOS 加载 MBR 后会跳到这里。我们首先关中断，设置所有段寄存器为 0，设置栈指针到 0x7C00（就是我们的代码上方），然后重新开中断。这是标准的 16 位启动代码。

然后是清屏和打印消息：

```nasm
    ; 清屏
    mov ah, 0x00
    mov al, 0x03
    int 0x10

    ; 打印消息
    mov si, msg_stage1
    call print_string
```

`int 0x10, ah=0x00, al=0x03` 是 BIOS 的视频中断，设置 80x25 文本模式。这会清屏并把光标移到左上角。然后我们打印 Stage 1 的消息。

接下来是加载 Stage 2：

```nasm
    ; 加载 Stage 2
    call load_stage2

    ; 跳转到 Stage 2
    jmp 0x7E00
```

`load_stage2` 是一个函数，我们稍后定义。加载完成后，我们跳转到 0x7E00，这是 Stage 2 的加载地址。

然后是 Stage 1 的函数定义：

```nasm
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
```

`print_string` 函数打印一个以 null 结尾的字符串。`lodsb` 从 DS:SI 读取一个字节到 AL，SI 自动加 1。`int 0x10, ah=0x0E` 是 BIOS 的 teletype 输出功能，打印 AL 中的字符。

`load_stage2` 函数更复杂一些：

```nasm
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
```

我们首先尝试 LBA 扩展读取，如果失败就回退到 CHS。LBA（Logical Block Addressing）是现代磁盘的寻址方式，用连续的扇区号来访问磁盘。CHS（Cylinder-Head-Sector）是古老的寻址方式，用柱面、磁头、扇区三元组来访问磁盘。

LBA 支持检查是这样的：

```nasm
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
```

`int 0x13, ah=0x41` 是 BIOS 的扩展磁盘驱动功能检查。如果支持 LBA，BX 会返回 0xAA55，CX 的 bit 0 会被设置。我们检查这些标志来确定 LBA 是否可用。

CHS 回退代码：

```nasm
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
```

CHS 读取用 `int 0x13, ah=0x02`，参数是 CH=柱面、CL=扇区、DH=磁头、DL=驱动器（0x80 是第一个硬盘）。我们读取 2 个扇区（AL=0x02）从柱面 0、磁头 0、扇区 2 开始。这是因为扇区 0 是 MBR（我们的 Stage 1），扇区 1 开始是 Stage 2。

Stage 1 的数据部分：

```nasm
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
```

DAP（Disk Address Packet）是 LBA 读取使用的参数包。`times 510-($-$$) db 0` 填充 0 到 510 字节，`$` 是当前地址，`$$` 是 section 开始地址。`dw 0xAA55` 是 MBR 签名，BIOS 检查这个值来确认这是一个有效的启动扇区。

---

## Stage 2：0x7E00 部分

Stage 2 在一个新的 section 中：

```nasm
; ==============================================================================
; Stage 2: Starts at offset 512 in file, loads at 0x7E00
; ==============================================================================
section .stage2 vstart=0x7E00
    bits 16

stage2_entry:
    jmp short stage2_main
    nop
```

`jmp short` 和 `nop` 是为了对齐，和之前的 boot2.asm 保持一致。GDT 从 0x7E04 开始。

Stage 2 的 GDT 定义：

```nasm
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
```

这些描述符在之前的文档中已经详细解释过了，这里不再赘述。

Stage 2 主函数：

```nasm
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
```

这部分和之前的 boot2.asm 基本一样，只是现在在同一个文件里。

Stage 2 的其他函数（print_string、load_kernel、setup_page_tables）和数据（msg_stage2、msg_loading 等）也类似，这里不重复了。

---

## 32 位和 64 位部分

文件的后半部分是 32 位和 64 位代码：

```nasm
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

这些代码和之前分开的文件完全一样，只是在同一个文件里而已。

---

## 合并后的布局

理解合并后的文件布局很重要：

```
偏移       内容              加载地址
────────────────────────────────────────
0x000      .mbr section     0x7C00    (Stage 1)
0x200      .stage2 section  0x7E00    (Stage 2)
```

`.mbr section` 的代码会被加载到 0x7C00（因为 `org 0x7C00`），`.stage2 section` 的代码会被加载到 0x7E00（因为 Stage 1 把它读到了那里）。`vstart=0x7E00` 确保标签从 0x7E00 开始计算，而不是从文件偏移。

编译命令很简单：

```bash
$ nasm -f bin boot/bootloader.asm -o build/bootloader.bin

$ ls -lh build/bootloader.bin
-rw-r--r-- 1 user user 1.4K Feb 17 00:20 build/bootloader.bin
```

---

## 更新 Makefile

现在我们只有一个 bootloader 文件，Makefile 需要相应更新：

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
	dd if=$(BOOTLOADER_BIN) of=$@ bs=512 count=3 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=3 conv=notrunc 2>/dev/null
	@echo "Boot image created: $@"
	@ls -lh $@
	@echo "========================================="
```

注意到我们不再分开编译 `boot.bin` 和 `boot2.bin`，而是直接编译 `bootloader.bin`。而且 `count=3` 因为我们知道 bootloader 大约 3 个扇区（实际大小需要在运行时检查）。

大小检查也需要更新：

```makefile
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
```

1536 字节是 3 个扇区，这是我们对 bootloader 大小的限制。如果超过了，内核会被推后，我们就要相应调整 `seek` 参数。

---

## 测试合并后的 bootloader

让我们来编译和测试：

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
Kernel:        20 / 512 bytes [OK]
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

成功！合并后的 bootloader 工作正常。

---

## 代码组织技巧

维护一个大文件不容易，这里有一些技巧让代码更易读。

首先是使用注释分隔不同部分：

```nasm
; ==============================================================================
; Stage 1: MBR (0x7C00)
; ==============================================================================

; ==============================================================================
; Stage 1 Functions
; ==============================================================================

; ==============================================================================
; Stage 1 Data
; ==============================================================================
```

这些分隔线让你快速找到代码的某个部分，而不需要滚动几百行。

其次是标签命名约定。保持一致的命名风格很重要：

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

`_entry` 后缀表示入口点，`_main` 后缀表示主函数，函数名用小写下划线分隔，数据标签用 `msg_` 前缀。

---

## 一些容易踩的坑

合并文件时有一些细节需要注意。

section 边界很重要。`section .mbr` 和 `section .stage2` 之间不要放任何代码，否则会被放到错误的 section。NASM 会把代码放到"当前"的 section，所以切换 section 时要小心。

`vstart` 参数要正确。`section .stage2 vstart=0x7E00` 确保标签从 0x7E00 开始计算，而不是从文件偏移。如果忘记 `vstart`，标签的值会是文件偏移，跳转会出错。

`bits` 切换不要忘记。每个 section 开头要设置正确的 `bits`：Stage 1 和 Stage 2 是 16 位，保护模式代码是 32 位，长模式代码是 64 位。如果 `bits` 设置错了，NASM 会生成错误的指令编码。

MBR 签名必须在偏移 510-511。`times 510-($-$$) db 0` 填充到 510 字节，`dw 0xAA55` 写入签名。确保你的填充计算正确，`$-$$` 是当前 section 的大小。我第一次做的时候把 `$$` 写成了 `$`，结果填充了 510 个 0，但签名被写到错误的位置，BIOS 无法识别。

---

## 总结

到这里，我们把 `boot.asm` 和 `boot2.asm` 合并成了 `bootloader.asm`，理解了 section 和 org/vstart 的区别，更新了 Makefile 以适应新的结构，验证了合并后的代码正常工作。现在我们只有一个 bootloader 文件，维护起来方便多了。

说实话，合并的过程虽然有点麻烦，但长远来看是值得的。单一的 bootloader 文件让你更容易理解整个启动流程，而不是在两个文件之间跳来跳去。而且编译一次比编译两次省事，makefile 也更简单。

下一步，我们会解决一个实际问题：当前只能加载 512 字节的内核，这太少了。我们需要支持大内核加载，这样才能写真正的操作系统功能。

**下篇预告**：《支持大内核加载》—— 从 512 字节到无限可能。


---

<div align="center">

## 文档导航

[← makefile自动构建省心省力](05_makefile自动构建省心省力.md)  | [支持大内核加载 →](07_支持大内核加载.md)

</div>
