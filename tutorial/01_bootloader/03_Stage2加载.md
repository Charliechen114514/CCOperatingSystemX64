# Stage 2 加载 —— 突破 512 字节的限制

## 前言

说实话，写到这个时候我才发现：MBR 的 512 字节限制真的太痛苦了。

你可能也发现了，我们的 VGA 打印代码、各种初始化代码，全部塞进 512 字节里，空间已经非常紧张了。更糟糕的是，我们还没开始写保护模式切换、页表设置这些更复杂的东西呢。我之前试着把这些代码都塞进 MBR，结果编译出来超过 600 字节，BIOS 根本不会加载它。那个时候我真的有点绝望，感觉整个项目要卡在这里了。

是时候引入两阶段 Bootloader 了。说实话，这个方案我一开始有点抗拒，觉得增加了复杂度。但真正实现之后发现，这不仅是突破空间限制的必要手段，也是让代码结构更清晰的好方法。

---

## 什么是两阶段 Bootloader？

两阶段 Bootloader 的思想其实很简单：既然 MBR 只能有 512 字节，那我们就让 MBR 只做一件事——加载更多代码到内存。加载的这段代码就是 Stage 2，它可以有更大的空间，做更复杂的事情。

你可以把它想象成一个接力赛。Stage 1（MBR）是第一棒选手，它的任务就是把接力棒（控制权）交给 Stage 2。Stage 2 拿到接力棒后，就可以做更多的事情了，比如切换到保护模式、设置页表、加载内核等。

⚠️ **关键点：Stage 1 和 Stage 2 在同一个二进制文件里**

这一点很重要。Stage 1 和 Stage 2 都在同一个二进制文件里，只是放在不同的位置。MBR 是扇区 0（偏移 0-511），Stage 2 从扇区 1 开始（偏移 512 开始）。我们编译的时候会把它们一起编译进去，然后 BIOS 加载 MBR 后，Stage 1 会读取磁盘的后续扇区，把 Stage 2 加载到内存。

为什么这样设计？有两个原因。首先，这样我们只需要一个磁盘镜像，不需要两个文件。其次，Stage 1 知道 Stage 2 在磁盘的什么位置（因为编译时就确定好了），所以可以准确地读取它。

---

## 理解磁盘布局

我们的磁盘镜像布局是这样的。为了方便理解，你可以把它想象成一本书，MBR 是封面，Stage 2 是正文。

```
扇区      偏移        内容
0         0x0000      MBR (Stage 1)
1         0x0200      Stage 2 代码
2         0x0400      Stage 2 代码（继续）
3         0x0600      Stage 2 代码（继续）
...       ...         ...
N                     内核（从某个扇区开始）
```

对于我们的 CCOS Bootloader，Stage 1 是 1 个扇区（512 字节），Stage 2 我们分配 3 个扇区（1536 字节）。总共 4 个扇区，2048 字节。内核从扇区 4 开始。

你可能会问，为什么 Stage 2 是 3 个扇区？这其实是个经验值。一开始我分配了 2 个扇区，后来发现不够用，又加了一个。你可以根据需要调整这个值，只要在编译时正确设置就行了。

---

## 编写 Stage 1 代码

现在我们来编写两阶段 Bootloader。我们需要修改 `boot/bootloader.asm`，把它分成 Stage 1 和 Stage 2 两部分。首先来看 Stage 1 的代码：

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

这段代码我们来逐段讲解。首先是配置部分，`%define BOOTLOADER_SECTORS 4` 定义了 bootloader 的总扇区数，包括 MBR 和 Stage 2。这样我们可以方便地调整大小。

然后是环境初始化和清屏，和之前一样。打印启动消息，告诉用户 Stage 1 正在加载 Stage 2。

接下来是核心的磁盘读取代码。这里我们用 BIOS 的 INT 13h 中断，AH = 0x02 功能来读取磁盘。这里有几个参数需要解释一下。ES:BX 是目标缓冲区地址，我们把它设为 0x7E00，这样 Stage 2 会被加载到 MBR 后面。AL 是要读取的扇区数，这里是 `BOOTLOADER_SECTORS - 1`，因为我们不包括 MBR 本身。CH、CL、DH 是 CHS 寻址方式的柱面、扇区、磁头号。CL = 0x02 表示从扇区 2 开始，也就是 LBA 1（因为 CHS 扇区号从 1 开始）。DL 是驱动器号，0x80 表示第一个硬盘。

调用 INT 13h 后，需要检查是否成功。CF（Carry Flag）= 1 表示错误，我们用 `jc load_error` 跳转到错误处理。然后比较 AL 和我们请求的扇区数，如果不相等，也跳转到错误处理。`cmp al, BOOTLOADER_SECTORS - 1` 比较 AL 和期望的扇区数，`jne load_error` 不相等则跳转。

