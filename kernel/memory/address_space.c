#include "address_space.h"
#include "pmm.h"
#include "vmm.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define ADDRESS_SPACE_ENTRIES 512

#define ADDRESS_SPACE_USER_ENTRIES 256

#define ADDRESS_SPACE_PAGE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL

#define ADDRESS_SPACE_PAGE_PRESENT (1ULL << 0)

#define ADDRESS_SPACE_PAGE_HUGE (1ULL << 7)

static void *address_space_physical_to_virtual(uint64_t physical_address)
{
    if (physical_address < PMM_RESERVED_MEMORY_END)
    {
        return (void *)physical_address;
    }

    return (void *)VMM_PHYSICAL_TO_VIRTUAL(physical_address);
}

static void address_space_clear(uint64_t *table)
{
    for (size_t i = 0; i < ADDRESS_SPACE_ENTRIES; i++)
    {
        table[i] = 0;
    }
}

/*
 * Release a page-table hierarchy.
 *
 * Level 1 contains page entries and does not own the
 * physical pages referenced by those entries.
 *
 * Huge pages are not page-table pointers and must not
 * be recursively released.
 */

static void address_space_release_table(
    uint64_t physical_address,
    uint8_t level
)
{
    if (physical_address == 0 || level == 0)
    {
        return;
    }

    uint64_t *table =
        (uint64_t *)address_space_physical_to_virtual(physical_address);

    if (table == NULL)
    {
        return;
    }

    if (level > 1)
    {
        for (size_t i = 0; i < ADDRESS_SPACE_ENTRIES; i++)
        {
            uint64_t entry = table[i];

            if ((entry & ADDRESS_SPACE_PAGE_PRESENT) == 0)
            {
                continue;
            }

            if ((level == 3 || level == 2) &&
                (entry & ADDRESS_SPACE_PAGE_HUGE))
            {
                continue;
            }

            uint64_t child_physical_address =
                entry & ADDRESS_SPACE_PAGE_ADDRESS_MASK;

            address_space_release_table(
                child_physical_address,
                level - 1
            );
        }
    }

    pmm_free_page(physical_address);
}

/*
 * Clone a page-table hierarchy.
 *
 * Page-table pages are duplicated, while the physical
 * memory referenced by leaf entries remains shared.
 *
 * This allows each address space to modify its own
 * virtual mappings without changing another address space.
 */

/*
 * Clone a page-table hierarchy.
 *
 * Page-table pages are duplicated, while the physical
 * memory referenced by leaf entries remains shared.
 *
 * This allows each address space to modify its own
 * virtual mappings without changing another address space.
 */

static bool address_space_clone_table(
    uint64_t source_physical_address,
    uint8_t level,
    uint64_t *destination_physical_address
)
{
    if (level == 0 || destination_physical_address == NULL)
    {
        return false;
    }

    uint64_t new_physical_address = pmm_allocate_page();

    if (new_physical_address == 0)
    {
        return false;
    }

    uint64_t *source =
        (uint64_t *)address_space_physical_to_virtual(
            source_physical_address
        );

    uint64_t *destination =
        (uint64_t *)address_space_physical_to_virtual(
            new_physical_address
        );

    if (source == NULL || destination == NULL)
    {
        pmm_free_page(new_physical_address);
        return false;
    }

    /*
     * Start with an empty table.
     *
     * Only successfully cloned entries will be inserted.
     * This makes recursive cleanup safe.
     */

    address_space_clear(destination);

    for (size_t i = 0; i < ADDRESS_SPACE_ENTRIES; i++)
    {
        uint64_t entry = source[i];

        if ((entry & ADDRESS_SPACE_PAGE_PRESENT) == 0)
        {
            continue;
        }

        /*
         * Leaf entries and huge pages reference physical
         * memory directly, not another page table.
         */

        if (level == 1 ||
            ((level == 3 || level == 2) &&
             (entry & ADDRESS_SPACE_PAGE_HUGE)))
        {
            destination[i] = entry;
            continue;
        }

        uint64_t child_physical_address =
            entry & ADDRESS_SPACE_PAGE_ADDRESS_MASK;

        uint64_t cloned_child_physical_address = 0;

        if (!address_space_clone_table(
            child_physical_address,
            level - 1,
            &cloned_child_physical_address
        ))
        {
            address_space_release_table(
                new_physical_address,
                level
            );

            return false;
        }

        destination[i] =
            cloned_child_physical_address |
            (entry & ~ADDRESS_SPACE_PAGE_ADDRESS_MASK);
    }

    *destination_physical_address = new_physical_address;

    return true;
}

