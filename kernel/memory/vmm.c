#include "vmm.h"
#include "pmm.h"

#include <stddef.h>

#define VMM_PAGE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL

#define VMM_PAGE_HUGE (1ULL << 7)

#define VMM_IDENTITY_LIMIT 0x200000ULL

#define VMM_ENTRIES_PER_TABLE 512

#define VMM_1G_PAGE_SIZE (1ULL << 30)
#define VMM_2M_PAGE_SIZE (1ULL << 21)

#define VMM_1G_ADDRESS_MASK 0x000FFFFFC0000000ULL
#define VMM_2M_ADDRESS_MASK 0x000FFFFFFFE00000ULL

static uint64_t *vmm_pml4 = NULL;

static bool vmm_direct_map_initialized = false;

static void vmm_invalidate_page(uint64_t address)
{
    __asm__ volatile (
        "invlpg (%0)"
        :
        : "r"((void *)address)
        : "memory"
    );
}

static bool vmm_is_canonical(uint64_t address)
{
    uint64_t upper_bits = address >> 48;
    uint64_t sign_bit = (address >> 47) & 1;

    return upper_bits == (sign_bit ? 0xFFFF : 0);
}

static void *vmm_physical_to_virtual(uint64_t address)
{
    /*
     * The first 2 MiB remain accessible through the initial
     * identity mapping, including the bootloader page tables.
     */

    if (address < VMM_IDENTITY_LIMIT)
    {
        return (void *)address;
    }

    /*
     * Physical memory above 2 MiB is accessed through
     * the direct physical memory map after initialization.
     */

    if (vmm_direct_map_initialized)
    {
        return (void *)VMM_PHYSICAL_TO_VIRTUAL(address);
    }

    return NULL;
}

static uint64_t *vmm_allocate_table(void)
{
    uint64_t physical_address = pmm_allocate_page();

    if (physical_address == 0)
    {
        return NULL;
    }

    void *virtual_address =
        vmm_physical_to_virtual(physical_address);

    if (virtual_address == NULL)
    {
        pmm_free_page(physical_address);
        return NULL;
    }

    uint64_t *table = (uint64_t *)virtual_address;

    for (size_t i = 0; i < VMM_ENTRIES_PER_TABLE; i++)
    {
        table[i] = 0;
    }

    return table;
}

static uint64_t *vmm_get_table(
    uint64_t *parent,
    uint64_t index,
    uint64_t flags,
    bool create
)
{
    uint64_t entry = parent[index];

    if ((entry & VMM_PAGE_PRESENT) == 0)
    {
        if (!create)
        {
            return NULL;
        }

        uint64_t *new_table = vmm_allocate_table();

        if (new_table == NULL)
        {
            return NULL;
        }

        uint64_t table_address =
            (uint64_t)new_table;

        if (table_address >= VMM_DIRECT_MAP_BASE)
        {
            table_address -= VMM_DIRECT_MAP_BASE;
        }

        parent[index] =
            table_address |
            VMM_PAGE_PRESENT |
            VMM_PAGE_WRITE |
            (flags & VMM_PAGE_USER);

        return new_table;
    }

    if (entry & VMM_PAGE_HUGE)
    {
        return NULL;
    }

    if (flags & VMM_PAGE_USER)
    {
        parent[index] |= VMM_PAGE_USER;
    }

    uint64_t table_address =
        entry & VMM_PAGE_ADDRESS_MASK;

    return (uint64_t *)vmm_physical_to_virtual(table_address);
}

