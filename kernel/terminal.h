#ifndef KERNEL_TERMINAL_H
#define KERNEL_TERMINAL_H

#include <stddef.h>
#include <stdint.h>

void terminal_initialize(void);
void terminal_write(const char *string);
void terminal_write_hex(const char *label, uint64_t value);
void terminal_write_char(char character);
void terminal_clear(void);

void terminal_get_cursor(size_t *row, size_t *column);
void terminal_set_cursor(size_t row, size_t column);

#endif