; ==============================================================================
; CCOS Unified Bootloader
; ==============================================================================
; This single file contains both Stage 1 (MBR) and Stage 2
; - Stage 1 (0x7C00): Loads the rest of bootloader from disk to 0x7E00
; - Stage 2 (0x7E00): Switches to 64-bit long mode and loads kernel
; ==============================================================================

; Include auto-generated configuration (must exist before first use)
%include "boot_config.inc"

; ==============================================================================
; Stage 1: MBR (0x7C00)
; ==============================================================================
section .mbr
    org 0x7c00
    bits 16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    ; clear screen
    mov ah, 0x00
    mov al, 0x03
    int 0x10

    ; print header
    mov si, header_msg
    call print_string

    ; print welcome
    mov si, welcome_msg
    call print_string

    ; Load Stage 2 from disk (sectors 2-4)
    ; Bootloader is BOOTLOADER_SECTORS total, minus 1 for MBR = remaining sectors
    mov ax, 0x7E0
    mov es, ax
    xor bx, bx
    mov ah, 0x02
    mov al, BOOTLOADER_SECTORS - 1    ; Load remaining sectors (Stage 2)
    mov ch, 0x00
    mov cl, 0x02                       ; Start from sector 2 (LBA 1)
    mov dh, 0x00
    mov dl, 0x80
    int 0x13
    jc load_error
    cmp al, BOOTLOADER_SECTORS - 1     ; Verify all sectors read
    jne load_error

    ; jump to Stage 2
    jmp 0x7E00

load_error:
    mov si, load_error_msg
    call print_string
.hang:
    hlt
    jmp .hang

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

header_msg:
    db "READY TO BOOT CCOS", 0x0d, 0x0a, 0

welcome_msg:
    db "[1] Stage 1: Loading second stage...", 0x0d, 0x0a, 0

load_error_msg:
    db "[E1] Failed to load Stage 2", 0x0d, 0x0a, 0

; Pad to exactly 510 bytes (before signature)
; Calculate: 510 - current_position_in_section
times 510-($-$$) db 0

; MBR signature at offset 510-511
dw 0xaa55


; ==============================================================================
; Stage 2: Starts at offset 512 in file, loads at 0x7E00
; ==============================================================================
section .stage2 vstart=0x7E00
    bits 16

stage2_entry:
    jmp short stage2_main
    nop

; GDT
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

stage2_main:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7E00

    ; Print message
    mov si, msg_stage2
    call print_bios

    ; Load kernel using automatic LBA/CHS selection
    call load_kernel_auto
    jc kernel_error

    ; Kernel loaded successfully - print success message
    mov si, msg_kernel_load_success
    call print_bios

    ; Load GDT and switch to protected mode
    lgdt [gdt_ptr]
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to protected mode
    db 0x66
    db 0xEA
    dd pm_entry
    dw 0x08

kernel_error:
    mov si, msg_kernel_error
    call print_bios
.hang:
    hlt
    jmp .hang

print_bios:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0e
    int 0x10
    jmp .loop
.done:
    popa
    ret

msg_stage2:
    db "[2] Stage 2: Loading kernel...", 0x0d, 0x0a, 0

msg_kernel_load_success:
    db "[I] Kernel Load Success, About Enter", 0x0d, 0x0a, 0

msg_kernel_error:
    db "[E2] Failed to load kernel", 0x0d, 0x0a, 0


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

    ; Setup page tables
    call setup_page_tables

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Load PML4
    mov eax, 0x9000
    mov cr3, eax

    ; Set LME bit
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; Jump to 64-bit
    jmp 0x18:long_mode

.hang:
    hlt
    jmp .hang

setup_page_tables:
    pusha

    ; Clear PML4
    mov edi, 0x9000
    mov ecx, 4096 / 4
    xor eax, eax
    rep stosd

    ; Clear PDPT
    mov edi, 0xA000
    mov ecx, 4096 / 4
    xor eax, eax
    rep stosd

    ; Clear PD
    mov edi, 0xB000
    mov ecx, 4096 / 4
    xor eax, eax
    rep stosd

    ; Setup entries
    mov dword [0x9000], 0x0000A003    ; PML4[0] -> PDPT
    mov dword [0x9FF8], 0x0000A003    ; PML4[511] -> PDPT
    mov dword [0xA000], 0x0000B003    ; PDPT[0] -> PD
    mov dword [0xB000], 0x00000083    ; PD[0] -> 2MB page
    mov dword [0xB004], 0x00000000
    mov dword [0xBFF0], 0x00000083    ; PD[511] -> 2MB page
    mov dword [0xBFF4], 0x00000000

    popa
    ret


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

    ; Clear screen
    push rax
    push rdi
    push rcx
    mov rdi, 0xB8000
    mov rcx, 80 * 25
    mov rax, 0x0720
.clear_loop:
    mov [rdi], ax
    add rdi, 2
    loop .clear_loop
    pop rcx
    pop rdi
    pop rax

    ; Print ready message
    mov rsi, msg_ready
    mov rdi, 0xB8000
    mov ah, 0x0F
.print_loop:
    lodsb
    test al, al
    jz .print_done
    stosw
    jmp .print_loop
