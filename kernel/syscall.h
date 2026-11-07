#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <stdint.h>

#define SYS_EXIT  0
#define SYS_WRITE 1
#define SYS_GETPID 2

#define SYSCALL_SUCCESS 0
#define SYSCALL_ERROR_INVALID_ARGUMENT -1
#define SYSCALL_ERROR_TOO_LARGE -2
#define SYSCALL_ERROR_NOT_IMPLEMENTED -38

int64_t syscall_dispatch(
    uint64_t number,
    uint64_t argument1,
    uint64_t argument2,
    uint64_t argument3
);

#endif