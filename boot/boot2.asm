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
    dq 0                    ; null descriptor
gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x9A
    db 0xCF
    db 0x00
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92
    db 0xCF
    db 0x00
gdt_code64:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x9A
    db 0xAF
    db 0x00
gdt_data64:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92
    db 0xCF
    db 0x00
gdt_end:

; GDT pointer - immediate after GDT
gdt_ptr:
    dw 5 * 8 - 1  ; 5 descriptors, 8 bytes each, limit = size - 1
    dd 0x00007E03  ; GDT base address (fixed, gdt_start = 0x03)

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

    ; Load GDT
    lgdt [gdt_ptr]

    ; Print debug message
    mov si, debug_msg1
    call print_bios

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

; Print string using BIOS
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

; ============================================================================
; 32-bit Protected Mode
; ============================================================================
bits 32

pm_entry:
    ; This label is used for the far jump from real mode
    ; fall through to protected_mode
protected_mode:
    ; Setup data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x7E00

    ; Print protected mode message using VGA
    mov esi, msg_protected
    call print_pm

    ; Setup page tables for long mode
    call setup_page_tables

    ; Enable PAE (Physical Address Extension)
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Load PML4 into CR3
    mov eax, 0x9000
    mov cr3, eax

    ; Set LME bit in EFER MSR (Long Mode Enable)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; Far jump to long mode
    jmp 0x18:long_mode

; Print string (protected mode) - VGA direct print
print_pm:
    pusha
    mov edi, 0xB8000
    mov ah, 0x1F
.loop:
    lodsb
    test al, al
    jz .done
    stosw
    jmp .loop
.done:
    popa
    ret

; Message for protected mode (defined in 32-bit section)
msg_protected:
    db "Protected Mode OK", 0x0d, 0x0a, 0

; Setup page tables for long mode
setup_page_tables:
    pusha

    ; Clear PML4 (at 0x9000)
    mov edi, 0x9000
    mov ecx, 4096 / 4
    xor eax, eax
    rep stosd

    ; Clear PDPT (at 0xA000)
    mov edi, 0xA000
    mov ecx, 4096 / 4
    xor eax, eax
    rep stosd

    ; Clear PD (at 0xB000)
    mov edi, 0xB000
    mov ecx, 4096 / 4
    xor eax, eax
    rep stosd

    ; Setup PML4[0] -> PDPT
    mov dword [0x9000], 0x0000A003

    ; Setup PML4[511] -> PDPT (higher-half)
    mov dword [0x9FF8], 0x0000A003

    ; Setup PDPT[0] -> PD
    mov dword [0xA000], 0x0000B003

    ; Setup PD entries (2MB pages)
    mov dword [0xB000], 0x00000083
    mov dword [0xB004], 0x00000000
    mov dword [0xBFF0], 0x00000083
    mov dword [0xBFF4], 0x00000000

    popa
    ret

; Temporarily disabled long mode setup
; We need to verify 32-bit protected mode works first

; ============================================================================
; 64-bit Long Mode
; ============================================================================
bits 64

long_mode:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x7E00

    ; Direct VGA write - print 'L' at position 4 to verify long mode
    mov rax, 0x1F004C1F004C1F  ; 'LL' in white on blue
    mov qword [0xB8000 + 8], rax

    ; Print long mode message
    mov rsi, msg_longmode
    call print_lm

hang:
    hlt
    jmp hang

; Print string (long mode)
print_lm:
    push rax
    push rdi
    mov rdi, 0xB8000
    mov ah, 0x1F
.loop:
    lodsb
    test al, al
    jz .done
    stosw
    jmp .loop
.done:
    pop rdi
    pop rax
    ret

; ============================================================================
; Data Section (16-bit real mode)
; ============================================================================
msg_stage2:
    db "Stage 2 running...", 0x0d, 0x0a, 0

debug_msg1:
    db "[1] GDT loaded, PM enabled...", 0x0d, 0x0a, 0

msg_longmode:
    db "=== LONG MODE ACTIVE ===", 0x0d, 0x0a, 0
    db 0
