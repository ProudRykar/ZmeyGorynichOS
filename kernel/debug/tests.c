#include "tests.h"

#include "terminal.h"

#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/heap.h"
#include "memory/address_space.h"
#include "memory/user_memory.h"

#include "process/process_manager.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USER_MEMORY_TEST_ADDRESS 0x400000ULL

static void debug_halt(void)
{
    for (;;)
    {
        __asm__ volatile ("cli; hlt");
    }
}

/*
 * Run tests for the Physical Memory Manager,
 * Virtual Memory Manager, and Kernel Heap.
 */
static void debug_test_pmm(void)
{
    pmm_stats_t memory_stats;

    terminal_write("");
    terminal_write("=== Physical Memory Manager ===");

    pmm_get_stats(&memory_stats);

    terminal_write_hex("Total pages", memory_stats.total_pages);
    terminal_write_hex("Used pages", memory_stats.used_pages);
    terminal_write_hex("Free pages", memory_stats.free_pages);

    terminal_write_hex(
        "Free memory (MiB)",
        memory_stats.free_pages * PMM_PAGE_SIZE / (1024 * 1024)
    );

    terminal_write("");
    terminal_write("=== PMM Allocation Test ===");

    uint64_t page_a = pmm_allocate_page();
    uint64_t page_b = pmm_allocate_page();

    terminal_write_hex("Page A", page_a);
    terminal_write_hex("Page B", page_b);

    if (page_a != 0 && page_b != 0 && page_a != page_b)
    {
        terminal_write("Allocation: SUCCESS");
    }
    else
    {
        terminal_write("Allocation: FAILED");
    }

    if (page_a != 0 && pmm_free_page(page_a))
    {
        terminal_write("Free page A: SUCCESS");
    }
    else
    {
        terminal_write("Free page A: FAILED");
    }

    if (page_b != 0 && pmm_free_page(page_b))
    {
        terminal_write("Free page B: SUCCESS");
    }
    else
    {
        terminal_write("Free page B: FAILED");
    }

    pmm_get_stats(&memory_stats);

    terminal_write_hex("Free pages after test", memory_stats.free_pages);
}

/*
 * Run tests for virtual page mapping and unmapping.
 */
static void debug_test_vmm(void)
{
    terminal_write("");
    terminal_write("=== Virtual Memory Manager ===");

    const uint64_t test_virtual_address = USER_MEMORY_TEST_ADDRESS;
    const uint64_t test_value = 0x5A4D475259;

    uint64_t test_physical_address = pmm_allocate_page();

    if (test_physical_address == 0)
    {
        terminal_write("Physical allocation: FAILED");
        return;
    }

    terminal_write_hex("Physical address", test_physical_address);

    if (!vmm_map_page(
        test_virtual_address,
        test_physical_address,
        VMM_PAGE_WRITE
    ))
    {
        terminal_write("Page mapping: FAILED");
        pmm_free_page(test_physical_address);
        return;
    }

    terminal_write("Page mapping: SUCCESS");

    volatile uint64_t *mapped_memory =
        (volatile uint64_t *)test_virtual_address;

    *mapped_memory = test_value;

    if (*mapped_memory == test_value)
    {
        terminal_write("Memory access: SUCCESS");
    }
    else
    {
        terminal_write("Memory access: FAILED");
    }

    uint64_t unmapped_physical_address = 0;

    if (!vmm_unmap_page(
        test_virtual_address,
        &unmapped_physical_address
    ))
    {
        terminal_write("Page unmapping: FAILED");

        /*
         * Do not release a physical page while it may
         * still be mapped into the current address space.
         */

        debug_halt();
    }

    terminal_write("Page unmapping: SUCCESS");

    terminal_write_hex(
        "Unmapped physical address",
        unmapped_physical_address
    );

    if (unmapped_physical_address == test_physical_address)
    {
        terminal_write("Physical address verification: SUCCESS");
    }
    else
    {
        terminal_write("Physical address verification: FAILED");
    }

    if (!pmm_free_page(unmapped_physical_address))
    {
        terminal_write("Physical page deallocation: FAILED");
        debug_halt();
    }
}

/*
 * Run tests for the Kernel Heap.
 */
