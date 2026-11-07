; entry.asm — 64-bit kernel entry point

bits 64

section .text.entry

global _start

extern kernel_main
extern __bss_start
extern __bss_end

_start:
    cli
    cld

    ; ================================
    ; PRESERVE BOOT INFO POINTER
    ; ================================

    mov rbx, rdi

    ; ================================
    ; CLEAR BSS SECTION
    ; ================================

    lea rdi, [rel __bss_start]
    lea rcx, [rel __bss_end]

    sub rcx, rdi

    xor eax, eax
    rep stosb

    ; ================================
    ; PREPARE STACK
    ; ================================

    and rsp, -16

    ; ================================
    ; CALL C KERNEL
    ; ================================

    mov rdi, rbx
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang