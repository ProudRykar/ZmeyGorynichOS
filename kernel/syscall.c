#include "syscall.h"

#include "process/process_manager.h"
#include "terminal.h"
#include "memory/vmm.h"

#define SYSCALL_WRITE_MAX_LENGTH 4096ULL
#define SYSCALL_USER_ADDRESS_LIMIT 0x0000800000000000ULL

static int64_t syscall_exit(uint64_t exit_code)
{
    process_exit((int64_t)exit_code);
}

static int64_t syscall_write(
    uint64_t user_buffer,
    uint64_t length
)
{
    if (length == 0)
    {
        return 0;
    }

    if (length > SYSCALL_WRITE_MAX_LENGTH)
    {
        return SYSCALL_ERROR_TOO_LARGE;
    }

    if (user_buffer >= SYSCALL_USER_ADDRESS_LIMIT)
    {
        return SYSCALL_ERROR_INVALID_ARGUMENT;
    }

    if (length > SYSCALL_USER_ADDRESS_LIMIT - user_buffer)
    {
        return SYSCALL_ERROR_INVALID_ARGUMENT;
    }

    const uint64_t last_address =
        user_buffer + length - 1;

    const uint64_t first_page =
        user_buffer & ~(VMM_PAGE_SIZE - 1);

    const uint64_t last_page =
        last_address & ~(VMM_PAGE_SIZE - 1);

    /*
     * Verify that every page containing the user buffer
     * is present and accessible from Ring 3.
     */

    for (
        uint64_t page = first_page;
        page <= last_page;
        page += VMM_PAGE_SIZE
    )
    {
        if (!vmm_is_user_accessible(page))
        {
            return SYSCALL_ERROR_INVALID_ARGUMENT;
        }
    }

    const volatile char *buffer =
        (const volatile char *)user_buffer;

    for (uint64_t i = 0; i < length; i++)
    {
        terminal_write_char(buffer[i]);
    }

    return (int64_t)length;
}

static int64_t syscall_getpid(void)
{
    process_t *process = process_current();

    if (process == NULL)
    {
        return SYSCALL_ERROR_INVALID_ARGUMENT;
    }

    return (int64_t)process->pid;
}

int64_t syscall_dispatch(
    uint64_t number,
    uint64_t argument1,
    uint64_t argument2,
    uint64_t argument3
)
{
    (void)argument3;

    switch (number)
    {
        case SYS_EXIT:
            return syscall_exit(argument1);

        case SYS_WRITE:
            return syscall_write(argument1, argument2);

        case SYS_GETPID:
            return syscall_getpid();

        default:
            return SYSCALL_ERROR_NOT_IMPLEMENTED;
    }
}