static void debug_test_heap(void)
{
    terminal_write("");
    terminal_write("=== Kernel Heap Test ===");

    uint64_t *block_a = kmalloc(sizeof(uint64_t));
    uint64_t *block_b = kcalloc(4, sizeof(uint64_t));

    if (block_a != NULL && block_b != NULL)
    {
        terminal_write("Memory allocation: SUCCESS");

        *block_a = 0x5A4D475259;

        bool zero_initialized = true;

        for (size_t i = 0; i < 4; i++)
        {
            if (block_b[i] != 0)
            {
                zero_initialized = false;
                break;
            }
        }

        terminal_write(
            zero_initialized
                ? "Calloc initialization: SUCCESS"
                : "Calloc initialization: FAILED"
        );

        uint64_t *resized_block =
            krealloc(block_a, 128);

        if (resized_block != NULL)
        {
            terminal_write(
                resized_block[0] == 0x5A4D475259
                    ? "Reallocation: SUCCESS"
                    : "Reallocation: FAILED"
            );

            block_a = resized_block;
        }
        else
        {
            terminal_write("Reallocation: FAILED");
        }

        kfree(block_a);
        kfree(block_b);

        terminal_write("Memory deallocation: SUCCESS");
    }
    else
    {
        terminal_write("Memory allocation: FAILED");

        kfree(block_a);
        kfree(block_b);
    }
}

/*
 * Run tests for the direct physical memory mapping.
 */
static void debug_test_direct_map(void)
{
    terminal_write("");
    terminal_write("=== Direct Physical Map Test ===");

    uint64_t test_physical_address = pmm_allocate_page();

    if (test_physical_address == 0)
    {
        terminal_write("Physical allocation: FAILED");
        return;
    }

    uint64_t test_virtual_address =
        VMM_PHYSICAL_TO_VIRTUAL(test_physical_address);

    terminal_write("Step 1: virtual address calculated");

    terminal_write_hex(
        "Virtual address",
        test_virtual_address
    );

    terminal_write("Step 2: before address translation");

    uint64_t translated_address =
        vmm_get_physical_address(test_virtual_address);

    terminal_write("Step 3: after address translation");

    terminal_write_hex(
        "Translated address",
        translated_address
    );

    if (translated_address == test_physical_address)
    {
        terminal_write("Address translation: SUCCESS");
    }
    else
    {
        terminal_write("Address translation: FAILED");
    }

    terminal_write("Step 4: before memory access");

    volatile uint64_t *memory =
        (volatile uint64_t *)test_virtual_address;

    *memory = 0x5A4D475259;

    terminal_write("Step 5: after memory write");

    if (*memory == 0x5A4D475259)
    {
        terminal_write("Direct memory access: SUCCESS");
    }
    else
    {
        terminal_write("Direct memory access: FAILED");
    }

    if (!pmm_free_page(test_physical_address))
    {
        terminal_write("Physical page deallocation: FAILED");
        debug_halt();
    }
}

/*
 * Run tests for address space creation, switching,
 * kernel mapping, and destruction.
 */
static void debug_test_address_space(void)
{
    terminal_write("");
    terminal_write("=== Address Space Manager ===");

    address_space_t test_address_space = {0};

    uint64_t kernel_pml4 =
        vmm_get_current_pml4_physical();

    if (!address_space_create(&test_address_space))
    {
        terminal_write("Address space creation: FAILED");
        return;
    }

    terminal_write("Address space creation: SUCCESS");

    if (address_space_switch(&test_address_space))
    {
        terminal_write("Address space switch: SUCCESS");

        uint64_t kernel_address =
            vmm_get_physical_address(0x10000);

        if (kernel_address == 0x10000)
        {
            terminal_write("Kernel mapping: SUCCESS");
        }
        else
        {
            terminal_write("Kernel mapping: FAILED");
        }
    }
    else
    {
        terminal_write("Address space switch: FAILED");
    }

    /*
     * Restore the original kernel address space
     * before destroying the test address space.
     */

    if (!vmm_load_pml4(kernel_pml4))
    {
        debug_halt();
    }

    terminal_write("Kernel address space restored");

    if (address_space_destroy(&test_address_space))
    {
        terminal_write("Address space destruction: SUCCESS");
    }
    else
    {
        terminal_write("Address space destruction: FAILED");
    }
}

/*
 * Run tests for user memory allocation,
 * user permissions, stack allocation, and address space isolation.
 */
