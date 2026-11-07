#include "boot_info_print.h"
#include "terminal.h"

#include <stdint.h>

static const char *boot_info_memory_type_name(uint32_t type)
{
    switch (type)
    {
        case MEMORY_TYPE_AVAILABLE:
            return "Available";

        case MEMORY_TYPE_RESERVED:
            return "Reserved";

        case MEMORY_TYPE_ACPI_RECLAIMABLE:
            return "ACPI Reclaimable";

        case MEMORY_TYPE_ACPI_NVS:
            return "ACPI NVS";

        case MEMORY_TYPE_BAD:
            return "Bad Memory";

        default:
            return "Unknown";
    }
}

void boot_info_print(const boot_info_t *boot_info)
{
    if (boot_info == NULL)
    {
        terminal_write("Boot info: missing");
        return;
    }

    terminal_write("");
    terminal_write("=== Boot Information ===");

    terminal_write_hex("Magic", boot_info->magic);
    terminal_write_hex("Version", boot_info->version);
    terminal_write_hex("Memory map entries", boot_info->memory_map_count);
    terminal_write_hex("Entry size", boot_info->memory_map_entry_size);

    if (boot_info->magic != BOOT_INFO_MAGIC)
    {
        terminal_write("Invalid boot info magic!");
        return;
    }

    if (boot_info->version != BOOT_INFO_VERSION)
    {
        terminal_write("Unsupported boot info version!");
        return;
    }

    if (boot_info->memory_map_count > MEMORY_MAP_MAX_ENTRIES)
    {
        terminal_write("Invalid memory map count!");
        return;
    }

    if (boot_info->memory_map_entry_size != sizeof(memory_map_entry_t))
    {
        terminal_write("Invalid memory map entry size!");
        return;
    }

    uint64_t total_available_memory = 0;

    terminal_write("");
    terminal_write("=== Memory Map ===");

    for (uint32_t i = 0; i < boot_info->memory_map_count; i++)
    {
        const memory_map_entry_t *entry = &boot_info->memory_map[i];

        if (entry->type == MEMORY_TYPE_AVAILABLE)
        {
            total_available_memory += entry->length;
        }

        terminal_write("");
        terminal_write_hex("Entry", i);

        terminal_write("Type:");
        terminal_write(boot_info_memory_type_name(entry->type));

        terminal_write_hex("Base", entry->base);
        terminal_write_hex("Length", entry->length);
        terminal_write_hex("Attributes", entry->attributes);
    }

    terminal_write("");
    terminal_write("=== Memory Summary ===");

    terminal_write_hex("Available memory (bytes)", total_available_memory);
    terminal_write_hex(
        "Available memory (MiB)",
        total_available_memory / (1024 * 1024)
    );

    terminal_write("");
    terminal_write("=== End Boot Information ===");
}