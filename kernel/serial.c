#include "serial.h"
#include "io.h"

#include <stdint.h>

#define SERIAL_COM1 0x3F8

static void serial_write_byte(uint8_t byte)
{
    while ((inb(SERIAL_COM1 + 5) & 0x20) == 0)
    {
        __asm__ volatile ("pause");
    }

    outb(SERIAL_COM1, byte);
}

static void serial_write_codepoint(uint32_t codepoint)
{
    if (codepoint <= 0x7F)
    {
        serial_write_byte((uint8_t)codepoint);
    }
    else if (codepoint <= 0x7FF)
    {
        serial_write_byte((uint8_t)(0xC0 | (codepoint >> 6)));
        serial_write_byte((uint8_t)(0x80 | (codepoint & 0x3F)));
    }
    else
    {
        serial_write_byte((uint8_t)(0xE0 | (codepoint >> 12)));
        serial_write_byte((uint8_t)(0x80 | ((codepoint >> 6) & 0x3F)));
        serial_write_byte((uint8_t)(0x80 | (codepoint & 0x3F)));
    }
}

void serial_initialize(void)
{
    outb(SERIAL_COM1 + 1, 0x00);
    outb(SERIAL_COM1 + 3, 0x80);
    outb(SERIAL_COM1 + 0, 0x03);
    outb(SERIAL_COM1 + 1, 0x00);
    outb(SERIAL_COM1 + 3, 0x03);
    outb(SERIAL_COM1 + 2, 0xC7);
    outb(SERIAL_COM1 + 4, 0x0B);
}

void serial_write_char(char character)
{
    uint8_t byte = (uint8_t)character;

    if (byte == '\n')
    {
        serial_write_byte('\r');
        serial_write_byte('\n');
        return;
    }

    if (byte == '\b')
    {
        serial_write_byte('\b');
        serial_write_byte(' ');
        serial_write_byte('\b');
        return;
    }

    if (byte >= 0x80 && byte <= 0x9F)
    {
        serial_write_codepoint(0x0410 + byte - 0x80);
    }
    else if (byte >= 0xA0 && byte <= 0xAF)
    {
        serial_write_codepoint(0x0430 + byte - 0xA0);
    }
    else if (byte >= 0xE0 && byte <= 0xEF)
    {
        serial_write_codepoint(0x0440 + byte - 0xE0);
    }
    else if (byte == 0xF0)
    {
        serial_write_codepoint(0x0401);
    }
    else if (byte == 0xF1)
    {
        serial_write_codepoint(0x0451);
    }
    else
    {
        serial_write_byte(byte);
    }
}

void serial_write(const char *string)
{
    while (*string != '\0')
    {
        serial_write_char(*string++);
    }
}