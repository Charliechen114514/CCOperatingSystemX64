; ==============================================================================
; CCOS - GDT/TSS Assembly Helpers
; ==============================================================================
; This file contains assembly functions for loading the GDT and TSS.
; ==============================================================================

section .text
bits 64

; ==============================================================================
; gdt_flush - Load the GDT
; ==============================================================================
; @param rdi: Pointer to gdt_ptr_t structure
global gdt_flush
gdt_flush:
    lgdt [rdi]          ; Load GDT pointer

    ; Reload segment registers
    mov ax, 0x10        ; Kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS
    push 0x08           ; Kernel code selector
    push .reload_cs
    retfq                ; Far return

.reload_cs:
    ret

; ==============================================================================
; tss_load - Load the TSS
; ==============================================================================
; Uses selector 0x28 (GDT_TSS)
global tss_load
tss_load:
    mov ax, 0x28        ; TSS selector
    ltr ax              ; Load Task Register
    ret
