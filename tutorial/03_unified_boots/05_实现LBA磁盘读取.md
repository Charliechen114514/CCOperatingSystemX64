# 实现 LBA 磁盘读取

说实话，CHS 这种古老的寻址方式早该淘汰了。

---

## LBA vs CHS

### CHS（柱面-磁头-扇区）

CHS 是老式硬盘的寻址方式：
- **C**ylinder（柱面）- 磁道号
- **H**ead（磁头）- 磁头号
- **S**ector（扇区）- 扇区号

问题：
- 需要转换逻辑地址到物理地址
- 受到 8GB 限制（1024 柱面 × 256 磁头 × 63 扇区 × 512 字节）
- BIOS INT 13h AH=02h 一次最多读 128 扇区（有的 BIOS 是 64）

### LBA（逻辑块地址）

LBA 直接用扇区编号寻址，从 0 开始：
- LBA 0 = 第一个扇区
- LBA 1 = 第二个扇区
- ...

优势：
- 简单直接，不需要转换
- 支持超大磁盘（48 位 LBA → 128PB）
- INT 13h 扩展读（AH=42h）一次最多读 127 扇区

---

## INT 13h 扩展读取

BIOS 提供了 INT 13h AH=42h 扩展读取功能，支持 LBA 寻址。

### 检测 LBA 支持

在使用 LBA 之前，我们需要检测 BIOS 是否支持：

```asm
; check_lba_support - Check if BIOS supports LBA extended reads
; Input: none
; Output: CF=0 if supported, CF=1 if not supported
; Clobbers: AX, BX, CX
bits 16
check_lba_support:
    pusha

    mov dl, 0x80                    ; First hard drive

    ; Check for LBA support using INT 13h AH=41h
    mov ah, 0x41
    mov bx, 0x55AA                  ; Magic value
    int 0x13

    ; Check if function is supported (CF=0 and BX=0xAA55)
    jc .not_supported
    cmp bx, 0xAA55
    jne .not_supported

    ; Check if LBA extensions are available (bit 0 of CX)
    test cx, 0x01
    jz .not_supported

    ; LBA is supported!
    popa
    clc                             ; Clear carry = supported
    ret

.not_supported:
    popa
    stc                             ; Set carry = not supported
    ret
```

**注意**：
- `BX=0x55AA` 是魔法值，BIOS 会把它返回为 `0xAA55` 表示支持
- `CF=0` 表示功能成功
- `CX` 的 bit 0 表示是否支持 LBA

### DAP 结构

INT 13h AH=42h 使用一个叫 DAP（Disk Address Packet）的结构：

```asm
; Disk Address Packet (DAP) for LBA extended reads
; Structure: 16 bytes total
align 4
dap_structure:
    db 16                      ; Packet size (16 bytes)
    db 0                       ; Reserved
    dw 0                       ; Block count (filled at runtime)
    dw 0                       ; Destination offset (filled at runtime)
    dw 0                       ; Destination segment (filled at runtime)
    dq 0                       ; Starting LBA (filled at runtime)
```

各字段说明：
| 偏移 | 大小 | 说明                           |
| ---- | ---- | ------------------------------ |
| 0    | 1    | 数据包大小（必须是 16）        |
| 1    | 1    | 保留（必须为 0）               |
| 2-3  | 2    | 要传输的块数（1-127）          |
| 4-5  | 2    | 目标缓冲区偏移                 |
| 6-7  | 2    | 目标缓冲区段                   |
| 8-15 | 8    | 起始 LBA（64 位，我们用低 32 位） |

---

## 实现 LBA 读取函数

### 单次读取函数

```asm
; read_sectors_lba - Read sectors using LBA extended addressing
; Input:  EAX = Starting LBA address
;         CX  = Number of sectors to read
;         ES:BX = Destination buffer
; Output: CF=0 on success, CF=1 on error
; Clobbers: AX, BX, CX, DX, SI
bits 16
read_sectors_lba:
    pusha

    ; Validate sector count (max 127 for compatibility)
    cmp cx, 0
    je .error
    cmp cx, 127
    jbe .count_ok
    mov cx, 127                     ; Cap at 127 sectors
.count_ok:

    ; Setup DS to point to our code segment (where dap_structure is)
    push ax
    mov ax, cs
    mov ds, ax
    pop ax

    ; Fill in DAP structure
    mov byte [dap_structure + 2], cl    ; Block count (low byte)
    mov byte [dap_structure + 3], 0     ; Block count (high byte)

    ; Destination buffer
    mov [dap_structure + 4], bx         ; Offset
    mov word [dap_structure + 6], es    ; Segment

    ; Starting LBA (64-bit, we use lower 32 bits)
    mov dword [dap_structure + 8], eax  ; LBA (low 32-bit)
    mov dword [dap_structure + 12], 0   ; LBA (high 32-bit) = 0

    ; Perform LBA read (INT 13h AH=42h)
    mov si, dap_structure               ; DS:SI points to DAP
    mov dl, 0x80                        ; First hard drive
    mov ah, 0x42                        ; Extended read
    int 0x13

    ; Restore DS to 0
    push ax
    xor ax, ax
    mov ds, ax
    pop ax

    jc .error

    ; Success
    popa
    clc
    ret

.error:
    ; Restore DS before error return
    push ax
    xor ax, ax
    mov ds, ax
    pop ax
    popa
    stc
    ret
```

