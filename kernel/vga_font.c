#include "vga_font.h"
#include "io.h"

#include <stdint.h>

#define VGA_SEQUENCER_INDEX 0x3C4
#define VGA_SEQUENCER_DATA  0x3C5

#define VGA_GRAPHICS_INDEX  0x3CE
#define VGA_GRAPHICS_DATA   0x3CF

#define VGA_FONT_MEMORY ((volatile uint8_t *)0xA0000)

#define VGA_FONT_COUNT 256
#define VGA_FONT_HEIGHT 16
#define VGA_FONT_STRIDE 32

extern const uint8_t vga_font_start[];
extern const uint8_t vga_font_end[];

/**
 * @brief Writes a value to a VGA sequencer register.
 *
 * @param index The sequencer register index.
 * @param value The value to write.
 */
static void vga_sequencer_write(uint8_t index, uint8_t value)
{
    outb(VGA_SEQUENCER_INDEX, index);
    outb(VGA_SEQUENCER_DATA, value);
}

/**
 * @brief Writes a value to a VGA graphics controller register.
 *
 * @param index The graphics controller register index.
 * @param value The value to write.
 */
static void vga_graphics_write(uint8_t index, uint8_t value)
{
    outb(VGA_GRAPHICS_INDEX, index);
    outb(VGA_GRAPHICS_DATA, value);
}

void vga_font_initialize(void)
{
    const uint8_t saved_sequencer_2 = 0x03;
    const uint8_t saved_sequencer_4 = 0x03;

    const uint8_t saved_graphics_4 = 0x00;
    const uint8_t saved_graphics_5 = 0x10;
    const uint8_t saved_graphics_6 = 0x0E;

    // Select VGA font memory plane.
    vga_sequencer_write(0x02, 0x04);
    vga_sequencer_write(0x04, 0x07);

    vga_graphics_write(0x04, 0x02);
    vga_graphics_write(0x05, 0x00);
    vga_graphics_write(0x06, 0x04);

    // Copy glyphs into VGA font memory.
    for (uint16_t glyph = 0; glyph < VGA_FONT_COUNT; glyph++)
    {
        for (uint8_t row = 0; row < VGA_FONT_HEIGHT; row++)
        {
            VGA_FONT_MEMORY[glyph * VGA_FONT_STRIDE + row] =
                vga_font_start[glyph * VGA_FONT_HEIGHT + row];
        }
    }

    // Restore standard VGA text mode registers.
    vga_sequencer_write(0x02, saved_sequencer_2);
    vga_sequencer_write(0x04, saved_sequencer_4);

    vga_graphics_write(0x04, saved_graphics_4);
    vga_graphics_write(0x05, saved_graphics_5);
    vga_graphics_write(0x06, saved_graphics_6);
}