; ============================================================================
; interrupt.s
; Interrupt and exception entry/exit stubs for x86_64
;
; This file contains the low-level assembly code that handles interrupt and
; exception entry. Each stub saves the processor state, calls the C handler,
; and then restores the state before returning.
;
; The x86_64 interrupt stack frame:
;   (higher addresses)
;   | SS     | (only if CPL change)
;   | RSP    | (only if CPL change)
;   | RFLAGS |
;   | CS     |
;   | RIP    |
;   | Error Code | (only for some exceptions)
;   | ...    |
;   (lower addresses)
; ============================================================================

; We use a macro to avoid repetition - this generates all 32 ISR stubs
%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    ; Push 0 as dummy error code (CPU doesn't push one for these exceptions)
    push qword 0
    ; Push the interrupt vector number
    push qword %1
    ; Align stack to 16 bytes - CPU pushes RIP(8)+CS(8)+RFLAGS(8)=24 bytes
    ; 24 + 16 (our pushes) = 40, which is 8 mod 16. Push another 8 to align.
    push qword 0
    ; Jump to the common interrupt handler
    jmp interrupt_common
%endmacro

; Macro for ISRs that push an error code
%macro ISR_ERRCODE 1
  global isr%1
  isr%1:
    ; CPU already pushed error code, just push the vector number
    push qword %1
    ; Align stack to 16 bytes - CPU pushes error(8)+RIP(8)+CS(8)+RFLAGS(8)=32 bytes
    ; 32 + 8 (our push) = 40, which is 8 mod 16. Push another 8 to align.
    push qword 0
    ; Jump to the common interrupt handler
    jmp interrupt_common
%endmacro

; Macro for IRQ handlers (no error code)
%macro IRQ 2
  global irq%1
  irq%1:
    push qword 0
    push qword %2
    ; Align stack to 16 bytes - same alignment as ISR_NOERRCODE
    push qword 0
    jmp interrupt_common
%endmacro

section .text

; ============================================================================
; ISR Stubs (Exception Handlers 0-31)
; ============================================================================

; Exceptions without error code
ISR_NOERRCODE 0   ; Divide Error (#DE)
ISR_NOERRCODE 1   ; Debug (#DB)
ISR_NOERRCODE 2   ; Non-Maskable Interrupt
ISR_NOERRCODE 3   ; Breakpoint (#BP)
ISR_NOERRCODE 4   ; Overflow (#OF)
ISR_NOERRCODE 5   ; BOUND Range Exceeded (#BR)
ISR_NOERRCODE 6   ; Invalid Opcode (#UD)
ISR_NOERRCODE 7   ; Device Not Available (#NM)

; Double Fault - HAS ERROR CODE
ISR_ERRCODE   8   ; Double Fault (#DF)

ISR_NOERRCODE 9   ; Coprocessor Segment Overrun

; Invalid TSS - HAS ERROR CODE
ISR_ERRCODE   10  ; Invalid TSS (#TS)

; Segment Not Present - HAS ERROR CODE
ISR_ERRCODE   11  ; Segment Not Present (#NP)

; Stack-Segment Fault - HAS ERROR CODE
ISR_ERRCODE   12  ; Stack-Segment Fault (#SS)

; General Protection Fault - HAS ERROR CODE
ISR_ERRCODE   13  ; General Protection Fault (#GP)

; Page Fault - HAS ERROR CODE
ISR_ERRCODE   14  ; Page Fault (#PF)

ISR_NOERRCODE 15  ; x87 FPU Error (#MF)
ISR_NOERRCODE 16  ; Alignment Check (#AC) - can have error code in some cases
ISR_NOERRCODE 17  ; Machine Check (#MC)
ISR_NOERRCODE 18  ; SIMD Floating-Point Exception (#XM)
ISR_NOERRCODE 19  ; Virtualization Exception (#VE)
ISR_NOERRCODE 20  ; Control Protection Exception (#CP)
ISR_NOERRCODE 21  ; Reserved
ISR_NOERRCODE 22  ; Reserved
ISR_NOERRCODE 23  ; Reserved
ISR_NOERRCODE 24  ; Reserved
ISR_NOERRCODE 25  ; Reserved
ISR_NOERRCODE 26  ; Reserved
ISR_NOERRCODE 27  ; Reserved
ISR_NOERRCODE 28  ; Reserved
ISR_NOERRCODE 29  ; SSE Exception (#XF)
ISR_NOERRCODE 30  ; Reserved
ISR_NOERRCODE 31  ; Reserved

; ============================================================================
; IRQ Stubs (32-47)
; ============================================================================

IRQ 0,  32    ; Timer
IRQ 1,  33    ; Keyboard
IRQ 2,  34    ; Cascade
IRQ 3,  35    ; COM2
IRQ 4,  36    ; COM1
IRQ 5,  37    ; LPT2
IRQ 6,  38    ; Floppy
IRQ 7,  39    ; LPT1
IRQ 8,  40    ; RTC
IRQ 9,  41    ; Free
IRQ 10, 42    ; Free
IRQ 11, 43    ; Free
IRQ 12, 44    ; PS/2 Mouse
IRQ 13, 45    ; FPU
IRQ 14, 46    ; Primary ATA
IRQ 15, 47    ; Secondary ATA

; ============================================================================
; Common Interrupt Handler
; ============================================================================
; This is the common interrupt handler that all stubs jump to.
; Stack layout on entry (with alignment fix):
;   [rsp]      = dummy/error code (pushed by stub or CPU)
;   [rsp+8]    = vector number (pushed by stub)
;   [rsp+16]   = alignment dummy (pushed by stub for 16-byte alignment)
;   [rsp+24]   = RIP (pushed by CPU)
;   [rsp+32]   = CS (pushed by CPU)
;   [rsp+40]   = RFLAGS (pushed by CPU)
;   [rsp+48]   = RSP (pushed by CPU if CPL change)
;   [rsp+56]   = SS (pushed by CPU if CPL change)
; ============================================================================

extern interrupt_handler

interrupt_common:
    ; Save all general-purpose registers
    ; We need to preserve the full CPU state
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Save segment registers
    mov ax, ds
    push rax
    mov ax, es
    push rax
    mov ax, fs
    push rax
    mov ax, gs
    push rax

    ; Load kernel data segment (bootloader gdt_data64 selector = 0x20)
    ; This should match your GDT setup
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; At this point, the stack looks like (pushed in order, growing down):
    ;   [rsp+0]   = RAX  (first push)
    ;   [rsp+8]   = RBX
    ;   [rsp+16]  = RCX
    ;   [rsp+24]  = RDX
    ;   [rsp+32]  = RSI
    ;   [rsp+40]  = RDI
    ;   [rsp+48]  = RBP
    ;   [rsp+56]  = R8
    ;   [rsp+64]  = R9
    ;   [rsp+72]  = R10
    ;   [rsp+80]  = R11
    ;   [rsp+88]  = R12
    ;   [rsp+96]  = R13
    ;   [rsp+104] = R14
    ;   [rsp+112] = R15
    ;   [rsp+120] = DS (pushed as rax)
    ;   [rsp+128] = ES (pushed as rax)
    ;   [rsp+136] = FS (pushed as rax)
    ;   [rsp+144] = GS (pushed as rax) ← RSP points here
    ;   ---
    ;   [rsp+152] = error code / dummy (pushed by ISR stub)
    ;   [rsp+160] = vector number (pushed by ISR stub)
    ;   [rsp+168] = alignment dummy (pushed by ISR stub)
    ;   [rsp+176] = RIP (pushed by CPU)
    ;   [rsp+184] = CS (pushed by CPU)
    ;   [rsp+192] = RFLAGS (pushed by CPU)
    ;   [rsp+200] = RSP (pushed by CPU, only if CPL change)
    ;   [rsp+208] = SS (pushed by CPU, only if CPL change)

    ; Prepare arguments for C handler
    ; void interrupt_handler(uint64_t vector, uint64_t error_code, interrupt_frame_t* frame)
    ;
    ; The interrupt_frame_t expects: {error_code, rip, cs, rflags, rsp, ss}
    ; But the stack has: error_code, vector, alignment, rip, cs, rflags, (rsp, ss if CPL changed)
    ;
    ; We need to construct a proper frame. We'll use the stack space above our saved regs.
    ;
    ; Stack layout at this point:
    ;   [rsp+152] = error_code (stub push)
    ;   [rsp+160] = vector (stub push)
    ;   [rsp+168] = alignment dummy (stub push)
    ;   [rsp+176] = RIP (CPU push)
    ;   [rsp+184] = CS (CPU push)
    ;   [rsp+192] = RFLAGS (CPU push)
    ;   [rsp+200] = RSP (CPU push, only if CPL changed)
    ;   [rsp+208] = SS (CPU push, only if CPL changed)

    ; Allocate space for interrupt_frame_t on stack
    sub rsp, 48             ; 6 * 8 = 48 bytes for the struct

    ; Construct the frame struct
    mov rax, [rsp+48+152]   ; Original error_code position
    mov [rsp], rax          ; frame->error_code

    mov rax, [rsp+48+176]   ; Original RIP position (skip over vector and alignment)
    mov [rsp+8], rax        ; frame->rip

    mov rax, [rsp+48+184]   ; CS
    mov [rsp+16], rax       ; frame->cs

    mov rax, [rsp+48+192]   ; RFLAGS
    mov [rsp+24], rax       ; frame->rflags

    mov rax, [rsp+48+200]   ; RSP (may be garbage if CPL didn't change)
    mov [rsp+32], rax       ; frame->rsp

    mov rax, [rsp+48+208]   ; SS
    mov [rsp+40], rax       ; frame->ss

    ; Set up function arguments
    mov rdi, [rsp+48+160]   ; RDI = vector number
    mov rsi, [rsp+48+152]   ; RSI = error_code
    mov rdx, rsp            ; RDX = pointer to constructed frame

    ; Call the C handler
    cld                     ; Clear direction flag (should be already)
    call interrupt_handler

    ; Clean up the temporary frame (48 bytes)
    add rsp, 48

    ; Restore segment registers
    pop rax
    mov gs, ax
    pop rax
    mov fs, ax
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    ; Restore all general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Clean up the error code, vector number, and alignment dummy
    add rsp, 24

    ; Use iretq to return from interrupt
    ; This pops RIP, CS, RFLAGS, RSP, and SS
    iretq

; ============================================================================
; IDT Load Function
; ============================================================================
; void idt_load(uint64_t idt_ptr)
; Loads the IDT using the lidt instruction
; ============================================================================

global idt_load
idt_load:
    ; RDI contains the address of the idt_ptr structure
    lidt [rdi]
    ret
