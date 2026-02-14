; CCOS Bootloader Stage 2
; Loads at 0x7E00, switches to 64-bit long mode

section .text
    org 0x7E00
    bits 16

start:
    jmp short main
    nop

; GDT - Put immediately after entry point to minimize displacement
gdt_start:
    dq 0x0000000000000000                    ; null descriptor
gdt_code:
    dq 0x00CF9A000000FFFF                   ; 32-bit code segment
gdt_data:
    dq 0x00CF92000000FFFF                   ; 32-bit data segment
gdt_code64:
    dq 0x00AF9A000000FFFF                   ; 64-bit code segment
gdt_data64:
    dq 0x00CF92000000FFFF                   ; 64-bit data segment
gdt_end:

; GDT pointer
gdt_ptr:
    dw 5 * 8 - 1              ; limit = 5 * 8 - 1 = 39 (0x27)
    dd 0x00007E03             ; base = 0x7E03

main:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7E00

    ; Print message using BIOS
    mov si, msg_stage2
    call print_bios

    ; Load kernel NOW while still in real mode
    call load_kernel

    ; Load GDT
    lgdt [gdt_ptr]

    ; Enable protected mode
    mov eax, cr0         ; Read CR0 to EAX
    or eax, 1           ; Set PE bit
    mov cr0, eax        ; Write to CR0

    ; Far jump to protected mode
    ; Manual encoding: 66 EA [32-bit offset] [16-bit segment]
    db 0x66             ; operand-size prefix (32-bit offset)
    db 0xEA             ; jmp far opcode
    dd pm_entry         ; 32-bit offset (little-endian)
    dw 0x08             ; 16-bit segment selector

; ============================================================================
; Include library functions (16-bit)
; ============================================================================
%include "boot/lib/bios_string.asm"
%include "boot/lib/disk_io.asm"

; ============================================================================
; 32-bit Protected Mode
; ============================================================================
bits 32

pm_entry:
protected_mode:
    ; Setup data segments
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

    ; Load PML4 into CR3
    mov eax, 0x9000
    mov cr3, eax

    ; Set LME bit in EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging (this switches to long mode!)
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; Far jump to 64-bit code
    jmp 0x18:long_mode

    ; Should never reach here
.hang:
    hlt
    jmp .hang

; ============================================================================
; Include protected mode library functions
; ============================================================================
%include "boot/lib/pmode.asm"

; ============================================================================
; 64-bit Long Mode
; ============================================================================
bits 64

long_mode:
    ; Setup data segments
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x7E00

    ; Clear screen and print ready message
    call clear_screen64
    mov rsi, msg_ready
    call print_string64

    ; Jump to kernel entry point at 0x10000
    mov rdi, 0x10000
    call rdi

    ; If kernel returns, halt
kernel_halt:
    hlt
    jmp kernel_halt

; ============================================================================
; Include long mode library functions
; ============================================================================
%include "boot/lib/lmode.asm"

; ============================================================================
; Data Section (16-bit real mode)
; ============================================================================
msg_stage2:
    db "[2] Stage 2: Loading kernel...", 0x0d, 0x0a, 0

msg_ready:
    db "READY TO BOOT KERNEL", 0
