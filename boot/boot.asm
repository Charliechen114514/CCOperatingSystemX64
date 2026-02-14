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

    ; clear screen first
    call clear_screen

    ; print header
    mov si, header_msg
    call print_string

    ; print welcome message (real mode)
    mov si, welcome_msg
    call print_string

    ; load second stage bootloader
    call load_second_stage

    ; jump to second stage
    jmp 0x7E00

; 16-bit real mode code
bits 16

; ============================================================================
; Include library functions
; ============================================================================
%include "boot/lib/bios_screen.asm"
%include "boot/lib/bios_string.asm"
%include "boot/lib/disk_io.asm"

; ============================================================================
; Data Section
; ============================================================================
header_msg:
    db "READY TO BOOT CCOS", 0x0d, 0x0a, 0

welcome_msg:
    db "[1] Stage 1: Loading second stage...", 0x0d, 0x0a, 0

; pad to 510 bytes
times 510-($-$$) db 0

; MBR signature
dw 0xaa55
