; CCOS Kernel Entry Point
; Assembly stub that sets up C environment and jumps to kernel_main
; Loaded at 0x10000 by bootloader (set by linker script)

section .text
    bits 64

; External C function
extern kernel_main

; Kernel entry point - bootloader jumps here
global kernel_start
kernel_start:
    ; Disable interrupts for setup
    cli

    ; Set up stack - grow down from 0x80000
    ; Ensure 16-byte alignment for SSE/AVX instructions used by O2/O3 optimizations
    ; The bootloader's 'call rdi' pushed 8 bytes (return address), so we need to
    ; account for that. We want the stack to be 16-byte aligned BEFORE we call
    ; kernel_main (which will push another 8 bytes).
    mov rsp, 0x80000 - 8  ; Adjust for bootloader's call
    and rsp, -16           ; Align to 16-byte boundary
    mov rbp, rsp

    ; Clear BSS section (uninitialized data)
    ; Linker puts __bss_start and __bss_end symbols
    extern __bss_start
    extern __bss_end
    cld
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    shr rcx, 3  ; Convert to qwords
    xor eax, eax
    rep stosq

    ; Optional: Write 'X' to VGA to prove we got here (for debugging)
    mov rdi, 0xB8000 + 160 * 5  ; Line 5
    mov word [rdi], 0x1F58  ; 'X' in white on blue

    ; Jump to C kernel main
    call kernel_main

    ; If kernel_main returns, halt
halt_loop:
    cli
    hlt
    jmp halt_loop
