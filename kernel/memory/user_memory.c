#include "user_memory.h"

#include "pmm.h"
#include "vmm.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USER_MEMORY_PAGE_SIZE 4096ULL

#define USER_MEMORY_LIMIT 0x0000800000000000ULL

#define USER_MEMORY_PAGE_FLAGS \
    (VMM_PAGE_WRITE | VMM_PAGE_USER)

static uint64_t user_memory_stack_base =
    USER_MEMORY_STACK_TOP -
    USER_MEMORY_STACK_PAGES * USER_MEMORY_PAGE_SIZE;

static bool user_memory_validate_range(
    uint64_t virtual_address,
    size_t page_count
)
{
    if (page_count == 0 ||
        virtual_address % USER_MEMORY_PAGE_SIZE != 0)
    {
        return false;
    }

    if (virtual_address >= USER_MEMORY_LIMIT)
    {
        return false;
    }

    if (page_count >
        (USER_MEMORY_LIMIT - virtual_address) /
        USER_MEMORY_PAGE_SIZE)
    {
        return false;
    }

    return true;
}

bool user_memory_allocate(
    uint64_t virtual_address,
    size_t page_count
)
{
    if (!user_memory_validate_range(
        virtual_address,
        page_count
    ))
    {
        return false;
    }

    size_t allocated_pages = 0;

    for (; allocated_pages < page_count; allocated_pages++)
    {
        uint64_t physical_address = pmm_allocate_page();

        if (physical_address == 0)
        {
            break;
        }

        uint64_t current_virtual_address =
            virtual_address +
            allocated_pages * USER_MEMORY_PAGE_SIZE;

        if (!vmm_map_page(
            current_virtual_address,
            physical_address,
            USER_MEMORY_PAGE_FLAGS
        ))
        {
            pmm_free_page(physical_address);
            break;
        }

        uint8_t *page =
            (uint8_t *)(uintptr_t)current_virtual_address;

        for (size_t i = 0; i < USER_MEMORY_PAGE_SIZE; i++)
        {
            page[i] = 0;
        }
    }

    if (allocated_pages == page_count)
    {
        return true;
    }

    /*
     * Roll back all pages allocated before the failure.
     */

    for (size_t i = 0; i < allocated_pages; i++)
    {
        uint64_t current_virtual_address =
            virtual_address +
            i * USER_MEMORY_PAGE_SIZE;

        uint64_t physical_address = 0;

        if (vmm_unmap_page(
            current_virtual_address,
            &physical_address
        ))
        {
            pmm_free_page(physical_address);
        }
    }

    return false;
}

bool user_memory_free(
    uint64_t virtual_address,
    size_t page_count
)
{
    if (!user_memory_validate_range(
        virtual_address,
        page_count
    ))
    {
        return false;
    }

    /*
     * Verify that every page is mapped before freeing anything.
     */

    for (size_t i = 0; i < page_count; i++)
    {
        uint64_t current_virtual_address =
            virtual_address +
            i * USER_MEMORY_PAGE_SIZE;

        if (vmm_get_physical_address(
            current_virtual_address
        ) == 0)
        {
            return false;
        }
    }

    for (size_t i = 0; i < page_count; i++)
    {
        uint64_t current_virtual_address =
            virtual_address +
            i * USER_MEMORY_PAGE_SIZE;

        uint64_t physical_address = 0;

        if (!vmm_unmap_page(
            current_virtual_address,
            &physical_address
        ))
        {
            return false;
        }

        if (!pmm_free_page(physical_address))
        {
            return false;
        }
    }

    return true;
}

bool user_memory_create_stack(uint64_t *stack_pointer)
{
    if (stack_pointer == NULL)
    {
        return false;
    }

    if (!user_memory_allocate(
        user_memory_stack_base,
        USER_MEMORY_STACK_PAGES
    ))
    {
        return false;
    }

    *stack_pointer = USER_MEMORY_STACK_TOP;

    return true;
}

bool user_memory_destroy_stack(void)
{
    return user_memory_free(
        user_memory_stack_base,
        USER_MEMORY_STACK_PAGES
    );
}