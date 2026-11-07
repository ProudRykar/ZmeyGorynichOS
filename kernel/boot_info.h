#ifndef KERNEL_BOOT_INFO_H
#define KERNEL_BOOT_INFO_H

#include <stdint.h>

#define BOOT_INFO_MAGIC 0x474D5A59
#define BOOT_INFO_VERSION 1

#define MEMORY_MAP_MAX_ENTRIES 128

#define MEMORY_TYPE_AVAILABLE 1
#define MEMORY_TYPE_RESERVED 2
#define MEMORY_TYPE_ACPI_RECLAIMABLE 3
#define MEMORY_TYPE_ACPI_NVS 4
#define MEMORY_TYPE_BAD 5

typedef struct __attribute__((packed))
{
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
} memory_map_entry_t;

typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint32_t version;
    uint32_t memory_map_count;
    uint32_t memory_map_entry_size;

    memory_map_entry_t memory_map[];
} boot_info_t;

#endif