bool vmm_initialize(const boot_info_t *boot_info)
{
    if (boot_info == NULL)
    {
        return false;
    }

    uint64_t cr3;

    __asm__ volatile (
        "mov %%cr3, %0"
        : "=r"(cr3)
    );

    uint64_t pml4_address =
        cr3 & VMM_PAGE_ADDRESS_MASK;

    if (pml4_address >= VMM_IDENTITY_LIMIT)
    {
        return false;
    }

    vmm_pml4 = (uint64_t *)pml4_address;

    /*
     * Build the direct physical memory map.
     *
     * Only E820 available memory regions are mapped.
     * The first MiB remains reserved by the PMM.
     *
     * During initialization, page tables are allocated
     * from the identity-mapped first 2 MiB.
     */

    for (uint32_t i = 0; i < boot_info->memory_map_count; i++)
    {
        const memory_map_entry_t *entry =
            &boot_info->memory_map[i];

        if (entry->type != MEMORY_TYPE_AVAILABLE)
        {
            continue;
        }

        if ((entry->attributes & 1) == 0)
        {
            continue;
        }

        if (entry->length == 0 ||
            entry->base >= PMM_MAX_MEMORY)
        {
            continue;
        }

        uint64_t start =
            (entry->base + VMM_PAGE_SIZE - 1) &
            ~(VMM_PAGE_SIZE - 1);

        uint64_t end = entry->base + entry->length;

        if (end < entry->base || end > PMM_MAX_MEMORY)
        {
            end = PMM_MAX_MEMORY;
        }

        end &= ~(VMM_PAGE_SIZE - 1);

        if (start < PMM_RESERVED_MEMORY_END)
        {
            start = PMM_RESERVED_MEMORY_END;
        }

        for (uint64_t physical_address = start;
             physical_address < end;
             physical_address += VMM_PAGE_SIZE)
        {
            uint64_t virtual_address =
                VMM_PHYSICAL_TO_VIRTUAL(physical_address);

            if (!vmm_map_page(
                virtual_address,
                physical_address,
                VMM_PAGE_WRITE
            ))
            {
                return false;
            }
        }
    }

    /*
     * All available physical memory is now accessible
     * through the direct physical memory map.
     *
     * Page tables can now be accessed through the
     * direct map instead of the initial identity map.
     */

    vmm_direct_map_initialized = true;

    return true;
}

bool vmm_map_page(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags
)
{
    if (vmm_pml4 == NULL)
    {
        return false;
    }

    if (!vmm_is_canonical(virtual_address))
    {
        return false;
    }

    if (virtual_address % VMM_PAGE_SIZE != 0 ||
        physical_address % VMM_PAGE_SIZE != 0)
    {
        return false;
    }

    if (physical_address >= PMM_MAX_MEMORY)
    {
        return false;
    }

    uint64_t pml4_index =
        (virtual_address >> 39) & 0x1FF;

    uint64_t pdpt_index =
        (virtual_address >> 30) & 0x1FF;

    uint64_t pd_index =
        (virtual_address >> 21) & 0x1FF;

    uint64_t pt_index =
        (virtual_address >> 12) & 0x1FF;

    uint64_t *pdpt = vmm_get_table(
        vmm_pml4,
        pml4_index,
        flags,
        true
    );

    if (pdpt == NULL)
    {
        return false;
    }

    uint64_t *pd = vmm_get_table(
        pdpt,
        pdpt_index,
        flags,
        true
    );

    if (pd == NULL)
    {
        return false;
    }

    uint64_t *pt = vmm_get_table(
        pd,
        pd_index,
        flags,
        true
    );

    if (pt == NULL)
    {
        return false;
    }

    if (pt[pt_index] & VMM_PAGE_PRESENT)
    {
        return false;
    }

    pt[pt_index] =
        physical_address |
        VMM_PAGE_PRESENT |
        (flags & 0xFFF);

    vmm_invalidate_page(virtual_address);

    return true;
}

bool vmm_unmap_page(
    uint64_t virtual_address,
    uint64_t *physical_address
)
{
    if (vmm_pml4 == NULL)
    {
        return false;
    }

    if (!vmm_is_canonical(virtual_address))
    {
        return false;
    }

    if (virtual_address % VMM_PAGE_SIZE != 0)
    {
        return false;
    }

    uint64_t pml4_index =
        (virtual_address >> 39) & 0x1FF;

    uint64_t pdpt_index =
        (virtual_address >> 30) & 0x1FF;

    uint64_t pd_index =
        (virtual_address >> 21) & 0x1FF;

    uint64_t pt_index =
        (virtual_address >> 12) & 0x1FF;

    uint64_t *pdpt = vmm_get_table(
        vmm_pml4,
        pml4_index,
        0,
        false
    );

    if (pdpt == NULL)
    {
        return false;
    }

    uint64_t *pd = vmm_get_table(
        pdpt,
        pdpt_index,
        0,
        false
    );

    if (pd == NULL)
    {
        return false;
    }

    uint64_t *pt = vmm_get_table(
        pd,
        pd_index,
        0,
        false
    );

    if (pt == NULL)
    {
        return false;
    }

    uint64_t entry = pt[pt_index];

    if ((entry & VMM_PAGE_PRESENT) == 0)
    {
        return false;
    }

    if (physical_address != NULL)
    {
        *physical_address =
            entry & VMM_PAGE_ADDRESS_MASK;
    }

    pt[pt_index] = 0;

    vmm_invalidate_page(virtual_address);

    return true;
}

