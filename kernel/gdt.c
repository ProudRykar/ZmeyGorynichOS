#include "gdt.h"

#include <stddef.h>
#include <stdint.h>

#define GDT_ENTRIES 9
#define KERNEL_STACK_SIZE (16 * 1024)

#define GDT_TSS_ACCESS 0x89

typedef struct __attribute__((packed))
{
    uint32_t reserved0;

    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;

    uint64_t reserved1;

    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;

    uint64_t reserved2;

    uint16_t reserved3;
    uint16_t iomap_base;

} tss_t;

typedef struct __attribute__((packed))
{
    uint16_t limit;
    uint64_t base;

} gdt_register_t;

static uint64_t gdt[GDT_ENTRIES];

static tss_t tss;

static uint8_t kernel_stack[KERNEL_STACK_SIZE]
    __attribute__((aligned(16)));

extern void gdt_load(const gdt_register_t *register_pointer);

static void gdt_set_tss_descriptor(
    uint64_t base,
    uint32_t limit
)
{
    const uint64_t low =
        ((uint64_t)(limit & 0xFFFF)) |
        ((base & 0xFFFFFF) << 16) |
        ((uint64_t)GDT_TSS_ACCESS << 40) |
        ((uint64_t)((limit >> 16) & 0x0F) << 48) |
        ((base & 0xFF000000) << 32);

    const uint64_t high = base >> 32;

    gdt[7] = low;
    gdt[8] = high;
}

void gdt_initialize(void)
{
    /*
     * Preserve the selectors established by Stage2.
     *
     * 0x08 - 32-bit kernel code
     * 0x10 - kernel data
     * 0x18 - 64-bit kernel code
     * 0x20 - kernel data
     */

    gdt[0] = 0x0000000000000000ULL;
    gdt[1] = 0x00CF9A000000FFFFULL;
    gdt[2] = 0x00CF92000000FFFFULL;
    gdt[3] = 0x00AF9A000000FFFFULL;
    gdt[4] = 0x00CF92000000FFFFULL;

    /*
     * Ring 3 segments.
     *
     * 0x2B - user data (index 5, RPL 3)
     * 0x33 - user code (index 6, RPL 3)
     */

    gdt[5] = 0x00CFF2000000FFFFULL;
    gdt[6] = 0x00AFFA000000FFFFULL;

    /*
     * Initialize the Task State Segment.
     */

    tss.rsp0 = (uint64_t)(kernel_stack + sizeof(kernel_stack));

    tss.iomap_base = sizeof(tss_t);

    gdt_set_tss_descriptor(
        (uint64_t)&tss,
        sizeof(tss_t) - 1
    );

    const gdt_register_t gdtr =
    {
        .limit = sizeof(gdt) - 1,
        .base = (uint64_t)gdt
    };

    /*
     * Load the new GDT and reload segment registers.
     */

    gdt_load(&gdtr);

    /*
     * Load the Task Register.
     */

    const uint16_t tss_selector = GDT_TSS_SELECTOR;

    __asm__ volatile (
        "ltr %0"
        :
        : "r"(tss_selector)
        : "memory"
    );
}

uint64_t gdt_get_kernel_stack(void)
{
    return tss.rsp0;
}