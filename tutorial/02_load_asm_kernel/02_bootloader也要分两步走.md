# bootloader 也要分两步走

## 前言：512 字节真的不够用

如果你跟着上一篇做了，现在应该有一个可爱的 20 字节内核。但问题来了 —— 怎么加载它？你可能会想："直接把所有代码塞进 MBR 不就行了吗？"

朋友，我当年也是这么想的，直到我被现实狠狠地打了一巴掌。MBR（Master Boot Record）只有 512 字节，最后 6 字节还是磁盘签名 `0x55AA`，最多也就 446 字节可用于代码，这点空间大概也就 70-80 条汇编指令。初始化 CPU 和从磁盘读取更多数据还行，但要加载内核、设置页表、切换到长模式？想都别想。

既然一次装不下，那就分两次 —— 这就是 Stage 1 和 Stage 2 的由来。Stage 1 在 MBR 里，负责读取 Stage 2 到内存；Stage 2 在 0x7E00，负责完成复杂操作并加载内核；最后跳转到位于 0x10000 的内核执行操作系统。今天我们就来实现这个方案。

---

## 第一步：先规划一下磁盘布局

在写代码之前，我们先规划一下磁盘上各个部分的位置。Sector 0 是我们的 Stage 1 MBR，会被 BIOS 加载到 0x7C00；Sector 1 和 2 是 Stage 2 的内容，会被加载到 0x7E00 开始的连续内存；Sector 3 开始就是我们的内核了，会被加载到 0x10000。

你可能会问，为什么 Stage 2 要占用 2 个扇区？因为我们的 Stage 2 代码最终会长到大约 800-900 字节，一个扇区 512 字节根本装不下。至于为什么内核从 Sector 3 开始，算一下就知道了：Sector 0 是 MBR，Sector 1-2 是 Stage 2，那么内核自然就从 Sector 3 开始了，这对应 LBA 3 或者 CHS 扇区 4。

这里有个坑千万别踩 —— LBA 是 0-based 的，扇区从 0 开始数；而 CHS 是 1-based 的，扇区从 1 开始数。所以 MBR 在 LBA 0、CHS 扇区 1，Stage 2 在 LBA 1-2、CHS 扇区 2-3，内核在 LBA 3、CHS 扇区 4。搞混了就会读错位置，然后系统就起不来了。

---

## 第二步：写一个最简单的 Stage 1

Stage 1 的职责很简单：初始化 CPU，从磁盘读取 Stage 2，然后跳转到 Stage 2。创建一个文件 `boot/boot.asm`，我们先从最基础的部分开始：

```nasm
; ==============================================================================
; CCOS Bootloader - Stage 1 (MBR)
; ==============================================================================
; 职责：
;   1. 初始化 CPU（设置段寄存器、栈）
;   2. 从磁盘读取 Stage 2 到 0x7E00
;   3. 跳转到 Stage 2
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

这段代码在做最基本的初始化。`cli` 关闭中断，防止在初始化过程中被打断。然后把所有段寄存器都设为 0，这是实模式的标准做法。栈指针设为 0x7C00，正好是 MBR 的开始位置，这样栈会向下增长不会覆盖我们的代码。`sti` 重新开启中断。

接下来我们添加清屏和打印欢迎消息的功能，这样我们知道 Stage 1 确实在运行：

```nasm
    ; 清屏
    mov ah, 0x00
    mov al, 0x03
    int 0x10

    ; 打印欢迎消息
    mov si, welcome_msg
    call print_string
```

`int 0x10` 是 BIOS 的视频中断，功能 00h 是设置视频模式，03h 是标准的 80x25 文本模式。然后我们调用 `print_string` 函数来打印消息，这个函数稍后实现。

现在来实现磁盘读取功能。我们先写一个简单的字符串打印函数：

```nasm
; ============================================================================
; 函数：print_string
; 输入：SI = 字符串地址
; ============================================================================
print_string:
    pusha
.loop:
    lodsb                   ; 加载 [SI] 到 AL，SI++
    test al, al
    jz .done
    mov ah, 0x0E            ; BIOS teletype 功能
    int 0x10
    jmp .loop
.done:
    popa
    ret
