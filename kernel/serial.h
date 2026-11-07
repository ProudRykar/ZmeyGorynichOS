#ifndef KERNEL_SERIAL_H
#define KERNEL_SERIAL_H

void serial_initialize(void);
void serial_write_char(char character);
void serial_write(const char *string);

#endif