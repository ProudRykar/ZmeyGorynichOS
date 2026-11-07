#include "terminal.h"
#include "serial.h"

#include <stddef.h>
#include <stdint.h>

#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define VGA_COLOR 0x07

#include "vga_font.h"

static size_t terminal_row = 0;
static size_t terminal_column = 0;

static void terminal_update_cursor(void)
{
    uint16_t position =
        (uint16_t)(terminal_row * VGA_WIDTH + terminal_column);

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((uint8_t)0x0F), "Nd"((uint16_t)0x3D4)
    );

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((uint8_t)(position & 0xFF)), "Nd"((uint16_t)0x3D5)
    );

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((uint8_t)0x0E), "Nd"((uint16_t)0x3D4)
    );

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((uint8_t)(position >> 8)), "Nd"((uint16_t)0x3D5)
    );
}

static void terminal_scroll(void)
{
    for (size_t row = 1; row < VGA_HEIGHT; row++)
    {
        for (size_t column = 0; column < VGA_WIDTH; column++)
        {
            const size_t source = row * VGA_WIDTH + column;
            const size_t destination = (row - 1) * VGA_WIDTH + column;

            VGA_MEMORY[destination] = VGA_MEMORY[source];
        }
    }

    for (size_t column = 0; column < VGA_WIDTH; column++)
    {
        const size_t index =
            (VGA_HEIGHT - 1) * VGA_WIDTH + column;

        VGA_MEMORY[index] =
            ((uint16_t)VGA_COLOR << 8) | ' ';
    }

    terminal_row = VGA_HEIGHT - 1;
}

static void terminal_putchar(char character)
{
    if (character == '\n')
    {
        terminal_column = 0;
        terminal_row++;

        if (terminal_row >= VGA_HEIGHT)
        {
            terminal_scroll();
        }

        terminal_update_cursor();
        return;
    }

    if (character == '\b')
    {
        if (terminal_column > 0)
        {
            terminal_column--;
        }
        else if (terminal_row > 0)
        {
            terminal_row--;
            terminal_column = VGA_WIDTH - 1;
        }
        else
        {
            terminal_update_cursor();
            return;
        }

        const size_t index =
            terminal_row * VGA_WIDTH + terminal_column;

        VGA_MEMORY[index] =
            ((uint16_t)VGA_COLOR << 8) | ' ';

        terminal_update_cursor();
        return;
    }

    if (terminal_row >= VGA_HEIGHT)
    {
        terminal_scroll();
    }

    const size_t index =
        terminal_row * VGA_WIDTH + terminal_column;

    VGA_MEMORY[index] =
        ((uint16_t)VGA_COLOR << 8) | (uint8_t)character;

    terminal_column++;

    if (terminal_column >= VGA_WIDTH)
    {
        terminal_column = 0;
        terminal_row++;

        if (terminal_row >= VGA_HEIGHT)
        {
            terminal_scroll();
        }
    }

    terminal_update_cursor();
}

void terminal_write_char(char character)
{
    serial_write_char(character);
    terminal_putchar(character);
}

void terminal_initialize(void)
{
    vga_font_initialize();
    terminal_clear();
}

void terminal_write(const char *string)
{
    if (string == NULL)
    {
        return;
    }

    while (*string != '\0')
    {
        terminal_write_char(*string);
        string++;
    }

    terminal_write_char('\n');
}

void terminal_write_hex(const char *label, uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    if (label != NULL)
    {
        while (*label != '\0')
        {
            terminal_write_char(*label);
            label++;
        }

        terminal_write_char(':');
        terminal_write_char(' ');
    }

    for (int i = 60; i >= 0; i -= 4)
    {
        terminal_write_char(digits[(value >> i) & 0xF]);
    }

    terminal_write_char('\n');
}

void terminal_clear(void)
{
    terminal_row = 0;
    terminal_column = 0;

    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
    {
        VGA_MEMORY[i] =
            ((uint16_t)VGA_COLOR << 8) | ' ';
    }

    terminal_update_cursor();
}

void terminal_get_cursor(size_t *row, size_t *column)
{
    if (row != NULL)
    {
        *row = terminal_row;
    }

    if (column != NULL)
    {
        *column = terminal_column;
    }
}

void terminal_set_cursor(size_t row, size_t column)
{
    if (row >= VGA_HEIGHT)
    {
        row = VGA_HEIGHT - 1;
    }

    if (column >= VGA_WIDTH)
    {
        column = VGA_WIDTH - 1;
    }

    terminal_row = row;
    terminal_column = column;

    terminal_update_cursor();
}