```

`lodsb` 指令把 DS:SI 指向的字节加载到 AL，然后 SI 自动加 1。`test al, al` 检查是否到达字符串末尾（0）。`int 0x10` 的功能 0Eh 是 teletype 输出，会在当前光标位置打印一个字符并自动前进光标。

接下来是最重要的部分 —— 从磁盘读取 Stage 2。我们用 LBA 扩展读取，因为这是现代标准，但如果不支持就回退到 CHS：

```nasm
; ============================================================================
; 函数：load_stage2
; 从磁盘读取 Stage 2 到 0x7E00
; ============================================================================
load_stage2:
    pusha

    ; 设置目标地址：0x7E00
    mov ax, 0x7E0
    mov es, ax
    xor bx, bx              ; ES:BX = 0x7E0:0x0000 = 0x7E00

    ; 尝试 LBA 扩展读取
    call check_lba_support
    jc .try_chs             ; LBA 不支持，尝试 CHS

    ; LBA 读取
    mov si, dap
    mov ah, 0x42            ; 扩展读取功能
    mov dl, 0x80            ; 第一个硬盘
    int 0x13
    jc .try_chs             ; 失败，尝试 CHS

    cmp al, 0x02            ; 检查是否读取了 2 个扇区
    jne .try_chs

    popa
    ret

.try_chs:
    ; CHS 读取（传统方式）
    mov ax, 0x7E0
    mov es, ax
    xor bx, bx

    mov ah, 0x02            ; 读取功能
    mov al, 0x02            ; 读取 2 个扇区
    mov ch, 0x00            ; 柱面 0
    mov cl, 0x02            ; 扇区 2（1-based，MBR 是扇区 1）
    mov dh, 0x00            ; 磁头 0
    mov dl, 0x80            ; 第一个硬盘
    int 0x13

    jc disk_error
    cmp al, 0x02            ; 验证读取了 2 个扇区
    jne disk_error

    popa
    ret

disk_error:
    mov si, error_msg
    call print_string
.hang:
    hlt
    jmp .hang
```

LBA 扩展读取需要一个 DAP（Disk Address Packet）结构，它告诉 BIOS 要读取多少扇区、从哪个 LBA 开始、数据放到哪里。DAP 结构有 16 字节：第一个字节是结构大小（固定 16），第二个字节保留，然后两个字节是要读取的扇区数，两个字节是目标缓冲区偏移，两个字节是目标缓冲区段，最后 8 个字节是起始 LBA。这个结构必须 16 字节对齐，所以我们用 `align 4` 来确保。

CHS 读取是传统方式，参数多但简单。柱面号放在 CH，磁头号放在 DH，扇区号放在 CL（低 5 位，高 2 位是柱面号的高位）。注意 CHS 的扇区号是 1-based，所以 MBR 是扇区 1，Stage 2 从扇区 2 开始。

我们还需要检查 BIOS 是否支持 LBA 扩展读取：

```nasm
; ============================================================================
; 函数：check_lba_support
; 检查 BIOS 是否支持 LBA 扩展读取
; 输出：CF=0 支持，CF=1 不支持
; ============================================================================
check_lba_support:
    pusha
    mov dl, 0x80
    mov ah, 0x41
    mov bx, 0x55AA          ; 魔术值
    int 0x13

    jc .not_support
    cmp bx, 0xAA55          ; BIOS 应该返回反转的魔术值
    jne .not_support
    test cx, 0x01           ; 检查 LBA 扩展位
    jz .not_support

    popa
    clc                     ; 清除进位标志 = 支持
    ret

.not_support:
    popa
    stc                     ; 设置进位标志 = 不支持
    ret
```

最后我们添加数据区和 MBR 签名：

```nasm
; ============================================================================
; 数据区
; ============================================================================
welcome_msg:
    db "[1] Stage 1: Loading Stage 2...", 0x0D, 0x0A, 0

error_msg:
    db "[E] Failed to load Stage 2", 0x0D, 0x0A, 0

; DAP (Disk Address Packet) 用于 LBA 扩展读取
align 4
dap:
    db 16                   ; 数据包大小（16 字节）
    db 0                    ; 保留
    dw 0x0002               ; 要读取的扇区数（2）
    dw 0x0000               ; 目标偏移
    dw 0x7E0                ; 目标段
    dq 1                    ; 起始 LBA（扇区 1，0-based）

; 填充到 510 字节
times 510-($-$$) db 0

