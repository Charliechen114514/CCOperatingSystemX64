; CCOS Kernel Entry Point
; This is a minimal 64-bit kernel for testing
; Built to be loaded by bootloader at 0x10000 (temporarily)

section .text
    org 0x10000          ; Kernel load address
    bits 64

; Kernel entry point - called from bootloader
kernel_start:
    ; Clear the screen and show we're alive
    mov rdi, 0xB8000 + 160 * 3  ; Line 3 (after bootloader messages)
    mov rsi, hello_msg
    mov ah, 0x1E              ; Yellow color

.kernel_print_loop:
    lodsb
    test al, al
    jz .kernel_done
    stosw
    jmp .kernel_print_loop

.kernel_done:
    ; Halt but keep interrupts enabled
    sti
.kernel_hang:
    hlt
    jmp .kernel_hang

; Data section
section .data
hello_msg:
    db "=== CCOS KERNEL IS RUNNING ===", 0
