; CCOS Kernel Entry Point
; Minimal 64-bit kernel for testing
; Loaded at 0x10000 by bootloader

section .text
    org 0x10000
    bits 64

; Kernel entry point
kernel_start:
    ; Write 'X' to VGA line 5 to prove we got here
    mov rdi, 0xB8000 + 160 * 5  ; Line 5
    mov word [rdi], 0x1F58  ; 'X' in white on blue

    ; Now halt
    cli
    hlt
    jmp kernel_start
