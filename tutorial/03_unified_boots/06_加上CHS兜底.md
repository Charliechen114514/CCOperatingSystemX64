# 加上 CHS 兜底

老机器还是有的，我们不能抛弃它们。

---

## 为什么需要 CHS 回退？

虽然现代 BIOS 都支持 LBA，但：
1. 一些很老的机器（90 年代的）可能不支持 LBA
2. 虚拟机配置不当也可能导致 LBA 不可用
3. 我们希望 bootloader 尽可能兼容

所以，我们的策略是：
- **优先尝试 LBA** - 快速高效
- **失败则用 CHS** - 兜底方案

---

## LBA 到 CHS 的转换

CHS 参数的标准值：
- **Sectors per Track**: 63
- **Heads**: 16

这是 BIOS 的"虚拟几何"，和实际磁盘物理结构无关。

### 转换公式

```
给定 LBA 地址，计算 CHS：

temp = LBA / sectors_per_track
cylinder = temp / heads
head = temp % heads
sector = (LBA % sectors_per_track) + 1    ; 注意：扇区号从 1 开始！
```

### 实现代码

```asm
; lba_to_chs - Convert LBA to CHS addressing
; Input: AX = LBA address (0-based)
; Output: CH = Cylinder, CL = Sector (1-based, bits 0-5), DH = Head
; Clobbers: AX, BX, CX, DX
; Uses: SECTORS_PER_TRACK, HEADS from boot_config.inc
bits 16
lba_to_chs:
    push bx

    ; Save LBA
    mov bx, ax

    ; Calculate temp = LBA / SECTORS_PER_TRACK
    xor dx, dx
    mov ax, bx
    mov cx, SECTORS_PER_TRACK
    div cx                      ; AX = temp, DX = LBA % SECTORS_PER_TRACK

    ; Save the remainder (sector index 0-based)
    push dx                      ; Save sector index

    ; Calculate Cylinder = temp / HEADS
    xor dx, dx
    mov cx, HEADS
    div cx                      ; AX = Cylinder, DX = Head

    mov ch, al                  ; CH = Cylinder (low 8 bits)
    mov dh, dl                  ; DH = Head

    ; Calculate Sector = (LBA % SECTORS_PER_TRACK) + 1
    pop dx                      ; Restore sector index (0-based)
    mov cl, dl                  ; CL = sector (0-based)
    add cl, 1                   ; Convert to 1-based

    pop bx
    ret
```

**注意**：
- 扇区号从 1 开始，所以要 `+1`
- 柱面号超过 255 的情况我们暂不考虑（内核不会那么大）

---

## CHS 读取函数

### 单次读取

```asm
; load_kernel_chs - Load kernel using CHS addressing
; Input: SI = starting LBA address, DI = total sector count
; Output: loads kernel to KERNEL_LOAD_SEGMENT:KERNEL_LOAD_OFFSET
; Clobbers: AX, BX, CX, DX, SI, DI, BP
; Returns: CF=0 on success, CF=1 on error
bits 16
load_kernel_chs:
    pusha

    ; Setup destination address
    mov ax, KERNEL_LOAD_SEGMENT
    mov es, ax
    mov bx, KERNEL_LOAD_OFFSET      ; ES:BX = destination

    ; Initialize tracking variables FIRST
    mov di, KERNEL_SECTOR_COUNT     ; DI = remaining sectors to read
    mov si, KERNEL_LBA_START        ; SI = current LBA

    ; Print loading message (VGA + Serial)
    pusha
    mov si, msg_loading_kernel
    call print_bios
    mov si, msg_loading_kernel
    call serial_write_string
    mov ax, di                    ; AX = total sector count
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

    ; Save sector count for later
    mov bp, cx
    mov bx, cx                      ; Also save in BX for 8-bit access

    ; Convert LBA to CHS
    mov ax, si
    call lba_to_chs                 ; CH=Cyl, DH=Head, CL=Sector(1-based)

    ; CL now contains sector number (1-based, in bits 0-5)
    ; BIOS INT 13h AH=02h expects:
    ;   AL = number of sectors to read
    ;   CH = cylinder number (low 8 bits)
    ;   CL = bits 7-6: cylinder high bits, bits 5-0: starting sector
    ;   DH = head number

    ; Set AL = sector count (saved in BL)
    mov ah, 0x02                    ; read function
    mov al, bl                      ; sector count
    mov dl, 0x80                    ; first hard drive

    ; Perform read
    int 0x13
    jc .read_error

    ; Verify sectors read (BIOS returns count in AL)
    cmp al, bl
    jne .read_mismatch

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
    clc                             ; clear carry = success
    ret

.read_error:
.read_mismatch:
    popa
    stc                             ; set carry = error
    ret
```

