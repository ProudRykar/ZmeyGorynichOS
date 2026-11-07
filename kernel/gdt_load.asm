; gdt_load.asm — Load GDT and reload segment registers

bits 64

section .text

global gdt_load

gdt_load:
    ; Load the new GDT.

    lgdt [rdi]

    ; Reload CS using a far return.

    push 0x18

    lea rax, [rel .reload_cs]
    push rax

    retfq

.reload_cs:

    ; Reload kernel data segments.

    mov ax, 0x20

    mov ds, ax
    mov es, ax
    mov ss, ax

    xor eax, eax

    mov fs, ax
    mov gs, ax

    ret