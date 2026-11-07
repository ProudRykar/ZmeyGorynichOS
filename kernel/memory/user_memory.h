#ifndef KERNEL_USER_MEMORY_H
#define KERNEL_USER_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USER_MEMORY_STACK_PAGES 4

#define USER_MEMORY_STACK_TOP 0x00007FFFFFFFF000ULL

bool user_memory_allocate(
    uint64_t virtual_address,
    size_t page_count
);

bool user_memory_free(
    uint64_t virtual_address,
    size_t page_count
);

bool user_memory_create_stack(uint64_t *stack_pointer);

bool user_memory_destroy_stack(void);

#endif