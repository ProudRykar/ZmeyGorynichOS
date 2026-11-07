#ifndef KERNEL_GDT_H
#define KERNEL_GDT_H

#include <stdint.h>

#define GDT_KERNEL_CODE_SELECTOR 0x18
#define GDT_KERNEL_DATA_SELECTOR 0x20

#define GDT_USER_DATA_SELECTOR 0x2B
#define GDT_USER_CODE_SELECTOR 0x33

#define GDT_TSS_SELECTOR 0x38

void gdt_initialize(void);

uint64_t gdt_get_kernel_stack(void);

#endif