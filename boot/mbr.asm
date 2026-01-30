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

    ; --- Debug перед int13h ---
    mov si, msg_before_int13
    call print_string

    ; === ИЗМЕНЕНИЕ НАЧАЛО: правильная подготовка DAP и вызов INT13h ===
    lea si, [dap]
    mov ah, 0x42         ; EXT_READ
    ; DL содержит номер диска от BIOS — не перезаписываем
    int 0x13
    jc disk_error
    ; === ИЗМЕНЕНИЕ КОНЕЦ ===

    ; --- Debug после int13h ---
    mov si, msg_after_int13
    call print_string

    ; Перепрыгиваем на stage2 (должен быть загружен в phys 0x8000)
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

; ================= DAP =================
dap:
    db 0x10           ; size of DAP structure (16)
    db 0x00           ; reserved
    dw 0x0002         ; <-- исправлено: читаем 2 сектора (stage2)
    dw 0x8000         ; buffer offset (0x8000)
    dw 0x0000         ; buffer segment (0x0000) -> phys 0x0000:0x8000
    dq 0x0000000000000001  ; starting LBA = 1

disk_err db "Disk load error (INT 13h)",0
msg_before_int13 db "DEBUG: before int13h",0
msg_after_int13 db "DEBUG: after int13h",0

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
