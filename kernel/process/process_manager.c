#include "process_manager.h"

#include "memory/user_memory.h"
#include "memory/vmm.h"
#include "memory/pmm.h"

#include "user_mode.h"

static process_t processes[PROCESS_MAX_COUNT];

static uint64_t next_pid = 1;
static uint64_t kernel_pml4 = 0;

static process_t *current_process = NULL;

bool process_manager_initialize(uint64_t kernel_pml4_address)
{
    if (kernel_pml4_address == 0)
    {
        return false;
    }

    kernel_pml4 = kernel_pml4_address;
    next_pid = 1;
    current_process = NULL;

    for (size_t i = 0; i < PROCESS_MAX_COUNT; i++)
    {
        processes[i].pid = 0;
        processes[i].state = PROCESS_UNUSED;
        processes[i].address_space.initialized = false;
        processes[i].address_space.pml4_physical = 0;
        processes[i].entry_point = 0;
        processes[i].stack_pointer = 0;
        processes[i].code_address = 0;
        processes[i].code_size = 0;
        processes[i].code_pages = 0;
        processes[i].context.kernel_rsp = 0;
        processes[i].context.kernel_cr3 = 0;
        processes[i].context.kernel_rflags = 0;
        processes[i].exit_code = 0;
    }

    return true;
}

process_t *process_find(uint64_t pid)
{
    if (pid == 0)
    {
        return NULL;
    }

    for (size_t i = 0; i < PROCESS_MAX_COUNT; i++)
    {
        if (processes[i].state != PROCESS_UNUSED &&
            processes[i].pid == pid)
        {
            return &processes[i];
        }
    }

    return NULL;
}

process_t *process_current(void)
{
    return current_process;
}

bool process_create(
    const void *code,
    size_t code_size,
    process_t **result
)
{
    if (kernel_pml4 == 0 ||
        code == NULL ||
        code_size == 0 ||
        result == NULL)
    {
        return false;
    }

    *result = NULL;

    if (code_size > SIZE_MAX - (PMM_PAGE_SIZE - 1))
    {
        return false;
    }

    size_t code_pages =
        (code_size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;

    if (code_pages == 0 ||
        code_pages > (0x0000800000000000ULL - PROCESS_CODE_BASE) /
                     PMM_PAGE_SIZE)
    {
        return false;
    }

    process_t *process = NULL;

    for (size_t i = 0; i < PROCESS_MAX_COUNT; i++)
    {
        if (processes[i].state == PROCESS_UNUSED)
        {
            process = &processes[i];
            break;
        }
    }

    if (process == NULL)
    {
        return false;
    }

    process->state = PROCESS_CREATING;

    if (!address_space_create(&process->address_space))
    {
        process->state = PROCESS_UNUSED;
        return false;
    }

    if (!address_space_switch(&process->address_space))
    {
        address_space_destroy(&process->address_space);
        process->state = PROCESS_UNUSED;
        return false;
    }

    bool code_allocated = false;
    bool stack_allocated = false;

    if (user_memory_allocate(PROCESS_CODE_BASE, code_pages))
    {
        code_allocated = true;

        uint8_t *destination =
            (uint8_t *)(uintptr_t)PROCESS_CODE_BASE;

        const uint8_t *source = (const uint8_t *)code;

        for (size_t i = 0; i < code_size; i++)
        {
            destination[i] = source[i];
        }
    }

    if (code_allocated &&
        user_memory_create_stack(&process->stack_pointer))
    {
        stack_allocated = true;
    }

    if (!code_allocated || !stack_allocated)
    {
        if (stack_allocated)
        {
            user_memory_destroy_stack();
        }

        if (code_allocated)
        {
            user_memory_free(PROCESS_CODE_BASE, code_pages);
        }

        vmm_load_pml4(kernel_pml4);
        address_space_destroy(&process->address_space);

        process->state = PROCESS_UNUSED;

        return false;
    }

    if (!vmm_load_pml4(kernel_pml4))
    {
        return false;
    }

    process->pid = next_pid++;
    process->entry_point = PROCESS_CODE_BASE;
    process->code_address = PROCESS_CODE_BASE;
    process->code_size = code_size;
    process->code_pages = code_pages;
    process->context.kernel_rsp = 0;
    process->context.kernel_cr3 = 0;
    process->context.kernel_rflags = 0;
    process->exit_code = 0;
    process->state = PROCESS_READY;

    *result = process;

    return true;
}

bool process_run(process_t *process)
{
    if (process == NULL ||
        process->state != PROCESS_READY ||
        current_process != NULL)
    {
        return false;
    }

    if (!address_space_switch(&process->address_space))
    {
        return false;
    }

    process->state = PROCESS_RUNNING;
    current_process = process;

    user_mode_enter(
        process->entry_point,
        process->stack_pointer,
        kernel_pml4,
        &process->context
    );

    current_process = NULL;

    if (process->state == PROCESS_RUNNING)
    {
        process->state = PROCESS_READY;
    }

    return true;
}

__attribute__((noreturn))
void process_exit(int64_t exit_code)
{
    if (current_process == NULL)
    {
        __asm__ volatile ("cli");

        for (;;)
        {
            __asm__ volatile ("hlt");
        }
    }

    current_process->exit_code = exit_code;
    current_process->state = PROCESS_TERMINATED;

    user_mode_exit();
}

bool process_destroy(uint64_t pid)
{
    process_t *process = process_find(pid);

    if (process == NULL ||
        process->state == PROCESS_RUNNING ||
        process == current_process)
    {
        return false;
    }

    if (!address_space_switch(&process->address_space))
    {
        return false;
    }

    bool code_freed =
        user_memory_free(
            process->code_address,
            process->code_pages
        );

    bool stack_freed =
        user_memory_destroy_stack();

    if (!vmm_load_pml4(kernel_pml4))
    {
        return false;
    }

    if (!code_freed || !stack_freed)
    {
        return false;
    }

    if (!address_space_destroy(&process->address_space))
    {
        return false;
    }

    process->pid = 0;
    process->state = PROCESS_UNUSED;
    process->entry_point = 0;
    process->stack_pointer = 0;
    process->code_address = 0;
    process->code_size = 0;
    process->code_pages = 0;
    process->context.kernel_rsp = 0;
    process->context.kernel_cr3 = 0;
    process->context.kernel_rflags = 0;
    process->exit_code = 0;

    return true;
}