### 多扇区读取函数

内核可能很大，一次读取不够。我们需要循环读取：

```asm
; load_kernel_lba - Load kernel using LBA extended addressing
; Input: none (uses KERNEL_LBA_START and KERNEL_SECTOR_COUNT from config)
; Output: loads kernel to KERNEL_LOAD_SEGMENT:KERNEL_LOAD_OFFSET
; Clobbers: AX, BX, CX, DX, SI, DI, BP
; Returns: CF=0 on success, CF=1 on error
bits 16
load_kernel_lba:
    pusha

    ; Setup destination address
    mov ax, KERNEL_LOAD_SEGMENT
    mov es, ax
    mov bx, KERNEL_LOAD_OFFSET      ; ES:BX = destination

    ; Initialize tracking variables
    mov di, KERNEL_SECTOR_COUNT     ; DI = remaining sectors to read
    mov si, KERNEL_LBA_START        ; SI = current LBA (low word)

    ; Print loading message (VGA + Serial)
    pusha
    mov si, msg_loading_lba
    call print_bios
    mov si, msg_loading_lba
    call serial_write_string
    mov ax, di
    call print_decimal
    push ax
    mov al, ah
    call serial_write_char_blocking
    pop ax
    call serial_write_decimal
    mov si, msg_sectors
    call print_bios
    mov si, msg_sectors
    call serial_write_string
    popa

.read_loop:
    ; Check if all sectors read
    cmp di, 0
    je .read_complete

    ; Calculate sectors to read this iteration (max 127)
    mov cx, di
    cmp cx, 127
    jbe .sectors_ok
    mov cx, 127
.sectors_ok:

    ; Save sector count
    mov bp, cx

    ; Convert SI to EAX (32-bit LBA)
    xor eax, eax
    mov ax, si

    ; Perform LBA read
    call read_sectors_lba
    jc .read_error

    ; Update tracking variables
    sub di, bp                      ; Decrease remaining sectors
    add si, bp                      ; Advance LBA

    ; Advance buffer pointer (ES:BX += BP * 512)
    push ax
    push dx
    mov ax, bp
    xor dx, dx
    mov cx, 512
    mul cx                          ; DX:AX = bytes read
    add bx, ax
    pop dx
    pop ax

    jmp .read_loop                  ; Next iteration

.read_complete:
    popa
    clc                             ; Clear carry = success
    ret

.read_error:
    popa
    stc                             ; Set carry = error
    ret
```

**注意**：这里我们用 `SI` 存储当前 LBA，`DI` 存储剩余扇区数。每次循环最多读 127 个扇区（BIOS 限制）。

---

## 使用 LBA 加载内核

在 Stage 2 主函数中调用：

```asm
stage2_main:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7E00

    ; Print message (VGA + Serial)
    mov si, msg_stage2
    call print_bios
    mov si, msg_stage2
    call serial_write_string

    ; ===== 使用 LBA 加载内核 =====
    call load_kernel_auto    ; 这个函数会先尝试 LBA
    jc kernel_error

    ; ... 继续后面的代码 ...
```

`load_kernel_auto` 函数会先检测 LBA 支持，如果支持就用 LBA，否则回退到 CHS（下一篇教程会讲）。

---

## 验证 LBA 读取

编译并运行：

```bash
nasm -f bin bootloader.asm -o bootloader.bin
qemu-system-x86_64 -drive format=raw,file=boot.img -serial stdio
```

你应该能在串口输出中看到：

```
[MODE] Using LBA extended read
[LOAD] LBA: loading 31 sectors
```

如果看到这个，恭喜！LBA 读取成功了。

---

## 常见问题

### 问题 1：LBA 检测失败

QEMU 默认支持 LBA，如果检测失败，检查：
1. `check_lba_support` 的返回值检查
2. BIOS 是否真的支持（QEMU 肯定支持）

### 问题 2：读取的扇区数不对

检查：
1. DAP 结构的 `Block count` 字段是否正确
2. 循环中的 `DI` 和 `BP` 是否正确更新

### 问题 3：缓冲区指针计算错误

检查：
1. `ES:BX += BP * 512` 的计算是否正确
2. 注意这里 `BX` 可能溢出，但我们的内核不大（< 64KB），所以没问题

---

## 下一步

现在我们有了 LBA 读取，但万一 BIOS 不支持 LBA 怎么办？

下一篇教程里，我们会加上 **CHS 回退机制**，确保在老机器上也能正常启动。

记住：兼容性很重要，不是所有机器都那么新。


---

<div align="center">

## 文档导航

[← 加上串口输出](04_加上串口输出.md)  | [加上CHS兜底 →](06_加上CHS兜底.md)

</div>
