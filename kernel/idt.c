#include "idt.h"
#include "terminal.h"

#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "user_mode.h"
#include "syscall.h"

#include <stddef.h>
#include <stdint.h>

#define IDT_ENTRIES 256

#define IDT_INTERRUPT_GATE 0x8E
#define IDT_USER_INTERRUPT_GATE 0xEE

#define KERNEL_CODE_SELECTOR 0x18

#define USER_SYSCALL_VECTOR 128

typedef struct
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_register_t;

typedef struct
{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;

    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    uint64_t vector;
    uint64_t error_code;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;

    /*
     * These fields are pushed by the CPU only when
     * an interrupt changes the privilege level.
     *
     * They are valid when (cs & 3) == 3.
     */

    uint64_t rsp;
    uint64_t ss;
} interrupt_frame_t;

static idt_entry_t idt[IDT_ENTRIES];

extern void (*isr_stub_table[48])(void);
extern void isr255(void);
extern void isr128(void);

static void idt_set_gate(
    uint8_t vector,
    void (*handler)(void),
    uint8_t attributes
)
{
    const uint64_t address = (uint64_t)handler;

    idt[vector].offset_low = address & 0xFFFF;
    idt[vector].selector = KERNEL_CODE_SELECTOR;
    idt[vector].ist = 0;
    idt[vector].attributes = attributes;
    idt[vector].offset_middle = (address >> 16) & 0xFFFF;
    idt[vector].offset_high = (address >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

void idt_initialize(void)
{
    for (uint16_t i = 0; i < IDT_ENTRIES; i++)
    {
        idt_set_gate(
            (uint8_t)i,
            isr255,
            IDT_INTERRUPT_GATE
        );
    }

    for (uint8_t i = 0; i < 48; i++)
    {
        idt_set_gate(
            i,
            isr_stub_table[i],
            IDT_INTERRUPT_GATE
        );
    }

    /*
     * Configure the system call interrupt.
     *
     * DPL = 3 allows user-mode applications
     * to invoke this interrupt using INT 0x80.
     */

    idt_set_gate(
        USER_SYSCALL_VECTOR,
        isr128,
        IDT_USER_INTERRUPT_GATE
    );

    const idt_register_t idtr =
    {
        .limit = sizeof(idt) - 1,
        .base = (uint64_t)idt
    };

    __asm__ volatile (
        "lidt %0"
        :
        : "m"(idtr)
    );
}

void interrupt_handler(interrupt_frame_t *frame)
{
    const uint64_t vector = frame->vector;

    /*
     * Handle user-mode system calls.
     */

    /*
     * Handle user-mode system calls.
     */

    if (vector == USER_SYSCALL_VECTOR)
    {
        if ((frame->cs & 3) != 3)
        {
            terminal_write("Invalid system call privilege level");

            __asm__ volatile ("cli");

            for (;;)
            {
                __asm__ volatile ("hlt");
            }
        }

        frame->rax = syscall_dispatch(
            frame->rax,
            frame->rdi,
            frame->rsi,
            frame->rdx
        );

        return;
    }

    // CPU exceptions.
    if (vector < 32)
    {
        terminal_write("KERNEL PANIC");
        terminal_write("A CPU exception has occurred.");

        terminal_write_hex("Exception vector", vector);
        terminal_write_hex("Error code", frame->error_code);
        terminal_write_hex("Instruction pointer", frame->rip);

        terminal_write("System halted.");

        __asm__ volatile ("cli");

        for (;;)
        {
            __asm__ volatile ("hlt");
        }
    }

    // Hardware IRQs.
    if (vector >= 32 && vector <= 47)
    {
        const uint8_t irq = vector - 32;

        switch (irq)
        {
            case 0:
                pit_tick();
                break;

            case 1:
                keyboard_handle_interrupt();
                break;

            default:
                break;
        }

        pic_send_eoi(irq);
    }
}