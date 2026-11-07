#ifndef KERNEL_KEYBOARD_H
#define KERNEL_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

#define KEYBOARD_KEY_UP    0x100
#define KEYBOARD_KEY_DOWN  0x101
#define KEYBOARD_KEY_LEFT  0x102
#define KEYBOARD_KEY_RIGHT 0x103
#define KEYBOARD_KEY_HOME  0x104
#define KEYBOARD_KEY_END   0x105
#define KEYBOARD_KEY_DELETE 0x106

void keyboard_initialize(void);
void keyboard_handle_interrupt(void);
bool keyboard_read_key(uint16_t *key);

#endif