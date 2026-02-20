; ============================================================================
; switch.s - Context Switch Assembly for x86_64
; ============================================================================
; This file contains the low-level context switch implementation for process
; management. The switch_context function saves the current process state and
; restores the next process state.
;
; C signature: void switch_context(pcb_t** prev, pcb_t* next)
;
; Register usage (System V AMD64 ABI):
;   RDI = &prev (pointer to pcb_t*)
;   RSI = next (pcb_t*)
;
; The function:
;   1. Saves callee-saved registers (RBX, RBP, R12-R15) and RSP to prev->cpu_ctx
;   2. Updates TSS.rsp0 to next->kernel_stack
;   3. Switches CR3 to next->mm.pml4_phys
;   4. Restores registers from next->cpu_ctx
;   5. Returns to next process (or calls trap frame restore if needed)
; ============================================================================

section .text

; ============================================================================
; External Functions
; ============================================================================

extern tss_set_kernel_stack_ctx
extern vmm_load_pml4
extern scheduler
extern panic

; ============================================================================
; PCB Structure Offsets
; ============================================================================
; These must match the pcb_t structure in process.h exactly!

struc pcb
    .pid                resd 1    ; offset   0
    .ppid               resd 1    ; offset   4
    .state              resd 1    ; offset   8
    .exit_code          resd 1    ; offset  12
    .run_list           resq 2    ; offset  16
    .siblings           resq 2    ; offset  32
    .children           resq 2    ; offset  48
    .zombie_children    resq 2    ; offset  64
    .sched_entity       resb 56   ; offset  80
    .mm_pml4            resq 1    ; offset 136
    .mm_brk             resq 1    ; offset 144
    .mm_stack_start     resq 1    ; offset 152
    .cpu_ctx            resq 1    ; offset 160
    .trap_frame         resb 176  ; offset 168
    .kernel_stack       resq 1    ; offset 344
    .kernel_stack_base  resq 1    ; offset 352
    .is_user_mode       resb 1    ; offset 360
    .in_user_mode_when_interrupted resb 1 ; offset 361
    .has_saved_trap_frame resb 1  ; offset 362
    .pad_user_mode      resb 1    ; offset 363
    .user_stack         resq 1    ; offset 364
    .user_stack_size    resq 1    ; offset 372
    .parent             resq 1    ; offset 380
    .start_time         resq 1    ; offset 388
    .comm               resb 16   ; offset 396
    .pad_comm           resb 4    ; offset 412 (padding to align tgid at 416)
    .tgid               resd 1    ; offset 416
    .is_thread          resb 1    ; offset 420
    .pad_thread         resb 3    ; offset 421 (padding for thread_list alignment)
    .thread_list        resq 2    ; offset 424
    .thread_group       resq 2    ; offset 440
    .mm_refcount        resd 1    ; offset 456 (atomic_t = 4 bytes)
    .pad_refcount       resb 4    ; offset 460 (padding for join_waiters alignment)
    .join_waiters       resq 1    ; offset 464
    .detached           resb 1    ; offset 472
    .pad_detached       resb 7    ; offset 473 (padding for return_value alignment)
    .return_value       resq 1    ; offset 480
    .thread_entry       resq 1    ; offset 488
    .thread_arg         resq 1    ; offset 496
    .preempt_count      resd 1    ; offset 504
    .has_run            resb 1    ; offset 508
    .pad_has_run        resb 3    ; offset 509 (padding for magic alignment)
    .magic              resd 1    ; offset 512
endstruc

; Offsets within cpu_context_t structure
struc cpu_ctx
    .rbx    resq 1
    .rbp    resq 1
    .r12    resq 1
    .r13    resq 1
    .r14    resq 1
    .r15    resq 1
    .rsp    resq 1
    .rip    resq 1
endstruc

; ============================================================================
; switch_context - Save current state, restore next state
; ============================================================================

