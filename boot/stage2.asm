; stage2.asm — x86_64 Long Mode bootloader
[org 0x8000]
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

    mov si, msg_stage2
    call print_string16

    ; ================================
    ; LOAD KERNEL
    ; ================================

    mov si, kernel_dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error

    mov si, msg_kernel_loaded
    call print_string16

    ; ================================
    ; GET BIOS MEMORY MAP
    ; ================================

    call e820_collect
    jc e820_error

    mov si, msg_e820_loaded
    call print_string16

    ; ================================
    ; CHECK CPUID
    ; ================================

    pushfd
    pop eax

    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd

    pushfd
    pop eax

    xor eax, ecx
    test eax, 1 << 21

    jz cpuid_error

    ; ================================
    ; CHECK LONG MODE
    ; ================================

    mov eax, 0x80000000
    cpuid

    cmp eax, 0x80000001
    jb long_mode_error

    mov eax, 0x80000001
    cpuid

    test edx, 1 << 29
    jz long_mode_error

    mov si, msg_long_mode_supported
    call print_string16

    ; ================================
    ; ENABLE A20
    ; ================================

    in al, 0x92
    or al, 00000010b
    and al, 11111110b
    out 0x92, al

    ; ================================
    ; LOAD GDT
    ; ================================

    lgdt [gdt_descriptor]

    ; ================================
    ; ENTER PROTECTED MODE
    ; ================================

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected_mode_entry


; ====================================
; 32-BIT PROTECTED MODE
; ====================================

[bits 32]

protected_mode_entry:
    ; Set data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Initialize stack
    mov esp, 0x8F000

    cld

    ; ================================
    ; CREATE PAGE TABLES
    ; ================================

    ; PML4  = 0x90000
    ; PDPT  = 0x91000
    ; PD    = 0x92000

    ; Clear all three pages
    mov edi, 0x90000
    xor eax, eax
    mov ecx, 3072
    rep stosd

    ; PML4[0] -> PDPT
    mov dword [0x90000], 0x91003

    ; PDPT[0] -> PD
    mov dword [0x91000], 0x92003

    ; PD[0] -> First 2 MiB
    ; Present | Writable | Page Size
    mov dword [0x92000], 0x00000083

    ; ================================
    ; ENABLE PAE
    ; ================================

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; ================================
    ; LOAD PML4 INTO CR3
    ; ================================

    mov eax, 0x90000
    mov cr3, eax

    ; ================================
    ; ENABLE LONG MODE
    ; ================================

    mov ecx, 0xC0000080
    rdmsr

    or eax, 1 << 8
    wrmsr

    ; ================================
    ; ENABLE PAGING
    ; ================================

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; ================================
    ; ENTER 64-BIT MODE
    ; ================================

    jmp 0x18:long_mode_entry


; ====================================
; 64-BIT LONG MODE
; ====================================

[bits 64]

long_mode_entry:
    ; Set data segments
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov ss, ax

    xor eax, eax
    mov fs, ax
    mov gs, ax

    ; Initialize 64-bit stack
    mov rsp, 0x8F000
    and rsp, -16

    ; ================================
    ; CALL KERNEL ENTRY
    ; ================================

    ; Pass boot_info pointer through RDI
    mov edi, 0x20000

    mov rax, 0x10000
    call rax

.hang:
    cli
    hlt
    jmp .hang


; ====================================
; REAL MODE ERROR HANDLERS
; ====================================

[bits 16]

disk_error:
    mov si, msg_disk_error
    call print_string16
    jmp halt16

e820_error:
    mov si, msg_e820_error
    call print_string16
    jmp halt16

cpuid_error:
    mov si, msg_cpuid_error
    call print_string16
    jmp halt16

long_mode_error:
    mov si, msg_long_mode_error
    call print_string16
    jmp halt16

halt16:
    cli
    hlt
    jmp halt16


; ====================================
; BIOS E820 MEMORY MAP
; ====================================

e820_collect:
    xor ebx, ebx

    mov word [e820_count], 0
    mov word [e820_offset], 16

.e820_loop:
    ; ES:DI -> next memory map entry

    mov ax, 0x2000
    mov es, ax

    mov di, [e820_offset]

    ; Default extended attributes:
    ; Entry is enabled.

    mov dword [es:di + 20], 1

    ; BIOS E820 request

    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24

    int 0x15

    jc .e820_failed

    ; Validate returned signature

    cmp eax, 0x534D4150
    jne .e820_failed

    ; Minimum supported entry size is 20 bytes

    cmp ecx, 20
    jb .e820_failed

    ; Restore DS because BIOS may modify it

    xor ax, ax
    mov ds, ax

    ; Save returned entry

    inc word [e820_count]
    add word [e820_offset], 24

    ; EBX = 0 means no more entries

    test ebx, ebx
    jz .e820_done

    ; Maximum 128 entries

    cmp word [e820_count], 128
    jae .e820_failed

    jmp .e820_loop

.e820_done:
    cmp word [e820_count], 0
    je .e820_failed

    ; Initialize boot_info header

    mov ax, 0x2000
    mov es, ax

    mov dword [es:0], 0x474D5A59
    mov dword [es:4], 1

    movzx eax, word [e820_count]
    mov dword [es:8], eax

    mov dword [es:12], 24

    clc
    ret

.e820_failed:
    stc
    ret


; ====================================
; BIOS PRINT
; ====================================

print_string16:
    lodsb
    or al, al
    jz .done

    mov ah, 0x0E
    int 0x10
    jmp print_string16

.done:
    ret


; ====================================
; KERNEL DISK ADDRESS PACKET
; ====================================

align 4

kernel_dap:
    db 0x10
    db 0x00
    dw 64             ; 64 sectors = 32 KiB kernel
    dw 0x0000         ; Buffer offset
    dw 0x1000         ; Buffer segment -> physical 0x10000
    dq 33             ; Kernel starts at LBA 33


boot_drive db 0

e820_count  dw 0
e820_offset dw 0


; ====================================
; GDT
; ====================================

align 8

gdt_start:
    dq 0x0000000000000000

    ; 32-bit Code Segment
    dq 0x00CF9A000000FFFF

    ; Data Segment
    dq 0x00CF92000000FFFF

    ; 64-bit Code Segment
    dq 0x00AF9A000000FFFF

    ; 64-bit Data Segment
    dq 0x00CF92000000FFFF

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start


; ====================================
; MESSAGES
; ====================================

msg_stage2 db "Stage 2 started", 13, 10, 0
msg_kernel_loaded db "Kernel loaded", 13, 10, 0
msg_e820_loaded db "BIOS memory map loaded", 13, 10, 0
msg_long_mode_supported db "Long Mode supported", 13, 10, 0

msg_disk_error db "ERROR: Kernel disk read failed", 13, 10, 0
msg_e820_error db "ERROR: BIOS E820 failed", 13, 10, 0
msg_cpuid_error db "ERROR: CPUID unavailable", 13, 10, 0
msg_long_mode_error db "ERROR: Long Mode unavailable", 13, 10, 0