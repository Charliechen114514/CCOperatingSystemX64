# bootloader 也要分两步走

## 前言：512 字节真的不够用

如果你跟着上一篇做了，现在应该有一个可爱的 20 字节内核。但问题来了 —— 怎么加载它？

你可能会想："直接把所有代码塞进 MBR 不就行了吗？"

朋友，我当年也是这么想的，直到我被现实狠狠地打了一巴掌。

### MBR 的硬伤

MBR（Master Boot Record）只有 512 字节，这其中：
- 最后 6 字节是磁盘签名（`0x55AA`）
- 最多 446 字节可用于代码
- 446 字节大概也就 70-80 条汇编指令

这点空间够干什么？
- ✅ 初始化 CPU
- ✅ 从磁盘读取更多数据
- ❌ 加载内核
- ❌ 设置页表
- ❌ 切换到长模式

### 解决方案：分阶段加载

既然一次装不下，那就分两次 —— 这就是 Stage 1 和 Stage 2 的由来：

```
Stage 1 (MBR)        → 读取 Stage 2 到内存
    ↓
Stage 2 (0x7E00)    → 完成复杂操作，加载内核
    ↓
Kernel (0x10000)    → 执行操作系统
```

今天我们就来实现这个方案。

---

## 第一步：理解磁盘布局

在写代码之前，我们先规划一下磁盘上各个部分的位置：

```
扇区     内容           大小      加载地址
────────────────────────────────────────────
Sector 0  Stage 1 (MBR)  512B     0x7C00
Sector 1  Stage 2 (部分)  512B     0x7E00
Sector 2  Stage 2 (部分)  512B     0x8000
Sector 3  Kernel         512B     0x10000
...
```

### 为什么 Stage 2 占用 2 个扇区？

因为我们的 Stage 2 代码最终会长到大约 800-900 字节，一个扇区（512 字节）装不下。

### 为什么 Kernel 从 Sector 3 开始？

Sector 0 是 MBR，Sector 1-2 是 Stage 2，那么 Kernel 自然就从 Sector 3 开始了（LBA 3）。

---

## 第二步：实现 Stage 1 (MBR)

Stage 1 的职责很简单：
1. 初始化 CPU
2. 从磁盘读取 Stage 2
3. 跳转到 Stage 2

创建文件 `boot/boot.asm`：

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

    ; 清屏
    mov ah, 0x00
    mov al, 0x03
    int 0x10

    ; 打印欢迎消息
    mov si, welcome_msg
    call print_string

    ; 加载 Stage 2
    call load_stage2

    ; 跳转到 Stage 2
    jmp 0x7E00

    ; 永远不会到这里
.hang:
    hlt
    jmp .hang

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
    cmp bx, 0xAA55
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

### 代码解析

#### DAP 结构

DAP（Disk Address Packet）是 LBA 扩展读取使用的数据结构：

```nasm
偏移    大小    内容
────────────────────────────
0       1       数据包大小（16）
1       1       保留（0）
2-3     2       要读取的扇区数
4-5     2       目标缓冲区偏移
6-7     2       目标缓冲区段
8-15    8       起始 LBA（64 位）
```

这个结构必须 16 字节对齐，所以我们要用 `align 4`。

#### LBA vs CHS

**LBA（Logical Block Addressing）**：
- 现代、简单、支持大磁盘
- 扇区从 0 开始编号
- 需要 DAP 结构

**CHS（Cylinder-Head-Sector）**：
- 传统方式，兼容性最好
- 扇区从 1 开始编号
- 参数复杂

我们的代码先尝试 LBA，失败后回退到 CHS。

---

## 第三步：实现 Stage 2

Stage 2 的职责：
1. 打印消息证明到达
2. （下一篇）切换到保护模式和长模式
3. （下一篇）加载并跳转到内核

创建文件 `boot/boot2.asm`：

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

### 为什么有个 `nop`？

在 `jmp short main` 后面放一个 `nop` 是为了确保 GDT 从对齐的地址开始。这不是严格必需的，但好习惯。

---

## 第四步：编译和合并

现在我们有了两个文件，需要编译它们并合并到一个磁盘镜像中。

### 编译

```bash
# 编译 Stage 1
$ nasm -f bin boot/boot.asm -o build/boot.bin

$ ls -lh build/boot.bin
-rw-r--r-- 1 user user 512 Feb 16 23:45 build/boot.bin

# 编译 Stage 2
$ nasm -f bin boot/boot2.asm -o build/boot2.bin

$ ls -lh build/boot2.bin
-rw-r--r-- 1 user user 146 Feb 16 23:45 build/boot2.bin
```

