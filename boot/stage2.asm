; stage2.asm — protected mode + IDT (runtime fill)
[org 0x8000]
bits 16

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov si, msg_before_pm
    call print_string16

    ; DS = CS
    push cs
    pop ax
    mov ds, ax

    cli
    lgdt [gdt_descriptor]

    ; set PE bit
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:pm_entry


; ================================
; PROTECTED MODE (32-bit)
; ================================
[bits 32]

pm_entry:
    ; set data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; stack
    mov esp, 0x009F000

    ; init keyboard cursor (вторая строка)
    mov dword [kbd_pos], 0xB8000 + 2*80*2
    
    ; remap PIC
    call pic_remap

    ; fill IDT from handlers table
    call fill_idt_from_handlers

    ; load IDT
    lidt [idtr]

    ; initialize keyboard cursor (start of 3rd text row) and clear shift flags
    mov dword [kbd_pos], 0x000B8140    ; 0xB8000 + 2*80*2 = 0xB8140
    mov dword [shift_flags], 0

    sti

    ; --- print PM message to VGA (first row) ---
    mov esi, msg_pm
    mov edi, 0xB8000
.print:
    lodsb
    test al, al
    jz .after_print
    mov [edi], al
    mov byte [edi+1], 0x07
    add edi, 2
    jmp .print
.after_print:

    ; enter low-power loop — IRQs will wake CPU
.hlt_loop:
    hlt
    jmp .hlt_loop


; ================================
; PIC REMAP
; ================================
pic_remap:
    mov al, 0x11
    out 0x20, al
    out 0xA0, al

    mov al, 0x20
    out 0x21, al
    mov al, 0x28
    out 0xA1, al

    mov al, 0x04
    out 0x21, al
    mov al, 0x02
    out 0xA1, al

    mov al, 0x01
    out 0x21, al
    out 0xA1, al

    mov al, 0x00
    out 0x21, al
    out 0xA1, al
    ret


; ================================
; IDT memory & handlers
; ================================
[bits 32]
align 8
idt_space:
    times 48*8 db 0
idt_end:

align 4
idt_handlers:
    dd isr0
    dd isr1
    dd isr2
    dd isr3
    dd isr4
    dd isr5
    dd isr6
    dd isr7
    dd isr8
    dd isr9
    dd isr10
    dd isr11
    dd isr12
    dd isr13
    dd isr14
    dd isr15
    dd isr16
    dd isr17
    dd isr18
    dd isr19
    dd isr20
    dd isr21
    dd isr22
    dd isr23
    dd isr24
    dd isr25
    dd isr26
    dd isr27
    dd isr28
    dd isr29
    dd isr30
    dd isr31
    dd isr32
    dd isr33
    dd isr34
    dd isr35
    dd isr36
    dd isr37
    dd isr38
    dd isr39
    dd isr40
    dd isr41
    dd isr42
    dd isr43
    dd isr44
    dd isr45
    dd isr46
    dd isr47

align 4
idtr:
    dw idt_end - idt_space - 1
    dd idt_space


; ================================
; Fill IDT routine
; ================================
fill_idt_from_handlers:
    pushad
    mov esi, idt_space
    mov ebx, idt_handlers
    mov ecx, 48
.fill_loop:
    mov eax, [ebx]
    mov word [esi], ax
    mov word [esi+2], 0x08
    mov byte [esi+4], 0
    mov byte [esi+5], 0x8E
    mov edx, eax
    shr edx, 16
    mov word [esi+6], dx
    add esi, 8
    add ebx, 4
    dec ecx
    jnz .fill_loop
    popad
    ret


; ================================
; ISR STUBS
; ================================
%macro ISR_NOERR 1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr%1:
    push dword %1
    jmp isr_common
%endmacro

; exceptions 0..31
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; IRQs 32..47
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47


; ================================
; ISR common handler
; ================================
align 4
tick_count dd 0
kbd_pos   dd 0

align 4
shift_flags dd 0   ; бит 0 = левый Shift, бит 1 = правый Shift

; full scancode → ASCII (set1, US layout)
align 4
kbd_map:
    db 0    ; 0x00 — нет клавиши
    db 27   ; 0x01 — ESC
    db '1'  ; 0x02
    db '2'  ; 0x03
    db '3'  ; 0x04
    db '4'  ; 0x05
    db '5'  ; 0x06
    db '6'  ; 0x07
    db '7'  ; 0x08
    db '8'  ; 0x09
    db '9'  ; 0x0A
    db '0'  ; 0x0B
    db '-'  ; 0x0C
    db '='  ; 0x0D
    db 8    ; 0x0E — Backspace
    db 9    ; 0x0F — Tab
    db 'q'  ; 0x10
    db 'w'  ; 0x11
    db 'e'  ; 0x12
    db 'r'  ; 0x13
    db 't'  ; 0x14
    db 'y'  ; 0x15
    db 'u'  ; 0x16
    db 'i'  ; 0x17
    db 'o'  ; 0x18
    db 'p'  ; 0x19
    db '['  ; 0x1A
    db ']'  ; 0x1B
    db 10   ; 0x1C — Enter
    db 0    ; 0x1D — Ctrl
    db 'a'  ; 0x1E
    db 's'  ; 0x1F
    db 'd'  ; 0x20
    db 'f'  ; 0x21
    db 'g'  ; 0x22
    db 'h'  ; 0x23
    db 'j'  ; 0x24
    db 'k'  ; 0x25
    db 'l'  ; 0x26
    db ';'  ; 0x27
    db 39  ; 0x28
    db '`'  ; 0x29
    db 0    ; 0x2A — Left Shift
    db '\'  ; 0x2B
    db 'z'  ; 0x2C
    db 'x'  ; 0x2D
    db 'c'  ; 0x2E
    db 'v'  ; 0x2F
    db 'b'  ; 0x30
    db 'n'  ; 0x31
    db 'm'  ; 0x32
    db ','  ; 0x33
    db '.'  ; 0x34
    db '/'  ; 0x35
    db 0    ; 0x36 — Right Shift
    db '*'  ; 0x37 — Keypad *
    db 0    ; 0x38 — Alt
    db ' '  ; 0x39 — Space
    ; остальные коды можно заполнить нулями или соответствующими символами
