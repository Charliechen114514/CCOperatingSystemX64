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
;   5. Returns to next process
; ============================================================================

section .text

; ============================================================================
; External Functions
; ============================================================================

extern tss_set_kernel_stack_ctx
extern vmm_load_pml4

; ============================================================================
; PCB Structure Offsets
; ============================================================================
; These must match the pcb_t structure in process.h

; Offsets within pcb_t structure
struc pcb
    .pid                resd 1    ; int32_t pid
    .ppid               resd 1    ; int32_t ppid
    .state              resd 1    ; process_state_t state
    .exit_code          resd 1    ; int32_t exit_code
    .run_list           resq 2    ; list_head (next, prev)
    .siblings           resq 2    ; list_head
    .children           resq 2    ; list_head
    .zombie_children    resq 2    ; list_head
    .mm_pml4            resd 1    ; physical_addr_t (from memory_context_t)
    .mm_brk             resq 1    ; virtual_addr_t
    .mm_stack_start     resq 1    ; virtual_addr_t
    .cpu_ctx            resq 1    ; cpu_context_t*
    .trap_frame         resq 1    ; trap_frame_t*
    .kernel_stack       resq 1    ; virtual_addr_t
    .kernel_stack_base  resq 1    ; virtual_addr_t
    .parent             resq 1    ; struct pcb*
    .start_time         resq 1    ; uint64_t
    .comm               resb 16   ; char[16]
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

    ; Save RSP to prev->cpu_ctx->rsp
    ; Current RSP points to the saved R15 on stack
    mov [rax + pcb.cpu_ctx + cpu_ctx.rsp], rsp

    ; Save registers to prev->cpu_ctx
    mov [rax + pcb.cpu_ctx + cpu_ctx.rbx], rbx
    mov [rax + pcb.cpu_ctx + cpu_ctx.rbp], rbp
    mov [rax + pcb.cpu_ctx + cpu_ctx.r12], r12
    mov [rax + pcb.cpu_ctx + cpu_ctx.r13], r13
    mov [rax + pcb.cpu_ctx + cpu_ctx.r14], r14
    mov [rax + pcb.cpu_ctx + cpu_ctx.r15], r15

    ; Save RIP (the return address pushed by the call)
    ; It's at [rsp + 6*8] = rsp + 48 (after we pushed 6 registers)
    mov r15, [rsp + 48]
    mov [rax + pcb.cpu_ctx + cpu_ctx.rip], r15

    ; ================================================================
    ; Load next process state
    ; ================================================================

    ; Update TSS.rsp0 with next->kernel_stack
    mov rdi, [rsi + pcb.kernel_stack]
    call tss_set_kernel_stack_ctx

    ; Always switch CR3 to next->mm.pml4_phys
    ; (user decision: simplify by always switching)
    mov rdi, [rsi + pcb.mm_pml4]
    call vmm_load_pml4

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

    ; Restore callee-saved registers and return
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

; ============================================================================
; switch_to_first - Switch from kernel to first process
; ============================================================================
; C signature: void switch_to_first(pcb_t* first)
;
; Special case for switching from kernel initialization to first process.
; This is used when there's no previous process to save.
;
; RDI = first (pcb_t*)
; ============================================================================

global switch_to_first
switch_to_first:
    ; Update TSS.rsp0 with first->kernel_stack
    mov rsi, rdi                ; Save first pointer
    mov rdi, [rsi + pcb.kernel_stack]
    push rsi                    ; Save first pointer for later
    call tss_set_kernel_stack_ctx
    pop rsi                     ; Restore first pointer

    ; Switch CR3 to first->mm.pml4_phys
    mov rdi, [rsi + pcb.mm_pml4]
    push rsi                    ; Save first pointer
    call vmm_load_pml4
    pop rsi                     ; Restore first pointer

    ; Restore registers from first->cpu_ctx
    mov rax, [rsi + pcb.cpu_ctx]
    mov rbx, [rax + cpu_ctx.rbx]
    mov rbp, [rax + cpu_ctx.rbp]
    mov r12, [rax + cpu_ctx.r12]
    mov r13, [rax + cpu_ctx.r13]
    mov r14, [rax + cpu_ctx.r14]
    mov r15, [rax + cpu_ctx.r15]

    ; Restore RSP
    mov rsp, [rax + cpu_ctx.rsp]

    ; Load RIP and jump to it
    mov rax, [rax + cpu_ctx.rip]
    jmp rax                     ; Jump to process entry point
