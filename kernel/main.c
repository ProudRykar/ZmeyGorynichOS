#include "terminal.h"
#include "serial.h"
#include "shell.h"
#include "boot_info.h"
#include "boot_info_print.h"

#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/heap.h"

#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"

#include "process/process_manager.h"
#include "debug/tests.h"

static void kernel_halt(void)
{
    for (;;)
    {
        __asm__ volatile ("cli; hlt");
    }
}

void kernel_main(const boot_info_t *boot_info)
{
    serial_initialize();
    terminal_initialize();

    boot_info_print(boot_info);

    terminal_write("Змий Горыныч проснулся");
    terminal_write("Зодчество: x86_64");
    terminal_write("Сердце: Бьётся");

    // Validate boot information.
    if (boot_info == 0 ||
        boot_info->magic != BOOT_INFO_MAGIC ||
        boot_info->version != BOOT_INFO_VERSION ||
        boot_info->memory_map_count == 0 ||
        boot_info->memory_map_count > MEMORY_MAP_MAX_ENTRIES ||
        boot_info->memory_map_entry_size != sizeof(memory_map_entry_t))
    {
        terminal_write("Ошибка: карта памяти повреждена.");
        kernel_halt();
    }

    // Initialize memory management.
    pmm_initialize(boot_info);

    if (!vmm_initialize(boot_info))
    {
        terminal_write("VMM initialization: FAILED");
        kernel_halt();
    }

    if (!heap_initialize())
    {
        terminal_write("Heap initialization: FAILED");
        kernel_halt();
    }

    // Initialize CPU structures.
    gdt_initialize();

    // Initialize interrupt handling.
    idt_initialize();
    pic_initialize();

    // Initialize hardware devices.
    pit_initialize();
    keyboard_initialize();

    // Initialize process management.
    uint64_t kernel_pml4 =
        vmm_get_current_pml4_physical();

    if (!process_manager_initialize(kernel_pml4))
    {
        terminal_write("Process Manager: FAILED");
        kernel_halt();
    }

    // Run kernel debugging tests.
    debug_run_memory_tests();
    debug_run_process_tests();
    debug_run_ring3_tests();

    // Start the shell.
    shell_initialize();
    shell_run();

    kernel_halt();
}