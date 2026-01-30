[org 0x7c00]
bits 16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    lea si, [dap]
    mov ah, 0x42 
    int 0x13
    jc disk_error

    jmp 0x0000:0x8000

disk_error:
    mov si, disk_err
.print_err:
    lodsb
    or al, al
    jz .hang
    mov ah, 0x0E
    int 0x10
    jmp .print_err
.hang:
    cli
    hlt
    jmp .hang

dap:
    db 0x10, 0x00
    dw 4
    dw 0x0000
    dw 0x0800
    dq 0x0000000000000001 

disk_err db "Disk load error (INT 13h)", 0

times 510 - ($ - $$) db 0
dw 0xAA55
