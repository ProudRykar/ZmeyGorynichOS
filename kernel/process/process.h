#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memory/address_space.h"
#include "user_mode.h"

#define PROCESS_CODE_BASE 0x400000ULL

typedef enum
{
    PROCESS_UNUSED,
    PROCESS_CREATING,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_TERMINATED
} process_state_t;

typedef struct
{
    uint64_t pid;

    process_state_t state;

    address_space_t address_space;

    uint64_t entry_point;
    uint64_t stack_pointer;

    uint64_t code_address;
    size_t code_size;
    size_t code_pages;

    user_mode_context_t context;

    int64_t exit_code;

} process_t;

#endif