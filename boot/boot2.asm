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
; load_kernel - Load kernel.bin from disk to memory
; IMPORTANT: Must be called in REAL MODE (BIOS disk functions only work here)
; Uses simple CHS read for maximum compatibility
; ============================================================================
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
    call clear_screen
    mov rsi, msg_ready
    call print_string

    ; Jump to kernel entry point at 0x10000
    mov rdi, 0x10000
    call rdi

    ; If kernel returns, halt
kernel_halt:
    hlt
    jmp kernel_halt

; Clear screen (VGA text mode 80x25)
clear_screen:
    push rax
    push rdi
    push rcx
    mov rdi, 0xB8000
    mov rcx, 80 * 25
    mov rax, 0x0720      ; space with white on black
.rep:
    mov [rdi], ax
    add rdi, 2
    loop .rep
    pop rcx
    pop rdi
    pop rax
    ret

; Print string to VGA (64-bit)
print_string:
    push rax
    push rdi
    mov rdi, 0xB8000
    mov ah, 0x0F         ; white on black
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
    db "[2] Stage 2: Loading kernel...", 0x0d, 0x0a, 0

msg_load_error:
    db "[E2] Failed to load kernel", 0x0d, 0x0a, 0

msg_halt:
    db "[ERR] System halted", 0x0d, 0x0a, 0

msg_ready:
    db "READY TO BOOT KERNEL", 0
