#include "pic.h"
#include "io.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21

#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC1_OFFSET 0x20
#define PIC2_OFFSET 0x28

#define PIC_EOI 0x20

void pic_initialize(void)
{
    // Disable all IRQs before reconfiguration.
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    // Begin initialization sequence.
    outb(PIC1_COMMAND, 0x11);
    io_wait();

    outb(PIC2_COMMAND, 0x11);
    io_wait();

    // Set interrupt vector offsets.
    outb(PIC1_DATA, PIC1_OFFSET);
    io_wait();

    outb(PIC2_DATA, PIC2_OFFSET);
    io_wait();

    // Configure master-slave relationship.
    outb(PIC1_DATA, 0x04);
    io_wait();

    outb(PIC2_DATA, 0x02);
    io_wait();

    // Set 8086/88 mode.
    outb(PIC1_DATA, 0x01);
    io_wait();

    outb(PIC2_DATA, 0x01);
    io_wait();

    // Enable IRQ0 and IRQ1 on the master PIC.
    outb(PIC1_DATA, 0xFC);

    // Disable all IRQs on the slave PIC.
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
    {
        outb(PIC2_COMMAND, PIC_EOI);
    }

    outb(PIC1_COMMAND, PIC_EOI);
}