static void debug_test_user_memory(void)
{
    terminal_write("");
    terminal_write("=== User Memory Manager ===");

    address_space_t user_space_a = {0};
    address_space_t user_space_b = {0};

    uint64_t kernel_pml4 =
        vmm_get_current_pml4_physical();

    uint64_t physical_a = 0;
    uint64_t physical_b = 0;

    uint64_t user_stack_pointer = 0;

    bool user_test_success = true;

    bool user_space_a_created = false;
    bool user_space_b_created = false;

    bool user_page_a_allocated = false;
    bool user_page_b_allocated = false;
    bool user_stack_allocated = false;

    if (!address_space_create(&user_space_a))
    {
        terminal_write("Address space A: FAILED");
        user_test_success = false;
    }
    else
    {
        terminal_write("Address space A: SUCCESS");
        user_space_a_created = true;
    }

    if (user_test_success &&
        !address_space_create(&user_space_b))
    {
        terminal_write("Address space B: FAILED");
        user_test_success = false;
    }
    else if (user_test_success)
    {
        terminal_write("Address space B: SUCCESS");
        user_space_b_created = true;
    }

    /*
     * Allocate memory in address space A.
     */

    if (user_test_success &&
        address_space_switch(&user_space_a))
    {
        terminal_write("Switch to address space A: SUCCESS");

        if (user_memory_allocate(USER_MEMORY_TEST_ADDRESS, 1))
        {
            volatile uint64_t *memory =
                (volatile uint64_t *)(uintptr_t)USER_MEMORY_TEST_ADDRESS;

            *memory = 0xAAAAAAAAAAAAAAAAULL;

            physical_a =
                vmm_get_physical_address(USER_MEMORY_TEST_ADDRESS);

            terminal_write("User page A allocation: SUCCESS");

            user_page_a_allocated = true;

            if (vmm_is_user_accessible(USER_MEMORY_TEST_ADDRESS))
            {
                terminal_write("User permissions A: SUCCESS");
            }
            else
            {
                terminal_write("User permissions A: FAILED");
                user_test_success = false;
            }
        }
        else
        {
            terminal_write("User page A allocation: FAILED");
            user_test_success = false;
        }

        if (user_test_success &&
            user_memory_create_stack(&user_stack_pointer))
        {
            terminal_write("User stack allocation: SUCCESS");
            user_stack_allocated = true;
        }
        else if (user_test_success)
        {
            terminal_write("User stack allocation: FAILED");
            user_test_success = false;
        }
    }
    else if (user_test_success)
    {
        terminal_write("Switch to address space A: FAILED");
        user_test_success = false;
    }

    /*
     * Allocate memory at the same virtual address in B.
     */

    if (user_test_success &&
        address_space_switch(&user_space_b))
    {
        terminal_write("Switch to address space B: SUCCESS");

        if (user_memory_allocate(USER_MEMORY_TEST_ADDRESS, 1))
        {
            volatile uint64_t *memory =
                (volatile uint64_t *)(uintptr_t)USER_MEMORY_TEST_ADDRESS;

            *memory = 0xBBBBBBBBBBBBBBBBULL;

            physical_b =
                vmm_get_physical_address(USER_MEMORY_TEST_ADDRESS);

            terminal_write("User page B allocation: SUCCESS");

            user_page_b_allocated = true;

            if (vmm_is_user_accessible(USER_MEMORY_TEST_ADDRESS))
            {
                terminal_write("User permissions B: SUCCESS");
            }
            else
            {
                terminal_write("User permissions B: FAILED");
                user_test_success = false;
            }

            if (physical_a != physical_b)
            {
                terminal_write("Physical page isolation: SUCCESS");
            }
            else
            {
                terminal_write("Physical page isolation: FAILED");
                user_test_success = false;
            }
        }
        else
        {
            terminal_write("User page B allocation: FAILED");
            user_test_success = false;
        }
    }
    else if (user_test_success)
    {
        terminal_write("Switch to address space B: FAILED");
        user_test_success = false;
    }

    /*
     * Verify that address space A retains its own data.
     */

    if (user_page_a_allocated)
    {
        if (address_space_switch(&user_space_a))
        {
            volatile uint64_t *memory =
                (volatile uint64_t *)(uintptr_t)USER_MEMORY_TEST_ADDRESS;

            if (*memory == 0xAAAAAAAAAAAAAAAAULL)
            {
                terminal_write("Address space A isolation: SUCCESS");
            }
            else
            {
                terminal_write("Address space A isolation: FAILED");
                user_test_success = false;
            }
        }
        else
        {
            terminal_write("Return to address space A: FAILED");
            user_test_success = false;
        }
    }

    /*
     * Verify that address space B retains its own data.
     */

    if (user_page_b_allocated)
    {
        if (address_space_switch(&user_space_b))
        {
            volatile uint64_t *memory =
                (volatile uint64_t *)(uintptr_t)USER_MEMORY_TEST_ADDRESS;

            if (*memory == 0xBBBBBBBBBBBBBBBBULL)
            {
                terminal_write("Address space B isolation: SUCCESS");
            }
            else
            {
                terminal_write("Address space B isolation: FAILED");
                user_test_success = false;
            }
        }
        else
        {
            terminal_write("Return to address space B: FAILED");
            user_test_success = false;
        }
    }

    /*
     * Release user memory from address space A.
     */

    if (user_space_a_created &&
        address_space_switch(&user_space_a))
    {
        if (user_page_a_allocated)
        {
            if (!user_memory_free(USER_MEMORY_TEST_ADDRESS, 1))
            {
                user_test_success = false;
                terminal_write("User page A deallocation: FAILED");
            }
            else
            {
                terminal_write("User page A deallocation: SUCCESS");
            }
        }

        if (user_stack_allocated)
        {
            if (!user_memory_destroy_stack())
            {
                user_test_success = false;
                terminal_write("User stack deallocation: FAILED");
            }
            else
            {
                terminal_write("User stack deallocation: SUCCESS");
            }
        }
    }
    else if (user_space_a_created &&
             (user_page_a_allocated || user_stack_allocated))
    {
        terminal_write("Address space A cleanup: FAILED");
        debug_halt();
    }

    /*
     * Release user memory from address space B.
     */

    if (user_space_b_created &&
        address_space_switch(&user_space_b))
    {
        if (user_page_b_allocated)
        {
            if (!user_memory_free(USER_MEMORY_TEST_ADDRESS, 1))
            {
                user_test_success = false;
                terminal_write("User page B deallocation: FAILED");
            }
            else
            {
                terminal_write("User page B deallocation: SUCCESS");
            }
        }
    }
    else if (user_space_b_created && user_page_b_allocated)
    {
        terminal_write("Address space B cleanup: FAILED");
        debug_halt();
    }

    /*
     * Restore kernel address space and destroy both spaces.
     */

    if (!vmm_load_pml4(kernel_pml4))
    {
        debug_halt();
    }

    if (user_space_a_created)
    {
        if (address_space_destroy(&user_space_a))
        {
            terminal_write("Address space A destruction: SUCCESS");
        }
        else
        {
            terminal_write("Address space A destruction: FAILED");
            user_test_success = false;
        }
    }

    if (user_space_b_created)
    {
        if (address_space_destroy(&user_space_b))
        {
            terminal_write("Address space B destruction: SUCCESS");
        }
        else
        {
            terminal_write("Address space B destruction: FAILED");
            user_test_success = false;
        }
    }

    if (user_test_success)
    {
        terminal_write("User Memory Manager: SUCCESS");
    }
    else
    {
        terminal_write("User Memory Manager: FAILED");
    }
}

