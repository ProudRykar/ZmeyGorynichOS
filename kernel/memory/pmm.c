#include "pmm.h"

#include <stddef.h>

static uint8_t *usable_bitmap =
    (uint8_t *)PMM_USABLE_BITMAP_ADDRESS;

static uint8_t *used_bitmap =
    (uint8_t *)PMM_USED_BITMAP_ADDRESS;

static pmm_stats_t pmm_stats;

static uint64_t pmm_last_page = PMM_RESERVED_MEMORY_END / PMM_PAGE_SIZE;

static bool pmm_bitmap_get(const uint8_t *bitmap, uint64_t index)
{
    return (bitmap[index / 8] & (1U << (index % 8))) != 0;
}

static void pmm_bitmap_set(uint8_t *bitmap, uint64_t index)
{
    bitmap[index / 8] |= (uint8_t)(1U << (index % 8));
}

static void pmm_bitmap_clear(uint8_t *bitmap, uint64_t index)
{
    bitmap[index / 8] &= (uint8_t)~(1U << (index % 8));
}

static void pmm_mark_usable(uint64_t start, uint64_t end)
{
    uint64_t first_page =
        (start + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;

    uint64_t last_page = end / PMM_PAGE_SIZE;

    if (last_page > PMM_MAX_PAGES)
    {
        last_page = PMM_MAX_PAGES;
    }

    for (uint64_t page = first_page; page < last_page; page++)
    {
        pmm_bitmap_set(usable_bitmap, page);
        pmm_bitmap_clear(used_bitmap, page);
    }
}

void pmm_initialize(const boot_info_t *boot_info)
{
    if (boot_info == NULL)
    {
        return;
    }

    for (uint64_t i = 0; i < PMM_BITMAP_SIZE; i++)
    {
        usable_bitmap[i] = 0;
        used_bitmap[i] = 0xFF;
    }

    pmm_stats.total_pages = PMM_MAX_PAGES;
    pmm_stats.used_pages = 0;
    pmm_stats.free_pages = 0;

    pmm_last_page = PMM_RESERVED_MEMORY_END / PMM_PAGE_SIZE;

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

        if (entry->length == 0 || entry->base >= PMM_MAX_MEMORY)
        {
            continue;
        }

        uint64_t end = entry->base + entry->length;

        if (end < entry->base || end > PMM_MAX_MEMORY)
        {
            end = PMM_MAX_MEMORY;
        }

        pmm_mark_usable(entry->base, end);
    }

    /*
     * Reserve the first MiB of physical memory.
     *
     * This region contains:
     * - BIOS data and real-mode memory;
     * - MBR and Stage 2;
     * - Kernel image;
     * - Boot information;
     * - Initial stack;
     * - Page tables;
     * - PMM bitmaps;
     * - VGA memory.
     */

    const uint64_t reserved_pages =
        PMM_RESERVED_MEMORY_END / PMM_PAGE_SIZE;

    for (uint64_t page = 0; page < reserved_pages; page++)
    {
        pmm_bitmap_set(used_bitmap, page);
    }

    for (uint64_t page = 0; page < PMM_MAX_PAGES; page++)
    {
        if (!pmm_bitmap_get(usable_bitmap, page))
        {
            continue;
        }

        if (pmm_bitmap_get(used_bitmap, page))
        {
            pmm_stats.used_pages++;
        }
        else
        {
            pmm_stats.free_pages++;
        }
    }
}

uint64_t pmm_allocate_page(void)
{
    if (pmm_stats.free_pages == 0)
    {
        return 0;
    }

    for (uint64_t offset = 0; offset < PMM_MAX_PAGES; offset++)
    {
        uint64_t page = (pmm_last_page + offset) % PMM_MAX_PAGES;

        if (!pmm_bitmap_get(usable_bitmap, page))
        {
            continue;
        }

        if (pmm_bitmap_get(used_bitmap, page))
        {
            continue;
        }

        pmm_bitmap_set(used_bitmap, page);

        pmm_stats.free_pages--;
        pmm_stats.used_pages++;

        pmm_last_page = page + 1;

        return page * PMM_PAGE_SIZE;
    }

    return 0;
}

bool pmm_free_page(uint64_t address)
{
    if (address % PMM_PAGE_SIZE != 0)
    {
        return false;
    }

    if (address < PMM_RESERVED_MEMORY_END ||
        address >= PMM_MAX_MEMORY)
    {
        return false;
    }

    uint64_t page = address / PMM_PAGE_SIZE;

    if (!pmm_bitmap_get(usable_bitmap, page))
    {
        return false;
    }

    if (!pmm_bitmap_get(used_bitmap, page))
    {
        return false;
    }

    pmm_bitmap_clear(used_bitmap, page);

    pmm_stats.free_pages++;
    pmm_stats.used_pages--;

    if (page < pmm_last_page)
    {
        pmm_last_page = page;
    }

    return true;
}

void pmm_get_stats(pmm_stats_t *stats)
{
    if (stats == NULL)
    {
        return;
    }

    *stats = pmm_stats;
}