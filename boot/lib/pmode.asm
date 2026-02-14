; Protected Mode Library
; Provides functions for setting up protected mode and page tables

; Setup page tables for long mode
; Input: none
; Output: page tables at 0x9000 (PML4), 0xA000 (PDPT), 0xB000 (PD)
; Clobbers: EAX, ECX, EDI
bits 32
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