**注意**：
- BIOS INT 13h AH=02h 一次最多读 128 扇区（但我们用 127 更安全）
- `cmp al, bl` 验证实际读取的扇区数是否正确

---

## 自动选择函数

现在我们实现 `load_kernel_auto`，自动选择 LBA 或 CHS：

```asm
; load_kernel_auto - Load kernel with automatic LBA/CHS selection
; Tries LBA first, falls back to CHS if LBA fails
; Input: none
; Output: loads kernel to KERNEL_LOAD_SEGMENT:KERNEL_LOAD_OFFSET
; Clobbers: AX, BX, CX, DX, SI, DI, BP
; Returns: CF=0 on success, CF=1 on error
bits 16
load_kernel_auto:
    pusha

    ; First, try LBA extended read
    call check_lba_support
    jc .try_chs                     ; LBA not supported, try CHS

    ; LBA is supported, attempt LBA load
    pusha
    mov si, msg_using_lba
    call print_bios
    mov si, msg_using_lba
    call serial_write_string
    popa

    call load_kernel_lba
    jnc .success                    ; LBA succeeded!

    ; LBA failed, fall back to CHS
    pusha
    mov si, msg_lba_fallback
    call print_bios
    mov si, msg_lba_fallback
    call serial_write_string
    popa

.try_chs:
    ; Use CHS mode
    pusha
    mov si, msg_using_chs
    call print_bios
    mov si, msg_using_chs
    call serial_write_string
    popa

    call load_kernel_chs
    jc .error                       ; CHS also failed

.success:
    popa
    clc
    ret

.error:
    popa
    stc
    ret
```

**流程**：
1. 检测 LBA 支持
2. 如果支持，尝试 LBA 读取
3. 如果 LBA 失败，回退到 CHS
4. 如果 CHS 也失败，报错

---

## 错误消息

别忘了加上相应的错误消息：

```asm
msg_using_lba:
    db "[MODE] Using LBA extended read", 0x0d, 0x0a, 0

msg_using_chs:
    db "[MODE] Using CHS fallback", 0x0d, 0x0a, 0

msg_lba_fallback:
    db "[WARN] LBA failed, falling back to CHS...", 0x0d, 0x0a, 0

msg_loading_lba:
    db "[LOAD] LBA: loading ", 0

msg_loading_kernel:
    db "[LOAD] CHS: loading ", 0

msg_sectors:
    db " sectors", 0x0d, 0x0a, 0
```

---

## 验证 CHS 回退

### 正常情况（使用 LBA）

```bash
qemu-system-x86_64 -drive format=raw,file=boot.img -serial stdio
```

输出：
```
[MODE] Using LBA extended read
[LOAD] LBA: loading 31 sectors
```

### 强制使用 CHS

你可以暂时修改代码，让 LBA 检测失败，来测试 CHS：

```asm
; 临时让 LBA 检测失败
check_lba_support:
    stc    ; 直接返回失败
    ret
```

然后重新编译运行，应该看到：

```
[MODE] Using CHS fallback
[LOAD] CHS: loading 31 sectors
```

**测试完后记得改回来！**

---

## 常见问题

### 问题 1：CHS 读取失败

检查：
1. `lba_to_chs` 的转换公式是否正确
2. 扇区号是否加了 1（扇区从 1 开始）
3. BIOS INT 13h 的参数是否正确

### 问题 2：多扇区读取不完整

检查：
1. 每次读取的扇区数是否超过 127
2. 循环中的 `DI` 和 `SI` 是否正确更新
3. 缓冲区指针是否正确前进

### 问题 3：LBA 回退到 CHS 仍然失败

这可能是磁盘镜像的问题，检查：
1. `boot.img` 是否正确生成
2. 内核扇区数是否正确
3. 内核起始扇区是否正确

---

## 下一步

现在我们的磁盘加载功能已经很完善了：
- ✅ LBA 扩展读取（优先）
- ✅ CHS 回退（兜底）
- ✅ 串口输出（调试）
- ✅ 单文件架构

下一篇教程里，我们会**更新构建系统**，让编译和部署更简单。

说实话，Makefile 写得漂亮一点，后续开发会省很多心。


---

<div align="center">

## 文档导航

[← 实现LBA磁盘读取](05_实现LBA磁盘读取.md)  | [更新构建系统 →](07_更新构建系统.md)

</div>
