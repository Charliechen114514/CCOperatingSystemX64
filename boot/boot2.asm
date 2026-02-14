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

; ============================================================================
; load_kernel - Load kernel.bin from disk to memory
; IMPORTANT: Must be called in REAL MODE (BIOS disk functions only work here)
; Uses simple CHS read for maximum compatibility
; ============================================================================
load_kernel:
    pusha

    ; Print debug message
    mov si, msg_loading_kernel
    call print_bios

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

    ; Print success
    mov si, msg_kernel_loaded
    call print_bios

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

    ; Print protected mode message using VGA
    mov esi, msg_protected
    call print_pm

    ; Print "Starting page tables..."
    mov esi, msg_pm_page
    call print_pm

    ; Setup page tables for long mode
    call setup_page_tables

    ; Print "Page tables done..."
    mov esi, msg_pm_done
    call print_pm

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

; Messages for protected mode (defined in 32-bit section)
msg_protected:
    db "Protected Mode OK", 0

msg_pm_page:
    db "Setting up page tables...", 0

msg_pm_done:
    db "Page tables OK, enabling long mode...", 0

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
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x7E00

    ; Print long mode message on line 2
    mov rsi, msg_longmode
    call print_lm

    ; Jump to kernel!
    ; Print jumping message on line 4
    mov rsi, msg_jumping_kernel
    call print_lm_line4

    ; Jump to kernel entry point at 0x10000
    mov rdi, 0x10000
    call rdi

    ; If kernel returns, halt
kernel_halt:
    hlt
    jmp kernel_halt

; Print string (long mode) - prints to VGA line 2
print_lm:
    push rax
    push rdi
    mov rdi, 0xB8000 + 160  ; Line 2
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

; Print to VGA line 4
print_lm_line4:
    push rax
    push rdi
    mov rdi, 0xB8000 + 160 * 4  ; Line 4
    mov ah, 0x1E  ; Yellow
.loop4:
    lodsb
    test al, al
    jz .done4
    stosw
    jmp .loop4
.done4:
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

msg_loading_kernel:
    db "[LOAD] Loading kernel...", 0x0d, 0x0a, 0

msg_kernel_loaded:
    db "[OK] Kernel loaded to 0x10000!", 0x0d, 0x0a, 0

msg_load_error:
    db "[ERROR] Disk read failed: ", 0

msg_halt:
    db 0x0d, 0x0a, "System halted.", 0x0d, 0x0a, 0

msg_longmode:
    db "=== LONG MODE ACTIVE ===", 0

msg_jumping_kernel:
    db "[LM] Jumping to kernel at 0x10000...", 0
