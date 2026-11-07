#ifndef KERNEL_PIT_H
#define KERNEL_PIT_H

#include <stdint.h>

void pit_initialize(void);
void pit_tick(void);
uint64_t pit_get_ticks(void);

#endif