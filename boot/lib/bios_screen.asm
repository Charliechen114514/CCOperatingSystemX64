; BIOS Screen Library
; Provides screen-related functions for 16-bit real mode

bits 16

; Clear screen using BIOS
; Input: none
; Output: none
; Clobbers: AX
clear_screen:
    mov ah, 0x00        ; set video mode
    mov al, 0x03        ; text mode 80x25, 16 colors
    int 0x10
    ret