global switch_context
switch_context:
    ; ================================================================
    ; Save current process state
    ; ================================================================
    ; RDI = &prev (pointer to pcb_t*)
    ; RSI = next (pcb_t*)

    ; Save callee-saved registers (System V AMD64 ABI)
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; Get the actual prev PCB pointer
    mov rax, [rdi]           ; rax = *prev (pcb_t*)

    ; Get the cpu_ctx pointer first (cpu_ctx is a POINTER, not embedded!)
    mov rcx, [rax + pcb.cpu_ctx]    ; rcx = prev->cpu_ctx (pointer!)

    ; Save RSP to prev->cpu_ctx->rsp
    ; Current RSP points to the saved R15 on stack
    mov [rcx + cpu_ctx.rsp], rsp

    ; Save registers to prev->cpu_ctx
    mov [rcx + cpu_ctx.rbx], rbx
    mov [rcx + cpu_ctx.rbp], rbp
    mov [rcx + cpu_ctx.r12], r12
    mov [rcx + cpu_ctx.r13], r13
    mov [rcx + cpu_ctx.r14], r14
    mov [rcx + cpu_ctx.r15], r15

    ; Save RIP (the return address pushed by the call)
    ; It's at [rsp + 6*8] = rsp + 48 (after we pushed 6 registers)
    mov r15, [rsp + 48]
    mov [rcx + cpu_ctx.rip], r15

    ; ================================================================
    ; Load next process state
    ; ================================================================

    ; Save RSI (next pointer) since function calls will clobber it
    push rsi

    ; Update scheduler.current
    lea rax, [rel scheduler + 16]
    mov [rax], rsi             ; scheduler.current = next

    ; Update TSS.rsp0 with next->kernel_stack
    mov rdi, [rsi + pcb.kernel_stack]
    call tss_set_kernel_stack_ctx

    ; Restore RSI (next pointer)
    pop rsi

    ; Save RSI again before second function call
    push rsi

    ; Always switch CR3 to next->mm.pml4_phys
    ; (user decision: simplify by always switching)
    mov rdi, [rsi + pcb.mm_pml4]
    call vmm_load_pml4

    ; Restore RSI (next pointer)
    pop rsi

    ; Restore registers from next->cpu_ctx
    mov rax, [rsi + pcb.cpu_ctx]    ; rax = next->cpu_ctx
    mov rbx, [rax + cpu_ctx.rbx]
    mov rbp, [rax + cpu_ctx.rbp]
    mov r12, [rax + cpu_ctx.r12]
    mov r13, [rax + cpu_ctx.r13]
    mov r14, [rax + cpu_ctx.r14]
    mov r15, [rax + cpu_ctx.r15]

    ; Restore RSP
    mov rsp, [rax + cpu_ctx.rsp]

    ; Restore callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; ================================================================
    ; Check if we need to restore trap frame and return to user mode
    ; ================================================================
    ; This is the ONLY extra logic we add to the basic switch_context
    ; If the next process has_saved_trap_frame=1, we need to iretq instead of ret
    mov rdi, [rel scheduler + 16]
    test rdi, rdi
    jz .normal_return

    cmp byte [rdi + pcb.has_saved_trap_frame], 1
    jne .normal_return

    ; Need to restore trap_frame and iretq to user mode
    call trap_frame_restore_and_iret
    ; Never returns

.normal_return:
    ret

; ============================================================================
; switch_to_first - Switch from kernel to first process
; ============================================================================
; C signature: void switch_to_first(pcb_t* first);
;
; Special case for switching from kernel initialization to first process.
; This is used when there's no previous process to save.
;
; RDI = first (pcb_t*)
; ============================================================================