### 合并

使用 `dd` 命令把它们合并：

```bash
# 创建磁盘镜像
$ dd if=build/boot.bin of=build/boot.img bs=512 count=1
$ dd if=build/boot2.bin of=build/boot.img bs=512 seek=1 conv=notrunc

# 验证
$ ls -lh build/boot.img
-rw-r--r-- 1 user user 1.0K Feb 16 23:46 build/boot.img

$ xxd build/boot.img | head -20
00000000: eb3e 0000 0000 0000 0000 0000 0000 0000  .>..............
00000010: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000100: eb09 0000 0000 0000 0000 0000 0000 0000  ................
```

---

## 第五步：测试

现在我们可以测试了：

```bash
$ qemu-system-x86_64 -drive format=raw,file=build/boot.img -nographic
```

你应该能看到：

```
[1] Stage 1: Loading Stage 2...
[2] Stage 2: Running! (next: load kernel)
```

然后系统就停住了（hlt 指令）。

### 验证磁盘内容

让我们验证一下 Stage 2 确实在正确的位置：

```bash
# 提取 Stage 2（从扇区 1 开始，LBA 1）
$ dd if=build/boot.img bs=512 skip=1 count=1 | xxd | head -5
00000000: eb09 90...               ; jmp short main, nop
```

完美！

---

## 第六步：添加内核加载函数

现在 Stage 2 已经能运行了，我们添加内核加载功能。

### 修改 boot2.asm

在 `main:` 函数的 `print_string` 之后添加：

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

### 添加 load_kernel 函数

在 `print_string` 函数后面添加：

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

### 添加消息

在数据区添加：

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

现在我们需要把内核也合并到磁盘镜像中：

```bash
# 编译内核（使用上篇的 kernel.asm）
$ nasm -f bin kernel/kernel.asm -o build/kernel.bin

# 合并到磁盘镜像
$ dd if=build/kernel.bin of=build/boot.img bs=512 seek=3 conv=notrunc

# 验证
$ ls -lh build/boot.img
-rw-r--r-- 1 user user 1.5K Feb 16 23:50 build/boot.img

# 验证内核在正确位置
$ dd if=build/boot.img bs=512 skip=3 count=1 | xxd | head -5
00000000: 48bf 0050 b800 0000 48b8 581f 0000  H..P....H.X...
```

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

然后系统停住。

### 为什么没有跳转到内核？

因为我们还没添加跳转代码！现在内核已经加载到内存了，但我们在 Stage 2 里只是停机，没有跳过去。

这将是下一篇的内容 —— 从 16 位实模式切换到 64 位长模式，然后跳转到内核。

---

## 踩坑预警

⚠️ **注意**：扇区编号的 0-based vs 1-based

- LBA 是 0-based：扇区 0, 1, 2, 3...
- CHS 是 1-based：扇区 1, 2, 3, 4...

所以：
- MBR 在 LBA 0，CHS 扇区 1
- Stage 2 在 LBA 1-2，CHS 扇区 2-3
- Kernel 在 LBA 3，CHS 扇区 4

千万别搞混了！

⚠️ **注意**：dd 的 seek 参数

`seek=3` 意味着"跳过前 3 个块（512 字节）"，所以数据从第 4 个块开始写。这正好对应 LBA 3。

⚠️ **注意**：ES:BX 地址计算

`0x1000:0x0000` = `0x1000 * 16 + 0x0000` = `0x10000`。这是实模式的段:偏移地址计算方式。

---

## 总结

到这里，我们：

1. ✅ 理解了为什么要分 Stage 1 和 Stage 2
2. ✅ 实现了 Stage 1（MBR）加载 Stage 2
3. ✅ 实现了 Stage 2 加载内核
4. ✅ 理解了 LBA vs CHS 两种磁盘读取方式
5. ❌ 还没切换到长模式
6. ❌ 还没跳转到内核

下一步，我们会进入 x86 处理器最复杂的部分 —— 从 16 位实模式切换到 64 位长模式。这涉及到 GDT、页表、CR0/CR4/EFER 寄存器等等。准备好了吗？

**下篇预告**：《从 16 位到 64 位的漫长旅途》—— Intel 的历史包袱，我们一个一个背。


---

<div align="center">

## 文档导航

[← 先搞个最简单的内核试试](01_先搞个最简单的内核试试.md)  | [从16位到64位的漫长旅途 →](03_从16位到64位的漫长旅途.md)

</div>
