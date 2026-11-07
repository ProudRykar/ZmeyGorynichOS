; mbr.asm
[org 0x7C00]
bits 16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld

    ; Сохраняем номер загрузочного диска BIOS
    mov [boot_drive], dl

    ; --- Debug перед int13h ---
    mov si, msg_before_int13
    call print_string

    ; === Загрузка Stage 2 через BIOS INT 13h Extensions ===
    lea si, [dap]
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; --- Debug после int13h ---
    mov si, msg_after_int13
    call print_string

    ; Восстанавливаем номер диска
    mov dl, [boot_drive]

    ; Перепрыгиваем на Stage 2 (phys 0x8000)
    jmp 0x0000:0x8000

disk_error:
    mov si, disk_err
    call print_string

.hang:
    cli
    hlt
    jmp .hang

; ================= DAP =================
dap:
    db 0x10
    db 0x00
    dw 32             ; 32 сектора = 16 KiB Stage 2
    dw 0x8000         ; Buffer offset
    dw 0x0000         ; Buffer segment
    dq 1              ; Starting LBA

boot_drive db 0

disk_err db "Disk load error (INT 13h)", 13, 10, 0
msg_before_int13 db "DEBUG: before int13h", 13, 10, 0
msg_after_int13 db "DEBUG: after int13h", 13, 10, 0

; ================= print_string =================
print_string:
    lodsb
    or al, al
    jz .done

    mov ah, 0x0E
    int 0x10
    jmp print_string

.done:
    ret

times 510-($-$$) db 0
dw 0xAA55