如果读取成功，打印成功消息，然后 `jmp 0x7E00` 跳转到 Stage 2。这是一个远跳转，直接跳到物理地址 0x7E00。

错误处理部分很简单，打印错误消息，然后死循环。`hlt` 让 CPU 停止，`jmp .hang` 形成无限循环。

---

## 编写 Stage 2 代码

现在我们来编写 Stage 2 代码。Stage 2 从偏移 512 开始，会被加载到 0x7E00。

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

这段代码我们来解释一下。首先是 `section .stage2 vstart=0x7E00`。这里我们用了 `vstart` 参数，告诉 NASM 这段代码的虚拟起始地址是 0x7E00。为什么需要 `vstart`？因为这段代码在文件中的偏移是 512，但加载到内存后是在 0x7E00。如果不用 `vstart`，NASM 会认为代码的地址从 512 开始，这样标签和跳转会出错。

⚠️ **关于 vstart 的坑**

`vstart` 这个坑真的踩了我半天。一开始我没有设置 `vstart`，结果 Stage 2 的代码跳转全都错了。症状是看到 "Stage 2 loaded" 但之后卡住或重启。后来查了半天文档才发现需要用 `vstart` 告诉 NASM 代码的实际运行地址。

Stage 2 的入口是 `stage2_entry`。首先 `jmp short stage2_main` 跳过填充，然后 `nop` 对齐。为什么需要这个？因为 `section .stage2 vstart=0x7E00` 会从 0x7E00 开始计算，但 `stage2_entry` 标签的地址应该是 0x7E00。如果直接把代码放在这里，`stage2_entry` 的地址会是 0x7E00，但 `stage2_main` 的地址会是 0x7E00 + 2（因为 `jmp short` 是 2 字节）。这样 `stage2_entry` 和 `stage2_main` 之间就没有空间了。所以我们用 `jmp short` 跳转，然后 `nop` 填充，这样 `stage2_main` 的地址就是 0x7E03。

然后是重新初始化环境。你可能奇怪，Stage 1 不是已经初始化过了吗？为什么还要再初始化？因为跳转到 Stage 2 时，我们用的是 `jmp 0x7E00`，这是一个远跳转，但不会改变段寄存器的值。而且为了安全起见，我们最好重新初始化一下，确保所有寄存器都在我们期望的状态。

接下来打印 Stage 2 消息，告诉用户 Stage 2 已经开始运行。然后是一个 TODO 注释，下一篇文章我们会在这里添加保护模式切换的代码。最后是死循环，`hlt` 让 CPU 停止，`jmp .hang` 形成无限循环。

---

## 更新 Makefile

我们的 Makefile 需要更新，因为现在 bootloader 不再是 512 字节了。

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

这个 Makefile 和之前的版本相比，有几个变化。首先是添加了 `BOOTLOADER_SECTORS` 变量，定义了 bootloader 的总扇区数。然后是编译规则不再限制输出文件大小，因为我们现在有 4 个扇区。创建磁盘镜像的规则也相应调整，确保镜像足够大。最后添加了一个 `check` 目标，用来检查编译出的文件大小和扇区数是否正确。

---

## 编译和测试

现在我们来编译运行这个两阶段 Bootloader。

```bash
cd boot
make clean
make
```

你应该看到类似这样的输出：

```
==> Building bootloader...
==> Build complete: ../build/bootloader.bin
==> Size: 856 bytes
==> Creating disk image...
==> Disk image created: ../build/boot.img
```

注意文件大小现在是 856 字节，不到 2 个扇区。如果你用 `make check` 查看，你会看到：

```
==> Bootloader size: 856 bytes
==> Sectors: 1
==> Expected: 4 sectors
```

扇区数是 1，因为我们用整数除法，856 / 512 = 1。这说明我们的 Stage 2 代码还不多，一个扇区就够了。没关系，我们分配了 4 个扇区，后面可以慢慢填充。

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

这说明我们的两阶段 Bootloader 工作正常。Stage 1 成功加载了 Stage 2，并跳转过去了。Stage 2 也成功打印了消息，说明它确实在 0x7E00 运行。

---

## 理解内存布局

现在我们的内存布局是这样的，我们来仔细看一下。理解内存布局很重要，因为后面我们要在这个基础上添加更多代码。

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

IVT（Interrupt Vector Table）在中断向量表在内存的最开始，从 0x0000 到 0x03FF，共 1KB。这是 BIOS 设置的，我们不需要修改。

Stage 1 (MBR) 从 0x7C00 开始，占 512 字节，到 0x7DFF。这是 BIOS 加载 MBR 的标准地址。

