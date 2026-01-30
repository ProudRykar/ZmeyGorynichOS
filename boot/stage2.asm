[org 0x8000]
bits 16

start:
    mov si, hello
.print_loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp .print_loop

.done:
    mov ah, 0x00
    int 0x16
    cli
    hlt
    jmp $

hello db "stage2 @ 0x8000 - real mode OK. Next: implement long mode loader...", 0