; MBR 签名
dw 0xAA55
```

`0x0D, 0x0A` 是回车换行符。`times 510-($-$$) db 0` 会填充 0 直到文件大小为 510 字节，然后我们写上 2 字节的 MBR 签名 `0xAA55`（注意是小端序，所以文件里是 `55 AA`）。

---

## 第三步：写一个最简单的 Stage 2

Stage 2 的职责是打印消息证明成功加载，然后等待下一步的扩展。创建文件 `boot/boot2.asm`：

```nasm
; ==============================================================================
; CCOS Bootloader - Stage 2
; ==============================================================================
; 职责：
;   1. 打印消息证明成功加载
;   2. （下一步）切换到长模式
;   3. （下一步）加载并跳转到内核
; ==============================================================================

section .text
    org 0x7E00
    bits 16

start:
    jmp short main
    nop                     ; 填充，让 GDT 从 0x7E04 开始
```

`jmp short main` 跳过下面的 GDT，`nop` 是为了对齐。GDT（全局描述符表）下一篇会详细讲，现在先预留位置：

```nasm
; ============================================================================
; GDT（全局描述符表）- 下篇会详细解释
; ============================================================================
gdt_start:
    dq 0x0000000000000000   ; 空描述符
gdt_code:
    dq 0x00CF9A000000FFFF   ; 32 位代码段
gdt_data:
    dq 0x00CF92000000FFFF   ; 32 位数据段
gdt_code64:
    dq 0x00AF9A000000FFFF   ; 64 位代码段
gdt_data64:
    dq 0x00CF92000000FFFF   ; 64 位数据段
gdt_end:

gdt_ptr:
    dw 5 * 8 - 1            ; GDT 限长
    dd 0x00007E03           ; GDT 基地址
```

然后是主函数和打印函数：

```nasm
; ============================================================================
; 主函数
; ============================================================================
main:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7E00

    ; 打印消息
    mov si, msg_stage2
    call print_string

    ; 暂时停在这里
.hang:
    hlt
    jmp .hang

; ============================================================================
; 函数：print_string（使用 BIOS）
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

; ============================================================================
; 数据区
; ============================================================================
msg_stage2:
    db "[2] Stage 2: Running! (next: load kernel)", 0x0D, 0x0A, 0
```

---

## 第四步：编译和合并

现在我们有了两个文件，需要编译它们并合并到一个磁盘镜像中。先编译 Stage 1：

```bash
$ nasm -f bin boot/boot.asm -o build/boot.bin

$ ls -lh build/boot.bin
-rw-r--r-- 1 user user 512 Feb 16 23:45 build/boot.bin
```

正好 512 字节！这是 MBR 的标准大小。然后编译 Stage 2：

```bash
$ nasm -f bin boot/boot2.asm -o build/boot2.bin

$ ls -lh build/boot2.bin
-rw-r--r-- 1 user user 146 Feb 16 23:45 build/boot2.bin
```

现在用 `dd` 命令把它们合并到一个磁盘镜像：

```bash
$ dd if=build/boot.bin of=build/boot.img bs=512 count=1
$ dd if=build/boot2.bin of=build/boot.img bs=512 seek=1 conv=notrunc

$ ls -lh build/boot.img
-rw-r--r-- 1 user user 1.0K Feb 16 23:46 build/boot.img
```

`dd` 的参数需要解释一下：`bs=512` 设置块大小为 512 字节（一个扇区），`count=1` 只复制 1 个块，`seek=1` 在输出文件中跳过前 1 个块再开始写，`conv=notrunc` 表示不要截断输出文件。所以第一条命令把 boot.bin 写入 boot.img 的开头，第二条命令把 boot2.bin 写入 boot.img 的第二个扇区开始的位置。

让我们验证一下 Stage 2 确实在正确的位置：

```bash
$ xxd build/boot.img | head -20
00000000: eb3e 0000 0000 0000 0000 0000 0000 0000  .>..............
00000010: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000100: eb09 90...               ; jmp short main, nop
```

可以看到 0x00000100 位置是 `eb09 90`，这正是 `jmp short main`（`eb09`）和 `nop`（`90`）的机器码！Stage 2 确实在正确的位置。

---

## 第五步：测试一下

现在我们可以测试了：

```bash
$ qemu-system-x86_64 -drive format=raw,file=build/boot.img -nographic
```

你应该能看到：

```
[1] Stage 1: Loading Stage 2...
[2] Stage 2: Running! (next: load kernel)
```

然后系统就停住了，因为我们在 Stage 2 里写了个死循环 `hlt`。但至少证明 Stage 1 成功加载了 Stage 2！

---

## 第六步：让 Stage 2 加载内核

现在 Stage 2 已经能运行了，我们来添加内核加载功能。首先在 `main:` 函数的 `print_string` 之后添加加载内核的代码：

```nasm
main:
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

    ; 打印成功消息
    mov si, msg_kernel_loaded
    call print_string

    ; （下一步）切换到长模式...

