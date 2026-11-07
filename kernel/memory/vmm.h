#ifndef KERNEL_VMM_H
#define KERNEL_VMM_H

#include <stdbool.h>
#include <stdint.h>
#include "boot_info.h"
#define VMM_PAGE_SIZE 4096ULL

#define VMM_PAGE_PRESENT (1ULL << 0)
#define VMM_PAGE_WRITE   (1ULL << 1)
#define VMM_PAGE_USER    (1ULL << 2)

#define VMM_DIRECT_MAP_BASE 0xFFFF800000000000ULL

#define VMM_PHYSICAL_TO_VIRTUAL(address) \
    (VMM_DIRECT_MAP_BASE + (uint64_t)(address))

bool vmm_initialize(const boot_info_t *boot_info);

bool vmm_map_page(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags
);

bool vmm_unmap_page(
    uint64_t virtual_address,
    uint64_t *physical_address
);

uint64_t vmm_get_physical_address(uint64_t virtual_address);

uint64_t vmm_get_current_pml4_physical(void);
bool vmm_load_pml4(uint64_t physical_address);

uint64_t *vmm_get_current_pml4(void);

bool vmm_is_user_accessible(uint64_t virtual_address);
#endif