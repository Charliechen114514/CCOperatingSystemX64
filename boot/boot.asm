; bootloader for x86_64 OS
; start in real mode, print string, then switch to protected mode

section .mbr
    org 0x7c00

start:
    ; clear interrupts
    cli

    ; set up segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    ; enable interrupts
    sti

    ; print welcome message (real mode)
    mov si, welcome_msg
    call print_string

    ; load second stage bootloader
    call load_second_stage

    ; print loading message
    mov si, loading_msg
    call print_string

    ; jump to second stage
    jmp 0x7E00

; 16-bit real mode code
bits 16

; print string function (BIOS)
; input: si = pointer to null-terminated string
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

; load second stage bootloader from disk
load_second_stage:
    pusha

    ; Use the boot drive (DL is preserved by BIOS)
    ; Instead of hardcoding dl=0x00, we'll try multiple methods

    ; Method 1: Try LBA extended read first
    mov ax, 0x4100
    mov bx, 0x55AA
    int 0x13

    jc .try_chs             ; no extended support
    cmp bx, 0xAA55         ; check signature
    jne .try_chs

    ; Extended read supported, use LBA
    pusha                   ; Save all registers
    mov edi, 0x5000        ; use a safer address for DAP

    mov byte [edi + 0], 0x10    ; packet size = 16
    mov byte [edi + 1], 0x00    ; reserved
    mov word [edi + 2], 0x02    ; read 2 sectors (boot2 is ~824 bytes)
    mov word [edi + 4], 0x0000  ; buffer offset = 0x0000
    mov word [edi + 6], 0x7E0   ; buffer segment = 0x7E0 (address = 0x7E00)
    mov dword [edi + 8], 0x00000001  ; LBA = 1 (sector 2, 0-based)
    mov dword [edi + 12], 0x00000000

    mov si, di
    mov ah, 0x42            ; extended read
    int 0x13

    popa                    ; Restore registers
    jc .try_chs             ; if failed, try CHS

    cmp al, 0x02            ; check if 2 sectors were read
    jne .try_chs            ; if not, try CHS

    popa
    ret

.try_chs:
    ; Method 2: Legacy CHS read
    ; Read from sector 2 (head 0, cylinder 0, sector 2)
    mov ax, 0x7E0
    mov es, ax
    xor bx, bx

    mov ah, 0x02            ; read function
    mov al, 0x02            ; read 2 sectors (boot2 is ~824 bytes)
    mov ch, 0x00            ; cylinder 0
    mov cl, 0x02            ; sector 2 (sector 1 is MBR)
    mov dh, 0x00            ; head 0
    ; dl already contains boot drive number from BIOS

    int 0x13

    jc disk_error

    cmp al, 0x02            ; verify 2 sectors were read
    jne disk_error

    popa
    ret

disk_error:
    ; print error message and hang
    mov si, disk_error_msg
    call print_string

    ; Print error code in AH (convert to hex)
    ; IMPORTANT: Save AH before using it for BIOS calls!
    mov bl, ah          ; Save error code in BL
    mov al, bl          ; Copy to AL
    shr al, 4           ; Get high 4 bits
    add al, '0'
    cmp al, '9'
    jbe .digit1
    add al, 7
.digit1:
    pusha               ; Save all registers
    mov ah, 0x0e
    mov bh, 0
    mov bl, 0x07        ; Restore BL from stack later
    int 0x10
    popa

    mov al, bl          ; Get saved error code
    and al, 0x0F        ; Get low 4 bits
    add al, '0'
    cmp al, '9'
    jbe .digit2
    add al, 7
.digit2:
    mov ah, 0x0e
    mov bh, 0
    mov bl, 0x07
    int 0x10

.hang:
    hlt
    jmp .hang

; data section
welcome_msg:
    db "CCOS Bootloader Stage 1...", 0x0d, 0x0a, 0

loading_msg:
    db "Loading Stage 2...", 0x0d, 0x0a, 0

disk_error_msg:
    db "DISK ERROR! Cannot read Stage 2", 0x0d, 0x0a, 0

; pad to 510 bytes
times 510-($-$$) db 0

; MBR signature
dw 0xaa55
