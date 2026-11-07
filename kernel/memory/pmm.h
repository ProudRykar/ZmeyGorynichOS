#ifndef KERNEL_PMM_H
#define KERNEL_PMM_H

#include <stdbool.h>
#include <stdint.h>

#include "boot_info.h"

#define PMM_PAGE_SIZE 4096

#define PMM_MAX_MEMORY (4ULL * 1024 * 1024 * 1024)
#define PMM_MAX_PAGES (PMM_MAX_MEMORY / PMM_PAGE_SIZE)

#define PMM_BITMAP_SIZE (PMM_MAX_PAGES / 8)

#define PMM_USABLE_BITMAP_ADDRESS 0x30000
#define PMM_USED_BITMAP_ADDRESS 0x50000

#define PMM_RESERVED_MEMORY_END 0x100000

typedef struct
{
    uint64_t total_pages;
    uint64_t used_pages;
    uint64_t free_pages;
} pmm_stats_t;

void pmm_initialize(const boot_info_t *boot_info);

uint64_t pmm_allocate_page(void);
bool pmm_free_page(uint64_t address);

void pmm_get_stats(pmm_stats_t *stats);

#endif