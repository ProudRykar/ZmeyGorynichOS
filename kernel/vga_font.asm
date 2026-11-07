section .rodata

global vga_font_start
global vga_font_end

vga_font_start:
    incbin "assets/cp866-8x16.bin"

vga_font_end: