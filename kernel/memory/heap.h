#ifndef KERNEL_HEAP_H
#define KERNEL_HEAP_H

#include <stdbool.h>
#include <stddef.h>

bool heap_initialize(void);

void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void *krealloc(void *pointer, size_t new_size);
void kfree(void *pointer);

#endif