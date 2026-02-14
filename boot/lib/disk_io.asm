; Disk I/O Library
; Provides disk loading functions for 16-bit real mode

bits 16

; Load second stage bootloader from disk
; Input: none (uses boot drive in DL from BIOS)
; Output: loads boot2 to 0x7E00
; Clobbers: AX, BX, CX, DX, SI, DI
load_second_stage:
    pusha

    ; Use the boot drive (DL is preserved by BIOS)

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

; Load kernel from disk to memory
; IMPORTANT: Must be called in REAL MODE (BIOS disk functions only work here)
; Input: none
; Output: loads kernel to 0x10000
; Clobbers: AX, BX, CX, DX
load_kernel:
    pusha

    ; Load kernel using CHS (simple, works everywhere)
    ; Kernel is at sector 4 (1-based: boot1=1 boot2=2,3 kernel=4)
    mov ax, 0x1000
    mov es, ax              ; ES:BX = 0x1000:0x0000 = 0x10000
    xor bx, bx

    mov ah, 0x02            ; read function
    mov al, 0x01            ; read 1 sector
    mov ch, 0x00            ; cylinder 0
    mov cl, 0x04            ; sector 4
    mov dh, 0x00            ; head 0
    mov dl, 0x80            ; first hard drive
    int 0x13

    jc .read_error
    popa
    ret

.read_error:
    ; Print error code
    mov si, msg_load_error
    call print_bios

    ; Show error code in AH
    mov al, ah
    shr al, 4
    add al, '0'
    cmp al, '9'
    jbe .d1
    add al, 7
.d1:
    mov ah, 0x0e
    int 0x10

    ; Hang on error
    mov si, msg_halt
    call print_bios
.hang:
    hlt
    jmp .hang

; Disk error handler
; Input: SI = error message pointer, AH = error code
; Clobbers: all registers, halts the system
disk_error:
    ; print error message and hang
    pusha
    mov si, disk_error_msg
    call print_string
    popa

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

; ============================================================================
; Data Section (no section directive - inherits from including file)
; ============================================================================

; Error messages (will be placed in the current section)
disk_error_msg:
    db "[E1] Failed to load Stage 2", 0x0d, 0x0a, 0

msg_load_error:
    db "[E2] Failed to load kernel", 0x0d, 0x0a, 0

msg_halt:
    db "[ERR] System halted", 0x0d, 0x0a, 0
