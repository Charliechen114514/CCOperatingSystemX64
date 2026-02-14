; Long Mode Library
; Provides functions for 64-bit long mode

bits 64

; Clear screen (VGA text mode 80x25)
; Input: none
; Output: clears VGA text buffer
; Clobbers: RAX, RDI, RCX
clear_screen64:
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
; Input: RSI = pointer to null-terminated string
; Output: prints to VGA buffer
; Clobbers: RAX, RDI, RSI
print_string64:
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
