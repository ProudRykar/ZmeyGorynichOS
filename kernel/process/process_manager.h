#ifndef KERNEL_PROCESS_MANAGER_H
#define KERNEL_PROCESS_MANAGER_H

#include "process.h"

#define PROCESS_MAX_COUNT 16

bool process_manager_initialize(uint64_t kernel_pml4_address);

bool process_create(
    const void *code,
    size_t code_size,
    process_t **process
);

process_t *process_find(uint64_t pid);

process_t *process_current(void);

bool process_run(process_t *process);

__attribute__((noreturn))
void process_exit(int64_t exit_code);

bool process_destroy(uint64_t pid);

#endif