.print_done:

    ; Jump to kernel
    mov rdi, 0x10000
    call rdi

    ; Halt if kernel returns
kernel_halt:
    hlt
    jmp kernel_halt

msg_ready:
    db "READY TO BOOT KERNEL", 0


; ==============================================================================
; Kernel Loading Functions
; ==============================================================================
; These functions are called from Stage 2 in real mode

; Disk Address Packet (DAP) for LBA extended reads
; Structure: 16 bytes total
;   Offset 0:    Size of packet (bytes)
;   Offset 1:    Reserved (must be 0)
;   Offset 2-3:  Number of blocks to transfer
;   Offset 4-7:  Destination buffer address (segment:offset)
;   Offset 8-15: Starting LBA address (64-bit)
align 4
dap_structure:
    db 16                      ; Packet size (16 bytes)
    db 0                       ; Reserved
    dw 0                       ; Block count (filled at runtime)
    dw 0                       ; Destination offset (filled at runtime)
    dw 0                       ; Destination segment (filled at runtime)
    dq 0                       ; Starting LBA (filled at runtime)


; check_lba_support - Check if BIOS supports LBA extended reads
; Input: none
; Output: CF=0 if supported, CF=1 if not supported
; Clobbers: AX, BX, CX
bits 16
check_lba_support:
    pusha

    ; Store current drive
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


; read_sectors_lba - Read sectors using LBA extended addressing (INT 13h AH=42h)
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

    ; Print loading message
    pusha
    mov si, msg_loading_lba
    call print_bios
    mov ax, di
    call print_decimal
    mov si, msg_sectors
    call print_bios
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
    popa

    call load_kernel_lba
    jnc .success                    ; LBA succeeded!

    ; LBA failed, fall back to CHS
    pusha
    mov si, msg_lba_fallback
    call print_bios
    popa

.try_chs:
    ; Use CHS mode
    pusha
    mov si, msg_using_chs
    call print_bios
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


; lba_to_chs - Convert LBA to CHS addressing (fallback for CHS mode)
; Input: AX = LBA address (0-based)
; Output: CH = Cylinder, CL = Sector (1-based, bits 0-5), DH = Head
; Clobbers: AX, BX, CX, DX
; Uses: SECTORS_PER_TRACK, HEADS from boot_config.inc
bits 16
lba_to_chs:
    push bx

    ; Save LBA
    mov bx, ax

    ; Calculate temporary value: LBA / SECTORS_PER_TRACK
    ; This gives us: (cylinder * HEADS) + head
    xor dx, dx
    mov ax, bx
    mov cx, SECTORS_PER_TRACK
    div cx                      ; AX = LBA / SECTORS_PER_TRACK, DX = LBA % SECTORS_PER_TRACK

    ; Save the remainder (sector index 0-based)
    push dx                      ; Save sector index

    ; Calculate Cylinder = (LBA / SECTORS_PER_TRACK) / HEADS
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


; load_kernel_chs - Load kernel using CHS addressing with multi-read support
; Input: SI = starting LBA address, DI = total sector count
;        ES:BX = destination buffer (if not using default KERNEL_LOAD_SEGMENT:OFFSET)
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

    ; Print loading message (AFTER initialization)
    pusha
    mov si, msg_loading_kernel
    call print_bios
    mov ax, di                    ; AX = total sector count (now DI is initialized!)
    call print_decimal
    mov si, msg_sectors
    call print_bios
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
    mov al, bl                      ; sector count (BL = low byte of original CX)
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
    ; For kernel < 64KB, we can ignore segment overflow
    push ax
    push dx
    mov ax, bp
    xor dx, dx
    mov cx, 512
    mul cx                          ; DX:AX = bytes read
    ; Add to BX
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


; ============================================================================
; Error Messages
; ============================================================================
msg_loading_lba:
    db "[LOAD] LBA: loading ", 0

msg_loading_kernel:
    db "[LOAD] CHS: loading ", 0

msg_using_lba:
    db "[MODE] Using LBA extended read", 0x0d, 0x0a, 0

msg_using_chs:
    db "[MODE] Using CHS fallback", 0x0d, 0x0a, 0

msg_lba_fallback:
    db "[WARN] LBA failed, falling back to CHS...", 0x0d, 0x0a, 0

msg_sectors:
    db " sectors", 0x0d, 0x0a, 0

msg_kernel_too_large:
    db "[E3] Kernel too large for single CHS read", 0x0d, 0x0a, 0


; ============================================================================
; Helper Functions
; ============================================================================

; print_decimal - Print AX as decimal number
; Input: AX = value to print
; Clobbers: AX, BX, CX, DX
bits 16
print_decimal:
    push bx
    push cx
    push dx

    mov bx, 10                   ; Base 10
    xor cx, cx                   ; Digit count

.dec_divide:
    xor dx, dx
    div bx                        ; DX:AX / 10
    push dx                       ; Save remainder (digit)
    inc cx
    test ax, ax
    jnz .dec_divide

.dec_print:
    pop ax
    add al, '0'
    mov ah, 0x0E
    int 0x10
    loop .dec_print

    pop dx
    pop cx
    pop bx
    ret
