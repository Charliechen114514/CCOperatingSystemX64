; ==============================================================================
; CCOS Unified Bootloader
; ==============================================================================
; This single file contains both Stage 1 (MBR) and Stage 2
; - Stage 1 (0x7C00): Loads the rest of bootloader from disk to 0x7E00
; - Stage 2 (0x7E00): Switches to 64-bit long mode and loads kernel
; ==============================================================================

; Include auto-generated configuration (must exist before first use)
%include "boot_config.inc"

; ==============================================================================
; Stage 1: MBR (0x7C00)
; ==============================================================================
section .mbr
    org 0x7c00
    bits 16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    ; clear screen
    mov ah, 0x00
    mov al, 0x03
    int 0x10

    ; print header
    mov si, header_msg
    call print_string

    ; print welcome
    mov si, welcome_msg
    call print_string

    ; Load Stage 2 from disk (sectors 2-3)
    mov ax, 0x7E0
    mov es, ax
    xor bx, bx
    mov ah, 0x02
    mov al, 0x02
    mov ch, 0x00
    mov cl, 0x02
    mov dh, 0x00
    mov dl, 0x80
    int 0x13
    jc load_error
    cmp al, 0x02
    jne load_error

    ; jump to Stage 2
    jmp 0x7E00

load_error:
    mov si, load_error_msg
    call print_string
.hang:
    hlt
    jmp .hang

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

header_msg:
    db "READY TO BOOT CCOS", 0x0d, 0x0a, 0

welcome_msg:
    db "[1] Stage 1: Loading second stage...", 0x0d, 0x0a, 0

load_error_msg:
    db "[E1] Failed to load Stage 2", 0x0d, 0x0a, 0

; Pad to exactly 510 bytes (before signature)
; Calculate: 510 - current_position_in_section
times 510-($-$$) db 0

; MBR signature at offset 510-511
dw 0xaa55


; ==============================================================================
; Stage 2: Starts at offset 512 in file, loads at 0x7E00
; ==============================================================================
section .stage2 vstart=0x7E00
    bits 16

stage2_entry:
    jmp short stage2_main
    nop

; GDT
gdt_start:
    dq 0x0000000000000000
gdt_code:
    dq 0x00CF9A000000FFFF
gdt_data:
    dq 0x00CF92000000FFFF
gdt_code64:
    dq 0x00AF9A000000FFFF
gdt_data64:
    dq 0x00CF92000000FFFF
gdt_end:

gdt_ptr:
    dw 5 * 8 - 1
    dd 0x00007E03

stage2_main:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7E00

    ; Print message
    mov si, msg_stage2
    call print_bios

    ; Load kernel using dynamic configuration from boot_config.inc
    ; Uses CHS mode for maximum compatibility
    call load_kernel_chs
    jc kernel_error

    ; Load GDT and switch to protected mode
    lgdt [gdt_ptr]
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to protected mode
    db 0x66
    db 0xEA
    dd pm_entry
    dw 0x08

kernel_error:
    mov si, msg_kernel_error
    call print_bios
.hang:
    hlt
    jmp .hang

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

msg_stage2:
    db "[2] Stage 2: Loading kernel...", 0x0d, 0x0a, 0

msg_kernel_error:
    db "[E2] Failed to load kernel", 0x0d, 0x0a, 0


; ==============================================================================
; 32-bit Protected Mode
; ==============================================================================
bits 32

pm_entry:
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

    ; Load PML4
    mov eax, 0x9000
    mov cr3, eax

    ; Set LME bit
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; Jump to 64-bit
    jmp 0x18:long_mode

.hang:
    hlt
    jmp .hang

setup_page_tables:
    pusha

    ; Clear PML4
    mov edi, 0x9000
    mov ecx, 4096 / 4
    xor eax, eax
    rep stosd

    ; Clear PDPT
    mov edi, 0xA000
    mov ecx, 4096 / 4
    xor eax, eax
    rep stosd

    ; Clear PD
    mov edi, 0xB000
    mov ecx, 4096 / 4
    xor eax, eax
    rep stosd

    ; Setup entries
    mov dword [0x9000], 0x0000A003    ; PML4[0] -> PDPT
    mov dword [0x9FF8], 0x0000A003    ; PML4[511] -> PDPT
    mov dword [0xA000], 0x0000B003    ; PDPT[0] -> PD
    mov dword [0xB000], 0x00000083    ; PD[0] -> 2MB page
    mov dword [0xB004], 0x00000000
    mov dword [0xBFF0], 0x00000083    ; PD[511] -> 2MB page
    mov dword [0xBFF4], 0x00000000

    popa
    ret


; ==============================================================================
; 64-bit Long Mode
; ==============================================================================
bits 64

long_mode:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x7E00

    ; Clear screen
    push rax
    push rdi
    push rcx
    mov rdi, 0xB8000
    mov rcx, 80 * 25
    mov rax, 0x0720
.clear_loop:
    mov [rdi], ax
    add rdi, 2
    loop .clear_loop
    pop rcx
    pop rdi
    pop rax

    ; Print ready message
    mov rsi, msg_ready
    mov rdi, 0xB8000
    mov ah, 0x0F
.print_loop:
    lodsb
    test al, al
    jz .print_done
    stosw
    jmp .print_loop
.print_done:

    ; Jump to kernel
    mov rdi, 0x10000
    call rdi

    ; Halt if kernel returns
kernel_halt:
    hlt
    jmp kernel_halt

msg_ready:
    db "READY TO BOOT KERNEL", 0


; ==============================================================================
; Kernel Loading Functions
; ==============================================================================
; These functions are called from Stage 2 in real mode

; load_kernel_chs - Load kernel using CHS addressing
; Input: none (uses boot_config.inc defines)
; Output: loads kernel to KERNEL_LOAD_SEGMENT:KERNEL_LOAD_OFFSET
; Clobbers: AX, BX, CX, DX, SI
; Returns: CF=0 on success, CF=1 on error
bits 16
load_kernel_chs:
    pusha

    ; Setup destination address
    mov ax, KERNEL_LOAD_SEGMENT
    mov es, ax
    mov bx, KERNEL_LOAD_OFFSET

    ; For large kernels, we may need multiple reads (BIOS can read max 127 sectors at once)
    ; TODO: Implement multi-read loop for kernels > 127 sectors
    ; For now, assert KERNEL_SECTOR_COUNT <= 127

    mov ax, KERNEL_SECTOR_COUNT
    cmp ax, 127
    ja .kernel_too_large

    ; BIOS disk read parameters
    mov ah, 0x02                    ; read function
    mov al, KERNEL_SECTOR_COUNT     ; number of sectors to read
    mov ch, KERNEL_CHS_CYLINDER     ; cylinder
    mov cl, KERNEL_CHS_SECTOR       ; sector (1-based)
    mov dh, KERNEL_CHS_HEAD         ; head
    mov dl, 0x80                    ; first hard drive

    ; Perform the read
    int 0x13

    ; Check for errors
    jc .read_error

    ; Verify sectors read
    cmp al, KERNEL_SECTOR_COUNT
    jne .read_mismatch

    popa
    clc                             ; clear carry = success
    ret

.kernel_too_large:
    mov si, msg_kernel_too_large
    call print_bios
    ; Fall through to error handler

.read_error:
.read_mismatch:
    popa
    stc                             ; set carry = error
    ret


; ============================================================================
; Error Messages
; ============================================================================
msg_kernel_too_large:
    db "[E3] Kernel too large for single CHS read", 0x0d, 0x0a, 0
