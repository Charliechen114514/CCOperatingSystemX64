; ============================================================================
; user_enter.asm - User Mode Entry Point for x86_64
; ============================================================================
; This file contains the low-level assembly code for transitioning from
; kernel mode (Ring 0) to user mode (Ring 3).
;
; The key function user_switch_to_usermode() sets up the user context
; and uses iretq to switch to Ring 3.
; ============================================================================

section .text
bits 64

; ============================================================================
; External Functions
; ============================================================================

; No external functions needed for this module

; ============================================================================
; user_context_t Structure Offsets
; ============================================================================
; These must match the user_context_t structure in user.h

struc user_context
    .entry     resq 1    ; virtual_addr_t entry (user RIP)
    .stack_top resq 1    ; virtual_addr_t stack_top (user RSP)
    .cs        resq 1    ; uint64_t cs
    .ss        resq 1    ; uint64_t ss
    .rflags    resq 1    ; uint64_t rflags
endstruc

; ============================================================================
; user_switch_to_usermode - Switch from kernel mode to user mode
; ============================================================================
; This function performs the actual transition from Ring 0 to Ring 3.
; It does NOT return.
;
; C signature: void user_switch_to_usermode(user_context_t* ctx)
;
; Register usage (System V AMD64 ABI):
;   RDI = ctx (pointer to user_context_t)
;
; The context structure contains:
;   - entry: User RIP (entry point)
;   - stack_top: User RSP
;   - cs: User CS selector (USER_CS = 0x1B)
;   - ss: User SS selector (USER_SS = 0x23)
;   - rflags: User RFLAGS (IF=1 for interrupts enabled)
;
; The function:
;   1. Loads the user context from the structure
;   2. Sets up the stack for iretq
;   3. Executes iretq to switch to user mode
; ============================================================================

global user_switch_to_usermode
user_switch_to_usermode:
    ; RDI contains pointer to user_context_t

    ; Load user context into registers
    mov rax, [rdi + user_context.entry]      ; RAX = entry (user RIP)
    mov rbx, [rdi + user_context.stack_top]  ; RBX = stack_top (user RSP)
    mov rcx, [rdi + user_context.cs]         ; RCX = user CS
    mov rdx, [rdi + user_context.ss]         ; RDX = user SS
    mov rsi, [rdi + user_context.rflags]     ; RSI = user RFLAGS

    ; Set up user stack for iretq
    ; iretq expects: SS, RSP, RFLAGS, CS, RIP (pushed in that order)
    ; But we push in reverse order since stack grows down

    ; Push user SS
    push rdx                 ; SS
    ; Push user RSP
    push rbx                 ; RSP
    ; Push user RFLAGS
    push rsi                 ; RFLAGS
    ; Push user CS
    push rcx                 ; CS
    ; Push user RIP
    push rax                 ; RIP

    ; Clear all data segment registers except CS and SS (set by iretq)
    ; This ensures we enter user mode with clean segments
    mov ax, 0x23             ; USER_SS = GDT_USER_DATA | 3
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Switch to user mode via iretq
    ; This will:
    ;   1. Pop RIP into user mode instruction pointer
    ;   2. Pop CS into user mode code segment (automatically sets CPL=3)
    ;   3. Pop RFLAGS into user mode flags
    ;   4. Pop RSP into user mode stack pointer
    ;   5. Pop SS into user mode stack segment
    iretq

; Never returns here

; ============================================================================
; Alternative entry point for testing - jump to simple user code
; ============================================================================
; This is a simple test function that jumps to a hardcoded user mode
; routine. It can be used for testing before full user process support
; is implemented.
; ============================================================================

; global user_test_jump
; extern user_test_function
;
; user_test_jump:
;     ; Set up a simple user mode context
;     mov rax, user_test_function    ; User RIP
;     mov rbx, USER_STACK_TOP         ; User RSP
;     mov rcx, USER_CS                ; User CS (0x1B)
;     mov rdx, USER_SS                ; User SS (0x23)
;     mov rsi, 0x202                  ; RFLAGS (IF=1)
;
;     ; Set up iretq stack frame
;     push rdx    ; SS
;     push rbx    ; RSP
;     push rsi    ; RFLAGS
;     push rcx    ; CS
;     push rax    ; RIP
;
;     ; Clear segment registers
;     mov ax, USER_SS
;     mov ds, ax
;     mov es, ax
;     mov fs, ax
;     mov gs, ax
;
;     iretq