/*
 * Run all memory subsystem tests.
 *
 * PMM, VMM, and Heap must already be initialized.
 */
void debug_run_memory_tests(void)
{
    debug_test_pmm();
    debug_test_vmm();
    debug_test_heap();
    debug_test_direct_map();
    debug_test_address_space();
    debug_test_user_memory();
}

/*
 * Run tests for process creation, PID uniqueness,
 * address space isolation, process lookup, and destruction.
 */
void debug_run_process_tests(void)
{
    terminal_write("");
    terminal_write("=== Process Creation Test ===");

    static const uint8_t process_code_a[] =
    {
        0xEB, 0xFE
    };

    static const uint8_t process_code_b[] =
    {
        0x90,
        0xEB, 0xFD
    };

    process_t *process_a = NULL;
    process_t *process_b = NULL;

    if (process_create(
        process_code_a,
        sizeof(process_code_a),
        &process_a
    ))
    {
        terminal_write("Process A creation: SUCCESS");
        terminal_write_hex("Process A PID", process_a->pid);
    }
    else
    {
        terminal_write("Process A creation: FAILED");
    }

    if (process_create(
        process_code_b,
        sizeof(process_code_b),
        &process_b
    ))
    {
        terminal_write("Process B creation: SUCCESS");
        terminal_write_hex("Process B PID", process_b->pid);
    }
    else
    {
        terminal_write("Process B creation: FAILED");
    }

    if (process_a != NULL && process_b != NULL)
    {
        if (process_a->pid != process_b->pid)
        {
            terminal_write("PID uniqueness: SUCCESS");
        }
        else
        {
            terminal_write("PID uniqueness: FAILED");
        }

        if (process_a->address_space.pml4_physical !=
            process_b->address_space.pml4_physical)
        {
            terminal_write("Address space isolation: SUCCESS");
        }
        else
        {
            terminal_write("Address space isolation: FAILED");
        }

        if (process_find(process_a->pid) == process_a &&
            process_find(process_b->pid) == process_b)
        {
            terminal_write("Process lookup: SUCCESS");
        }
        else
        {
            terminal_write("Process lookup: FAILED");
        }
    }

    if (process_a != NULL)
    {
        if (process_destroy(process_a->pid))
        {
            terminal_write("Process A destruction: SUCCESS");
        }
        else
        {
            terminal_write("Process A destruction: FAILED");
        }
    }

    if (process_b != NULL)
    {
        if (process_destroy(process_b->pid))
        {
            terminal_write("Process B destruction: SUCCESS");
        }
        else
        {
            terminal_write("Process B destruction: FAILED");
        }
    }
}

