; ============================================================================
; syscall.asm - System call entry/exit stubs for x86_64
; ============================================================================
; This file contains low-level assembly code for syscall/sysret instruction
; support and traditional int 0x80 fallback.
;
; Calling convention (System V AMD64 ABI):
;   RAX = System call number
;   RDI, RSI, RDX, R10, R8, R9 = Arguments (up to 6)
;   RAX = Return value
; ============================================================================

section .text
bits 64

; ============================================================================
; syscall Instruction Entry Point
; ============================================================================

; global syscall_handler
; Entry point for syscall instruction
; Stack layout on entry (pushed by hardware):
;   [RSP]    = RCX (saved user RIP)
;   [RSP+8]  = R11 (saved user RFLAGS)
;   (Note: syscall does NOT push SS/RSP like interrupts do)
global syscall_handler
extern syscall_dispatch
syscall_handler:
    ; At this point:
    ;   RCX = user RIP (saved by syscall)
    ;   R11 = user RFLAGS (saved by syscall)
    ;   RAX = syscall number
    ;   RDI, RSI, RDX, R10, R8, R9 = arguments

    ; Save all registers (syscall preserves RCX and R11, but we save everything)
    push rax                ; Save syscall number
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; Save RCX and R11 separately (these are user RIP/RFLAGS)
    mov r15, rcx            ; Save user RIP in R15
    mov r14, r11            ; Save user RFLAGS in R14

    ; Load kernel data segments
    mov ax, 0x10            ; GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Align stack to 16 bytes before C call
    ; We pushed 7 registers = 56 bytes (56 mod 16 = 8)
    ; Need to push 8 more bytes to align
    push qword 0

    ; Prepare syscall_frame_t on stack
    sub rsp, 48             ; Allocate syscall_frame_t

    ; Construct frame
    mov qword [rsp], rax    ; syscall_number (original RAX)
    mov qword [rsp+8], rdi  ; arg0
    mov qword [rsp+16], rsi ; arg1
    mov qword [rsp+24], rdx ; arg2
    mov qword [rsp+32], r10 ; arg3
    mov qword [rsp+40], r8  ; arg4
    ; Note: R9 is arg5, passed in register

    ; Call C dispatcher
    ; int64_t syscall_dispatch(syscall_frame_t* frame, uint64_t arg5)
    mov rdi, rsp            ; RDI = frame pointer
    mov rsi, r9             ; RSI = arg5
    call syscall_dispatch

    ; Return value in RAX, check for error
    ; If negative, it's an error code (like -EINVAL)

    ; Clean up frame
    add rsp, 48

    ; Clean up alignment
    add rsp, 8

    ; Restore user RIP and RFLAGS
    mov rcx, r15            ; Restore user RIP to RCX
    mov r11, r14            ; Restore user RFLAGS to R11

    ; Restore saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop rax                 ; Restore syscall number (discard)

    ; Return value already in RAX from syscall_dispatch

    ; Return to user mode using sysret
    ; sysretq loads RCX -> RIP, R11 -> RFLAGS
    ; and CS = STAR[63:48], SS = STAR[63:48] + 8
    sysretq

; ============================================================================
; int 0x80 Entry Point (Legacy Fallback)
; ============================================================================

; global int0x80_handler
; Entry point for int 0x80 system call (legacy interface)
; Uses standard interrupt stack frame
global int0x80_handler
extern syscall_dispatch_int80
int0x80_handler:
    ; Standard interrupt entry - CPU pushes:
    ; SS, RSP, RFLAGS, CS, RIP, (error code if applicable)

    ; Save all registers
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

    ; Save segments
    mov ax, ds
    push rax
    mov ax, es
    push rax
    mov ax, fs
    push rax
    mov ax, gs
    push rax

    ; Load kernel segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Align stack (19 registers saved = 152 bytes, 152 mod 16 = 8)
    push qword 0

    ; Stack layout at this point (after alignment push):
    ; [rsp+0]  = alignment
    ; [rsp+8]  = GS
    ; [rsp+16] = FS
    ; [rsp+24] = ES
    ; [rsp+32] = DS
    ; [rsp+40] = R15
    ; [rsp+48] = R14
    ; [rsp+56] = R13
    ; [rsp+64] = R12
    ; [rsp+72] = R11
    ; [rsp+80] = R10
    ; [rsp+88] = R9
    ; [rsp+96] = R8
    ; [rsp+104] = RBP
    ; [rsp+112] = RDI
    ; [rsp+120] = RSI
    ; [rsp+128] = RDX
    ; [rsp+136] = RCX
    ; [rsp+144] = RBX
    ; [rsp+152] = RAX (syscall number)
    ; [rsp+160] = dummy error code (pushed by stub)
    ; [rsp+168] = vector (128 for int 0x80, pushed by stub)
    ; [rsp+176] = alignment (pushed by stub)
    ; [rsp+184] = RIP
    ; [rsp+192] = CS
    ; [rsp+200] = RFLAGS
    ; [rsp+208] = RSP (user)
    ; [rsp+216] = SS (user)

    ; Build syscall frame
    sub rsp, 56             ; syscall_frame_int80_t

    ; Syscall number from original RAX
    mov rax, [rsp+56+152]
    mov [rsp], rax          ; syscall_number

    ; Arguments from saved registers
    mov rax, [rsp+56+112]   ; Original RDI
    mov [rsp+8], rax        ; arg0

    mov rax, [rsp+56+120]   ; Original RSI
    mov [rsp+16], rax       ; arg1

    mov rax, [rsp+56+128]   ; Original RDX
    mov [rsp+24], rax       ; arg2

    mov rax, [rsp+56+80]    ; Original R10
    mov [rsp+32], rax       ; arg3

    mov rax, [rsp+56+96]    ; Original R8
    mov [rsp+40], rax       ; arg4

    mov rax, [rsp+56+88]    ; Original R9
    mov [rsp+48], rax       ; arg5

    ; Call C dispatcher
    ; int64_t syscall_dispatch_int80(syscall_frame_int80_t* frame)
    mov rdi, rsp
    call syscall_dispatch_int80

    ; Save return value
    mov r15, rax

    ; Clean up
    add rsp, 56
    add rsp, 8

    ; Restore segments
    pop rax
    mov gs, ax
    pop rax
    mov fs, ax
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    ; Restore registers
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

    ; Put return value in RAX
    mov rax, r15

    ; Clean up dummy error code, vector, alignment (24 bytes)
    add rsp, 24

    iretq

; ============================================================================
; Helper: Enable syscall/sysret instructions
; ============================================================================

; global syscall_enable
; Enable SCE (System Call Extension) bit in CR4
global syscall_enable
extern klog_trace
extern sync_serial_puts
syscall_enable:
    ; Read current CR4 value
    mov rax, cr4

    ; Check if SCE bit is already set
    test rax, (1 << 11)
    jnz .already_set

    ; Set SCE bit (bit 11)
    or rax, (1 << 11)

    ; Write back to CR4
    mov cr4, rax
    ret

.already_set:
    ; SCE bit already set, just return
    ret
