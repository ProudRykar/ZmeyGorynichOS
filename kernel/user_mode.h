#ifndef KERNEL_USER_MODE_H
#define KERNEL_USER_MODE_H

#include <stdint.h>

typedef struct
{
    uint64_t kernel_rsp;
    uint64_t kernel_cr3;
    uint64_t kernel_rflags;

} user_mode_context_t;

void user_mode_enter(
    uint64_t instruction_pointer,
    uint64_t stack_pointer,
    uint64_t kernel_pml4,
    user_mode_context_t *context
);

__attribute__((noreturn))
void user_mode_exit(void);

#endif