global switch_to_first
switch_to_first:
    ; Note: scheduler.current is already set by the C code before calling us
    ; RDI = first (pcb_t*)

    ; ================================================================
    ; Extract all values we need BEFORE modifying any state
    ; ================================================================
    mov rsi, rdi                ; RSI = first pointer

    mov rax, [rsi + pcb.cpu_ctx]

    ; Extract callee-saved registers
    mov rbx, [rax + cpu_ctx.rbx]
    mov rbp, [rax + cpu_ctx.rbp]
    mov r12, [rax + cpu_ctx.r12]
    mov r13, [rax + cpu_ctx.r13]
    mov r14, [rax + cpu_ctx.r14]
    mov r15, [rax + cpu_ctx.r15]

    ; Extract RIP and other values
    mov rcx, [rax + cpu_ctx.rip]     ; RCX = entry point
    mov r8,  [rsi + pcb.kernel_stack] ; R8 = kernel_stack (for TSS)
    mov r9, [rsi + pcb.mm_pml4]      ; R9 = pml4_phys

    ; ================================================================
    ; Update system state
    ; ================================================================

    ; Update TSS.rsp0 with first->kernel_stack
    mov rdi, r8
    call tss_set_kernel_stack_ctx

    ; Mark the task as having run (has_run = 1)
    mov byte [rsi + pcb.has_run], 1

    ; Switch CR3 to first->mm_pml4_phys
    mov rdi, r9
    call vmm_load_pml4

    ; ================================================================
    ; Build iretq frame on the NEW process's stack
    ; ================================================================
    ; For same-privilege iretq (kernel to kernel), we only need:
    ; - RIP (entry point)
    ; - CS (kernel code segment = 0x08)
    ; - RFLAGS (with interrupts enabled)

    ; Get new RSP (we saved it earlier but need to re-read to be safe)
    mov rax, [rsi + pcb.cpu_ctx]
    mov rdx, [rax + cpu_ctx.rsp]     ; RDX = new stack pointer

    ; Switch to new process's stack
    mov rsp, rdx

    ; Push RFLAGS with interrupts enabled
    pushfq
    pop qword rax
    or rax, 0x200               ; Set interrupt flag (bit 9)
    push rax

    ; Push CS (kernel code segment)
    push qword 0x08

    ; Push RIP (entry point)
    push rcx

    ; ================================================================
    ; Execute iretq - all registers are already restored
    ; ================================================================
    iretq

; ============================================================================
; trap_frame_restore_and_iret - Restore trap frame and iretq to user mode
; ============================================================================
; C signature: void trap_frame_restore_and_iret(void) __attribute__((noreturn))
;
;
; This function restores all registers from current PCB's trap_frame
; and uses iretq to return to user mode.
;
; This is called from switch_context when has_saved_trap_frame = 1.
; ============================================================================

global trap_frame_restore_and_iret
trap_frame_restore_and_iret:
    ; Get current PCB
    mov rdi, [rel scheduler + 16]
    test rdi, rdi
    jz .invalid_pcb

    ; Get trap_frame address
    lea rsi, [rdi + pcb.trap_frame]   ; rsi = &trap_frame

    ; === Strategy: Build iretq frame FIRST, then restore registers ===
    ; We need to save rsi (trap_frame pointer) temporarily
    ; Use r15 as temporary storage (we'll restore it later anyway)
    mov r15, rsi                       ; r15 = &trap_frame

    ; Build iretq frame on stack: SS, RSP, RFLAGS, CS, RIP
    push qword [r15 + 168]    ; ss (offset 168)
    push qword [r15 + 160]    ; rsp (offset 160)
    push qword [r15 + 152]    ; rflags (offset 152)
    push qword [r15 + 144]    ; cs (offset 144)
    push qword [r15 + 136]    ; rip (offset 136)

    ; === Restore all general purpose registers ===
    ; Offsets are relative to trap_frame start (r15)
    mov rax, [r15 + 0]       ; rax
    mov rbx, [r15 + 8]       ; rbx
    mov rcx, [r15 + 16]      ; rcx
    mov rdx, [r15 + 24]      ; rdx
    mov rbp, [r15 + 48]      ; rbp
    mov r8,  [r15 + 56]      ; r8
    mov r9,  [r15 + 64]      ; r9
    mov r10, [r15 + 72]      ; r10
    mov r11, [r15 + 80]      ; r11
    mov r12, [r15 + 88]      ; r12
    mov r13, [r15 + 96]      ; r13
    mov r14, [r15 + 104]     ; r14

    ; Restore RDI, RSI, R15 last (since we used them)
    mov rdi, [r15 + 40]      ; rdi
    mov rsi, [r15 + 32]      ; rsi
    mov r15, [r15 + 112]     ; r15

    ; === Clear has_saved_trap_frame ===
    ; Get PCB again
    mov rbx, [rel scheduler + 16]
    mov byte [rbx + pcb.has_saved_trap_frame], 0

    ; Load kernel data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax

    ; Return to user mode via iretq
    iretq

.invalid_pcb:
    ; Should not happen
    mov rdi, str_invalid_pcb
    jmp panic

global restore_from_trap_frame_and_iret
restore_from_trap_frame_and_iret:
    jmp trap_frame_restore_and_iret

section .rodata
str_invalid_pcb: db "Invalid PCB in trap_frame_restore_and_iret", 0
