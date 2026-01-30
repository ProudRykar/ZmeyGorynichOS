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

    ; remap PIC
    call pic_remap

    ; fill IDT from handlers table
    call fill_idt_from_handlers

    ; load IDT
    lidt [idtr]

    sti

    ; test output in VGA
    mov esi, msg_pm
    mov edi, 0xB8000
    
.print:
    lodsb
    test al, al
    jz .halt
    mov [edi], al
    mov byte [edi+1], 0x07
    add edi, 2
    jmp .print

.halt:
    cli
    hlt
    jmp .halt


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
; IDT memory & handlers (data)
; ================================
[bits 32]
align 8
idt_space:               ; space for 48 entries (0..47) * 8 bytes
    times 48*8 db 0
idt_end:

; table of handler addresses (dd isrX)
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

; IDTR (limit + base)
align 4
idtr:
    dw idt_end - idt_space - 1
    dd idt_space


; ================================
; Fill IDT routine (32-bit)
; Copies handlers from idt_handlers (dd) into idt_space entries:
; layout per entry: dw offset_low, dw selector, db 0, db flags, dw offset_high
; ================================
fill_idt_from_handlers:
    pushad

    mov esi, idt_space       ; destination pointer (byte)
    mov ebx, idt_handlers    ; source pointer (dd list)
    mov ecx, 48              ; number of entries

.fill_loop:
    mov eax, [ebx]           ; handler address
    mov ax, ax               ; ensure ax is low word (no-op to satisfy assembler)
    mov word [esi], ax       ; offset low (word)
    mov word [esi+2], 0x08   ; selector (code)
    mov byte [esi+4], 0      ; zero
    mov byte [esi+5], 0x8E   ; flags: present, DPL=0, 32-bit interrupt gate
    mov edx, eax
    shr edx, 16
    mov dx, dx
    mov word [esi+6], dx     ; offset high
    add esi, 8
    add ebx, 4
    dec ecx
    jnz .fill_loop

    popad
    ret


; ================================
; ISR STUBS (32-bit)
; For exceptions with error code: 8,10,11,12,13,14,17 -> we use ISR_ERR
; Others use ISR_NOERR (push fake 0 error)
; Each stub pushes error (or fake) and vector number, then jumps to common handler.
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
; ISR common handler (32-bit)
; Stack at entry: [vector][error_code][EIP][CS][EFLAGS]...
; We will read vector & error_code, show simple marker in VGA, then remove pushed dwords and iret.
; ================================
isr_common:
    ; read pushed values (vector and error)
    mov eax, [esp]       ; vector
    mov ebx, [esp + 4]   ; error code

    pusha

    ; simple VGA marker: write '!' at row 0 col 40 + vector (clamped)
    mov edi, 0xB8000
    ; write at fixed offset (for debug)
    add edi, 160
    mov byte [edi], '!'
    mov byte [edi+1], 0x4F

    popa

    ; remove the two dwords we pushed in stub (vector and error_code)
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
    dq 0x0000000000000000        ; NULL
    dq 0x00CF9A000000FFFF        ; code
    dq 0x00CF92000000FFFF        ; data
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start


; ================================
; MESSAGES & BIOS PRINT (16-bit)
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