.hang:
    hlt
    jmp .hang

load_error:
    mov si, msg_error
    call print_string
.hang:
    hlt
    jmp .hang
```

然后在 `print_string` 函数后面添加 `load_kernel` 函数：

```nasm
; ============================================================================
; 函数：load_kernel
; 从磁盘加载内核到 0x10000
; 输出：CF=0 成功，CF=1 失败
; ============================================================================
load_kernel:
    pusha

    ; 打印加载消息
    mov si, msg_loading
    call print_string

    ; 设置目标地址：0x10000
    mov ax, 0x1000
    mov es, ax
    xor bx, bx              ; ES:BX = 0x1000:0x0000 = 0x10000

    ; 使用 CHS 读取内核（在 Sector 4）
    mov ah, 0x02            ; 读取功能
    mov al, 0x01            ; 读取 1 个扇区
    mov ch, 0x00            ; 柱面 0
    mov cl, 0x04            ; 扇区 4（1-based）
    mov dh, 0x00            ; 磁头 0
    mov dl, 0x80            ; 第一个硬盘
    int 0x13

    jc .error
    cmp al, 0x01            ; 验证读取了 1 个扇区
    jne .error

    popa
    clc                     ; 清除进位 = 成功
    ret

.error:
    popa
    stc                     ; 设置进位 = 失败
    ret
```

在数据区添加新的消息：

```nasm
msg_loading:
    db "[LOAD] Loading kernel...", 0x0D, 0x0A, 0

msg_kernel_loaded:
    db "[OK] Kernel loaded at 0x10000", 0x0D, 0x0A, 0

msg_error:
    db "[ERR] Failed to load kernel", 0x0D, 0x0A, 0
```

---

## 第七步：把内核也加进去

现在我们需要把内核也合并到磁盘镜像中。首先编译内核（使用上篇的 kernel.asm）：

```bash
$ nasm -f bin kernel/kernel.asm -o build/kernel.bin

$ ls -lh build/kernel.bin
-rw-r--r-- 1 user user 20 Feb 16 23:50 build/kernel.bin
```

然后合并到磁盘镜像：

```bash
$ dd if=build/kernel.bin of=build/boot.img bs=512 seek=3 conv=notrunc

$ ls -lh build/boot.img
-rw-r--r-- 1 user user 1.5K Feb 16 23:50 build/boot.img
```

验证内核在正确位置：

```bash
$ dd if=build/boot.img bs=512 skip=3 count=1 | xxd | head -5
00000000: 48bf 0050 b800 0000 48b8 581f 0000  H..P....H.X...
```

`48bf 0050 b800 0000` 正是 `mov rdi, 0xB8000 + 160 * 5` 的机器码的一部分！内核确实在正确的位置。

---

## 第八步：再次测试

```bash
$ qemu-system-x86_64 -drive format=raw,file=build/boot.img -nographic
```

你应该能看到：

```
[1] Stage 1: Loading Stage 2...
[2] Stage 2: Running! (next: load kernel)
[LOAD] Loading kernel...
[OK] Kernel loaded at 0x10000
```

然后系统停住。等等，内核已经加载到内存了，为什么没有执行？因为我们还没添加跳转代码！现在内核在 0x10000，但我们在 Stage 2 里只是停机，没有跳过去。

这将是下一篇的内容 —— 从 16 位实模式切换到 64 位长模式，然后跳转到内核。老实说这是 x86 最复杂的一部分，涉及到 GDT、页表、CR0/CR4/EFER 寄存器等等。做好准备！

到这里，我们已经理解了为什么要分 Stage 1 和 Stage 2，实现了 Stage 1 加载 Stage 2，实现了 Stage 2 加载内核，也理解了 LBA vs CHS 两种磁盘读取方式。下一步我们会进入 Intel 的历史包袱 —— 从 16 位实模式切换到 64 位长模式。

**下篇预告**：《从 16 位到 64 位的漫长旅途》—— Intel 的历史包袱，我们一个一个背。


---

<div align="center">

## 文档导航

[← 先搞个最简单的内核试试](01_先搞个最简单的内核试试.md)  | [从16位到64位的漫长旅途 →](03_从16位到64位的漫长旅途.md)

</div>