/*
 * Run a user process and verify SYS_WRITE, SYS_GETPID,
 * SYS_EXIT, and process destruction.
 */
void debug_run_ring3_tests(void)
{
    terminal_write("");
    terminal_write("=== Ring 3 Process Execution Test ===");

    static const uint8_t user_code[] =
    {
        // SYS_WRITE
        0xB8, 0x01, 0x00, 0x00, 0x00, // mov eax, 1
        0xBF, 0x28, 0x00, 0x40, 0x00, // mov edi, 0x400028
        0xBE, 0x13, 0x00, 0x00, 0x00, // mov esi, 19
        0xCD, 0x80,                   // int 0x80

        // SYS_GETPID
        0xB8, 0x02, 0x00, 0x00, 0x00, // mov eax, 2
        0xCD, 0x80,                   // int 0x80

        // Preserve PID as exit code.
        0x89, 0xC7,                   // mov edi, eax

        // SYS_EXIT
        0xB8, 0x00, 0x00, 0x00, 0x00, // mov eax, 0
        0xCD, 0x80,                   // int 0x80

        // Padding: offset 33–39
        0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90,

        // Message: offset 40 (0x400028)
        'H', 'e', 'l', 'l', 'o', ' ',
        'f', 'r', 'o', 'm', ' ',
        'R', 'i', 'n', 'g', ' ',
        '3', '!', '\n'
    };

    process_t *user_process = NULL;

    if (!process_create(
        user_code,
        sizeof(user_code),
        &user_process
    ))
    {
        terminal_write("Process creation: FAILED");
        return;
    }

    terminal_write("Process creation: SUCCESS");

    terminal_write_hex(
        "Process PID",
        user_process->pid
    );

    terminal_write("Starting process...");

    if (process_run(user_process))
    {
        terminal_write("Process execution: SUCCESS");

        terminal_write_hex(
            "Process exit code",
            (uint64_t)user_process->exit_code
        );

        if (user_process->state == PROCESS_TERMINATED &&
            user_process->exit_code == (int64_t)user_process->pid)
        {
            terminal_write("SYS_GETPID: SUCCESS");
            terminal_write("SYS_EXIT: SUCCESS");
        }
        else
        {
            terminal_write("Process termination: FAILED");
        }
    }
    else
    {
        terminal_write("Process execution: FAILED");
    }

    if (process_destroy(user_process->pid))
    {
        terminal_write("Process destruction: SUCCESS");
    }
    else
    {
        terminal_write("Process destruction: FAILED");
    }
}