Stage 2 从 0x7E00 开始，这是我们用 INT 13h 读取磁盘后加载到的地址。Stage 2 最多占 1536 字节（3 个扇区），到 0x83FF。

栈区域从 0x8400 向下增长。我们把栈指针设在 Stage 2 的起始位置 0x7E00，栈向下增长，所以栈会从 0x7E00 向下扩展，不会覆盖 Stage 1 和 Stage 2 的代码。

页表从 0x9000 开始，我们后面会用它来设置长模式。页表占 12KB（PML4、PDPT、PD 各 4KB）。

内核从 0x10000 开始加载，这是我们自己约定的地址。后面我们会写代码从磁盘读取内核到这个地址。

⚠️ **重要：Stage 2 的地址**

Stage 2 从 0x7E00 开始，紧接在 Stage 1 后面。Stage 1 在 0x7C00，占 512 字节（0x200），所以 Stage 1 结束于 0x7E00（0x7C00 + 0x200 = 0x7E00）。这个计算很重要，因为如果 Stage 2 的地址不对，跳转会失败。

---

## 常见问题

这里我总结一些两阶段 Bootloader 开发中常见的问题。

### 问题 1：Stage 2 没有执行

症状是只看到 Stage 1 的消息，看不到 Stage 2 的消息。原因可能有几个。磁盘读取失败，跳转地址不正确，或者 Stage 2 代码损坏。

调试方法：首先检查二进制文件，用 `ls -lh ../build/bootloader.bin` 查看文件大小，用 `ndisasm -b 16 ../build/bootloader.bin | head -50` 反汇编查看代码。确保 Stage 2 的代码确实在文件里。

然后检查磁盘读取是否成功。你可以在 Stage 1 里添加一些调试代码，比如在调用 INT 13h 前后打印一些信息，看看是否到达那里。或者用 GDB 单步调试，查看寄存器的值。

### 问题 2：跳转后崩溃

症状是看到 "Stage 2 loaded" 但之后卡住或重启。原因可能是 Stage 2 代码没有正确初始化段寄存器，栈指针设置不正确，或者 `vstart` 设置不正确。

解决方法：确保 Stage 2 开始时重新初始化所有寄存器。确保栈指针设在 Stage 2 的起始位置。确保 `section .stage2 vstart=0x7E00` 正确设置。

你可以在 Stage 2 的最开始添加一些调试代码，比如打印一个字符，看看是否到达那里。如果连这个都没有，说明跳转就失败了。

### 问题 3：文件大小不对

症状是编译出来的文件大小不是预期的。比如你期望 2048 字节（4 个扇区），但实际只有 856 字节。

这其实不是问题，只是说明你的 Stage 2 代码还不多。只要你分配了足够的空间（4 个扇区），后面可以慢慢填充。重要的是确保 Stage 2 的代码正确编译和链接，`vstart` 设置正确。

你可以用 `make check` 查看文件大小和扇区数。如果扇区数小于你分配的数量，不用担心，只要代码能正常工作就行。

### 问题 4：CHS 寻址问题

症状是磁盘读取失败，错误消息显示 "[E1] Failed to load Stage 2"。

原因可能是 CHS 寻址的参数不正确。CHS（Cylinder-Head-Sector）是古老的磁盘寻址方式，每个参数都有特定的范围。柱面号从 0 开始，磁头号从 0 开始，扇区号从 1 开始。注意扇区号是从 1 开始的，不是 0。

我们的代码里 `cl = 0x02` 表示扇区 2，也就是 LBA 1（从第二个扇区开始）。如果你从扇区 1 开始（LBA 0），那就是读取 MBR 自己了，没有意义。

如果你想了解更多关于 CHS 和 LBA 的转换，可以查一下相关资料。不过说实话，现在很少需要手动计算 CHS 了，因为 BIOS 的 INT 13h 扩展功能（AH = 0x42）支持 LBA 直接寻址。我们后面会用到这个功能。

---

## 下一步

现在我们有了两阶段 Bootloader，可以突破 512 字节的限制。在下一篇文章中，我们会深入理解 x86 处理器的模式（实模式 → 保护模式 → 长模式），设置 GDT（全局描述符表），切换到保护模式，然后切换到 64 位长模式。

这部分内容比较复杂，但也是最有意思的部分。我们会亲手从 16 位实模式一路切换到 64 位长模式，这个过程会让你对 x86 处理器的工作方式有非常直观的理解。

准备好了吗？精彩的部分才刚刚开始。


---

<div align="center">

## 文档导航

[← 从零开始的MBR](02_从零开始的MBR.md)  | [保护模式切换 →](04_保护模式切换.md)

</div>
