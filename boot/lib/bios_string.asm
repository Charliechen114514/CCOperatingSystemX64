; BIOS String Library
; Provides string printing functions for 16-bit real mode

bits 16

; Print string function (BIOS)
; Input: SI = pointer to null-terminated string
; Output: none
; Clobbers: AX, SI
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

; Print string using BIOS (alternate name for compatibility)
; Input: SI = pointer to null-terminated string
; Output: none
; Clobbers: AX, SI
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
