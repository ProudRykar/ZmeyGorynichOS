; stage2.asm
; Загружается MBR в phys 0x8000 -> org 0x8000
[org 0x8000]
bits 16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Печать в реальном режиме (BIOS teletype)
    mov si, msg_before_pm
    call print_string16

    ; === ИЗМЕНЕНИЕ НАЧАЛО: установить DS = CS, затем безопасно выполнить LGDT и включить PE ===
    ; Установим DS равным CS (чтобы обращения вида [gdt_descriptor] шли в тот же сегмент, где лежит GDT)
    push cs
    pop ax
    mov ds, ax

    ; Перед изменением CR0 выключаем прерывания
    cli

    lgdt [gdt_descriptor]      ; загрузить GDTR из памяти (DS:offset)

    ; Включаем Protected Mode (PE bit в CR0)
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump в кодовый сегмент GDT (0x08) для переключения CS
    jmp 0x08:protected_entry
    ; === ИЗМЕНЕНИЕ КОНЕЦ ===

; ---------------- Protected mode (32-bit) ----------------
[bits 32]
protected_entry:
    ; Установим сегменты данных на селектор 0x10 (data)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Настроим стек (линейный адрес)
    mov esp, 0x0009F000    ; безопасный стек (подстрой при необходимости)

    ; Печать в protected mode: пишем в VGA text buffer (0xB8000)
    mov esi, msg_prot
    mov edi, 0xB8000
.print_prot:
    mov al, [esi]
    test al, al
    jz .done_prot
    mov [edi], al
    mov byte [edi+1], 0x07
    add edi, 2
    inc esi
    jmp .print_prot
.done_prot:
    jmp $


; =========================
; GDT (выравнено)
; =========================
align 8
gdt_start:
    dq 0x0000000000000000        ; NULL

    ; Кодовый дескриптор: base=0, limit=0xFFFFF, access=0x9A, flags=0xCF
    dq 0x00CF9A000000FFFF

    ; Дескриптор данных: base=0, limit=0xFFFFF, access=0x92, flags=0xCF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; =========================
; Сообщения
; =========================
[bits 16]
msg_before_pm db "DEBUG: before protected mode (real-mode print)",0

[bits 32]
msg_prot db "Protected mode OK - text at 0xB8000",0

; =========================
; Print for 16-bit (BIOS teletype)
; =========================
[bits 16]
print_string16:
    lodsb
    or al, al
    jz .done16
    mov ah, 0x0E
    int 0x10
    jmp print_string16
.done16:
    ret

