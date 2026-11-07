; user_mode.asm — Ring 3 execution and kernel context restoration

bits 64

section .bss

align 8

user_mode_current_context:
    resq 1

section .text

global user_mode_enter
global user_mode_exit

; ====================================
; ENTER USER MODE
; ====================================

; Arguments:
; RDI - user instruction pointer
; RSI - user stack pointer
; RDX - kernel PML4 physical address
; RCX - user mode context pointer

user_mode_enter:

    ; Preserve callee-saved registers.
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Store the current process context pointer.
    mov [rel user_mode_current_context], rcx

    ; Save the kernel execution context.
    mov [rcx + 0], rsp
    mov [rcx + 8], rdx

    pushfq
    pop rax
    mov [rcx + 16], rax

    ; Prepare user data segments.
    mov ax, 0x2B
    mov ds, ax
    mov es, ax

    ; Construct the IRETQ frame.
    push qword 0x2B
    push rsi
    push qword 0x202
    push qword 0x33
    push rdi

    ; Enter Ring 3.
    iretq


; ====================================
; EXIT USER MODE
; ====================================

user_mode_exit:

    cli

    ; Get the current process context.
    mov rdx, [rel user_mode_current_context]

    ; Restore the kernel address space.
    mov rax, [rdx + 8]
    mov cr3, rax

    ; Restore the original kernel stack.
    mov rsp, [rdx + 0]

    ; Restore kernel data segments.
    mov ax, 0x20
    mov ds, ax
    mov es, ax

    ; Restore callee-saved registers.
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ; Restore the original kernel flags.
    push qword [rdx + 16]
    popfq

    ; Return to the caller of user_mode_enter.
    ret