bool address_space_create(address_space_t *address_space)
{
    if (address_space == NULL || address_space->initialized)
    {
        return false;
    }

    uint64_t *current_pml4 = vmm_get_current_pml4();

    if (current_pml4 == NULL)
    {
        return false;
    }

    uint64_t new_pml4_physical = pmm_allocate_page();

    if (new_pml4_physical == 0)
    {
        return false;
    }

    uint64_t *new_pml4 =
        (uint64_t *)address_space_physical_to_virtual(
            new_pml4_physical
        );

    if (new_pml4 == NULL)
    {
        pmm_free_page(new_pml4_physical);
        return false;
    }

    address_space_clear(new_pml4);

    /*
     * Copy the kernel half of the address space.
     *
     * Kernel mappings are shared between address spaces.
     */

    for (size_t i = ADDRESS_SPACE_USER_ENTRIES;
         i < ADDRESS_SPACE_ENTRIES;
         i++)
    {
        new_pml4[i] = current_pml4[i];
    }

    /*
     * Clone the user half of the address space.
     *
     * Each address space receives independent page tables.
     */

    for (size_t i = 0; i < ADDRESS_SPACE_USER_ENTRIES; i++)
    {
        uint64_t entry = current_pml4[i];

        if ((entry & ADDRESS_SPACE_PAGE_PRESENT) == 0)
        {
            continue;
        }

        uint64_t source_pdpt_physical =
            entry & ADDRESS_SPACE_PAGE_ADDRESS_MASK;

        uint64_t cloned_pdpt_physical = 0;

        if (!address_space_clone_table(
            source_pdpt_physical,
            3,
            &cloned_pdpt_physical
        ))
        {
            for (size_t j = 0; j < i; j++)
            {
                uint64_t previous_entry = new_pml4[j];

                if (previous_entry & ADDRESS_SPACE_PAGE_PRESENT)
                {
                    address_space_release_table(
                        previous_entry & ADDRESS_SPACE_PAGE_ADDRESS_MASK,
                        3
                    );
                }
            }

            pmm_free_page(new_pml4_physical);

            return false;
        }

        new_pml4[i] =
            cloned_pdpt_physical |
            (entry & ~ADDRESS_SPACE_PAGE_ADDRESS_MASK);
    }

    address_space->pml4_physical = new_pml4_physical;
    address_space->initialized = true;

    return true;
}

bool address_space_switch(const address_space_t *address_space)
{
    if (address_space == NULL ||
        !address_space->initialized)
    {
        return false;
    }

    return vmm_load_pml4(address_space->pml4_physical);
}

bool address_space_destroy(address_space_t *address_space)
{
    if (address_space == NULL ||
        !address_space->initialized)
    {
        return false;
    }

    /*
     * An active address space cannot be destroyed.
     */

    if (vmm_get_current_pml4_physical() ==
        address_space->pml4_physical)
    {
        return false;
    }

    uint64_t *pml4 =
        (uint64_t *)address_space_physical_to_virtual(
            address_space->pml4_physical
        );

    if (pml4 == NULL)
    {
        return false;
    }

    /*
     * Release only the user half.
     *
     * Kernel page tables are shared and must remain alive.
     */

    for (size_t i = 0; i < ADDRESS_SPACE_USER_ENTRIES; i++)
    {
        uint64_t entry = pml4[i];

        if ((entry & ADDRESS_SPACE_PAGE_PRESENT) == 0)
        {
            continue;
        }

        address_space_release_table(
            entry & ADDRESS_SPACE_PAGE_ADDRESS_MASK,
            3
        );
    }

    pmm_free_page(address_space->pml4_physical);

    address_space->pml4_physical = 0;
    address_space->initialized = false;

    return true;
}