uint64_t vmm_get_physical_address(uint64_t virtual_address)
{
    if (vmm_pml4 == NULL ||
        !vmm_is_canonical(virtual_address))
    {
        return 0;
    }

    uint64_t pml4_index =
        (virtual_address >> 39) & 0x1FF;

    uint64_t pdpt_index =
        (virtual_address >> 30) & 0x1FF;

    uint64_t pd_index =
        (virtual_address >> 21) & 0x1FF;

    uint64_t pt_index =
        (virtual_address >> 12) & 0x1FF;

    uint64_t *pdpt = vmm_get_table(
        vmm_pml4,
        pml4_index,
        0,
        false
    );

    if (pdpt == NULL)
    {
        return 0;
    }

    uint64_t pdpt_entry = pdpt[pdpt_index];

    if ((pdpt_entry & VMM_PAGE_PRESENT) == 0)
    {
        return 0;
    }

    if (pdpt_entry & VMM_PAGE_HUGE)
    {
        return (pdpt_entry & VMM_1G_ADDRESS_MASK) |
            (virtual_address & (VMM_1G_PAGE_SIZE - 1));
    }

    uint64_t *pd = vmm_get_table(
        pdpt,
        pdpt_index,
        0,
        false
    );

    if (pd == NULL)
    {
        return 0;
    }

    uint64_t pd_entry = pd[pd_index];

    if ((pd_entry & VMM_PAGE_PRESENT) == 0)
    {
        return 0;
    }

    if (pd_entry & VMM_PAGE_HUGE)
    {
        return (pd_entry & VMM_2M_ADDRESS_MASK) |
            (virtual_address & (VMM_2M_PAGE_SIZE - 1));
    }

    uint64_t *pt = vmm_get_table(
        pd,
        pd_index,
        0,
        false
    );

    if (pt == NULL)
    {
        return 0;
    }

    uint64_t pt_entry = pt[pt_index];

    if ((pt_entry & VMM_PAGE_PRESENT) == 0)
    {
        return 0;
    }

    return (pt_entry & VMM_PAGE_ADDRESS_MASK) |
        (virtual_address & (VMM_PAGE_SIZE - 1));
}

uint64_t vmm_get_current_pml4_physical(void)
{
    uint64_t cr3;

    __asm__ volatile (
        "mov %%cr3, %0"
        : "=r"(cr3)
    );

    return cr3 & VMM_PAGE_ADDRESS_MASK;
}

uint64_t *vmm_get_current_pml4(void)
{
    return vmm_pml4;
}

bool vmm_load_pml4(uint64_t physical_address)
{
    if (physical_address == 0 ||
        physical_address >= PMM_MAX_MEMORY ||
        physical_address % VMM_PAGE_SIZE != 0)
    {
        return false;
    }

    uint64_t *new_pml4 =
        (uint64_t *)vmm_physical_to_virtual(physical_address);

    if (new_pml4 == NULL)
    {
        return false;
    }

    vmm_pml4 = new_pml4;

    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"(physical_address)
        : "memory"
    );

    return true;
}

bool vmm_is_user_accessible(uint64_t virtual_address)
{
    if (vmm_pml4 == NULL)
    {
        return false;
    }

    uint64_t indices[4] =
    {
        (virtual_address >> 39) & 0x1FF,
        (virtual_address >> 30) & 0x1FF,
        (virtual_address >> 21) & 0x1FF,
        (virtual_address >> 12) & 0x1FF
    };

    uint64_t *table = vmm_pml4;

    for (size_t level = 0; level < 4; level++)
    {
        uint64_t entry = table[indices[level]];

        if ((entry & VMM_PAGE_PRESENT) == 0 ||
            (entry & VMM_PAGE_USER) == 0)
        {
            return false;
        }

        if (level == 3)
        {
            return true;
        }

        if ((level == 1 || level == 2) &&
            (entry & VMM_PAGE_HUGE))
        {
            return true;
        }

        uint64_t physical_address =
            entry & VMM_PAGE_ADDRESS_MASK;

        table = (uint64_t *)vmm_physical_to_virtual(
            physical_address
        );

        if (table == NULL)
        {
            return false;
        }
    }

    return false;
}