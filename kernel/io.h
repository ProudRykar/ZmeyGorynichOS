#ifndef KERNEL_IO_H
#define KERNEL_IO_H

#include <stdint.h>

/**
 * @brief Writes an 8-bit value to an I/O port.
 *
 * @param port The I/O port address.
 * @param value The value to write.
 */
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

/**
 * @brief Reads an 8-bit value from an I/O port.
 *
 * @param port The I/O port address.
 * @return The value read from the port.
 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

/**
 * @brief Introduces a small I/O delay.
 */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

#endif