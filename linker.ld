OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x10000;

    .text ALIGN(16) :
    {
        *(.text.entry)
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(16) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(16) :
    {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(16) (NOLOAD) :
    {
        __bss_start = .;

        *(COMMON)
        *(.bss)
        *(.bss.*)

        __bss_end = .;
    }

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.note*)
        *(.comment)
    }
}