align 4
kbd_map_shift:
    db 0    ; 0x00 — нет клавиши
    db 27   ; 0x01 — ESC
    db '!'  ; 0x02
    db '@'  ; 0x03
    db '#'  ; 0x04
    db '$'  ; 0x05
    db '%'  ; 0x06
    db '^'  ; 0x07
    db '&'  ; 0x08
    db '*'  ; 0x09
    db '('  ; 0x0A
    db ')'  ; 0x0B
    db '_'  ; 0x0C
    db '+'  ; 0x0D
    db 8    ; 0x0E — Backspace
    db 9    ; 0x0F — Tab
    db 'Q'  ; 0x10
    db 'W'  ; 0x11
    db 'E'  ; 0x12
    db 'R'  ; 0x13
    db 'T'  ; 0x14
    db 'Y'  ; 0x15
    db 'U'  ; 0x16
    db 'I'  ; 0x17
    db 'O'  ; 0x18
    db 'P'  ; 0x19
    db '{'  ; 0x1A
    db '}'  ; 0x1B
    db 10   ; 0x1C — Enter
    db 0    ; 0x1D — Ctrl
    db 'A'  ; 0x1E
    db 'S'  ; 0x1F
    db 'D'  ; 0x20
    db 'F'  ; 0x21
    db 'G'  ; 0x22
    db 'H'  ; 0x23
    db 'J'  ; 0x24
    db 'K'  ; 0x25
    db 'L'  ; 0x26
    db ':'  ; 0x27
    db '"'  ; 0x28
    db '~'  ; 0x29
    db 0    ; 0x2A — Left Shift
    db '|'  ; 0x2B
    db 'Z'  ; 0x2C
    db 'X'  ; 0x2D
    db 'C'  ; 0x2E
    db 'V'  ; 0x2F
    db 'B'  ; 0x30
    db 'N'  ; 0x31
    db 'M'  ; 0x32
    db '<'  ; 0x33
    db '>'  ; 0x34
    db '?'  ; 0x35
    db 0    ; 0x36 — Right Shift
    db '*'  ; 0x37
    db 0    ; 0x38 — Alt
    db ' '  ; 0x39


isr_common:
    pusha
    mov eax, [esp + 32]  ; vector

    ; -------- IRQ0: timer --------
    cmp eax, 32
    je timer_irq
    ; -------- IRQ1: keyboard --------
    cmp eax, 33
    je keyboard_irq
    jmp skip_irq

timer_irq:
    inc dword [tick_count]
    mov edi, 0xB8000 + 2*80*1
    mov eax, [tick_count]
    xor edx, edx
    mov ecx, 10
    div ecx
    add dl, '0'
    mov [edi], dl
    mov byte [edi+1], 0x0A
    jmp skip_irq

keyboard_irq:
    in al, 0x60
    mov ah, al
    test al, 0x80
    jnz key_release
    ; --- key press ---
    movzx eax, al
    cmp eax, 0x3F
    ja kbd_done
    ; special: Left Shift = 0x2A, Right Shift = 0x36
    cmp eax, 0x2A
    je shift_left_down
    cmp eax, 0x36
    je shift_right_down
    ; обычная клавиша
    mov bl, [shift_flags]
    test bl, 3        ; проверка битов Shift
    jz use_normal
    mov al, [kbd_map_shift + eax]
    jmp write_char

use_normal:
    mov al, [kbd_map + eax]

write_char:
    cmp al, 0
    je kbd_done

    mov edi, [kbd_pos]    ; загружаем текущую позицию
    test edi, edi
    jnz have_pos          ; если != 0 — используем её
    mov edi, 0xB8000 + 2*80*2   ; иначе — стартовая позиция (вторая строка)
have_pos:
    mov [edi], al
    mov byte [edi+1], 0x0F
    add edi, 2
    cmp edi, 0xB8000 + 2*80*3
    jb no_wrap
    mov edi, 0xB8000 + 2*80*2
no_wrap:
    mov [kbd_pos], edi

kbd_done:
    nop
    jmp skip_irq

key_release:
    movzx eax, ah
    ; release Shift
    cmp eax, 0xAA
    je shift_left_up
    cmp eax, 0xB6
    je shift_right_up
    jmp kbd_done

shift_left_down:
    or dword [shift_flags], 1
    jmp kbd_done
shift_left_up:
    and dword [shift_flags], 0xFFFFFFFE
    jmp kbd_done
shift_right_down:
    or dword [shift_flags], 2
    jmp kbd_done
shift_right_up:
    and dword [shift_flags], 0xFFFFFFFD
    jmp kbd_done

skip_irq:
    popa
    add esp, 8
    mov al, 0x20
    out 0x20, al
    out 0xA0, al
    iret


; ================================
; GDT
; ================================
[bits 16]
align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ================================
; MESSAGES & BIOS PRINT
; ================================
[bits 16]
msg_before_pm db "Entering protected mode",0
[bits 32]
msg_pm db "PM + IDT OK",0

[bits 16]
print_string16:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print_string16
.done:
    ret
