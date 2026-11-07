#include "pit.h"
#include "io.h"

#define PIT_CHANNEL_0 0x40
#define PIT_COMMAND 0x43

#define PIT_BASE_FREQUENCY 1193182
#define PIT_FREQUENCY 100

static volatile uint64_t timer_ticks = 0;

void pit_initialize(void)
{
    const uint16_t divisor =
        PIT_BASE_FREQUENCY / PIT_FREQUENCY;

    // Channel 0, access mode: low/high byte, mode 3.
    outb(PIT_COMMAND, 0x36);

    // Send divisor to channel 0.
    outb(PIT_CHANNEL_0, divisor & 0xFF);
    outb(PIT_CHANNEL_0, (divisor >> 8) & 0xFF);
}

void pit_tick(void)
{
    timer_ticks++;
}

uint64_t pit_get_ticks(void)
{
    return timer_ticks;
}