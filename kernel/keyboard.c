#include "keyboard.h"
#include "io.h"
#include <stddef.h>
#include <stdbool.h>

#define KEYBOARD_DATA_PORT 0x60

#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_COMMAND_PORT 0x64

#define KEYBOARD_CONTROLLER_ENABLE 0xAE
#define KEYBOARD_CONTROLLER_READ_CONFIG 0x20
#define KEYBOARD_CONTROLLER_WRITE_CONFIG 0x60

#define KEYBOARD_ENABLE_SCANNING 0xF4
#define KEYBOARD_ACK 0xFA

static bool russian_layout = false;
static bool caps_lock_pressed = false;

static bool shift_pressed = false;

#define KEYBOARD_BUFFER_SIZE 128
#define KEYBOARD_BUFFER_MASK (KEYBOARD_BUFFER_SIZE - 1)

static uint16_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static bool extended_scancode = false;
static volatile uint8_t keyboard_buffer_head = 0;
static volatile uint8_t keyboard_buffer_tail = 0;

static void keyboard_buffer_push(uint16_t key)
{
    const uint8_t next_head =
        (keyboard_buffer_head + 1) & KEYBOARD_BUFFER_MASK;

    // Drop the key if the buffer is full.
    if (next_head == keyboard_buffer_tail)
    {
        return;
    }

    keyboard_buffer[keyboard_buffer_head] = key;
    keyboard_buffer_head = next_head;
}

bool keyboard_read_key(uint16_t *key)
{
    if (key == NULL)
    {
        return false;
    }

    if (keyboard_buffer_head == keyboard_buffer_tail)
    {
        return false;
    }

    *key = keyboard_buffer[keyboard_buffer_tail];

    keyboard_buffer_tail =
        (keyboard_buffer_tail + 1) & KEYBOARD_BUFFER_MASK;

    return true;
}

static const char scancode_map[128] =
{
    [0x02] = '1',
    [0x03] = '2',
    [0x04] = '3',
    [0x05] = '4',
    [0x06] = '5',
    [0x07] = '6',
    [0x08] = '7',
    [0x09] = '8',
    [0x0A] = '9',
    [0x0B] = '0',
    [0x0C] = '-',
    [0x0D] = '=',

    [0x0E] = '\b',
    [0x0F] = '\t',

    [0x10] = 'q',
    [0x11] = 'w',
    [0x12] = 'e',
    [0x13] = 'r',
    [0x14] = 't',
    [0x15] = 'y',
    [0x16] = 'u',
    [0x17] = 'i',
    [0x18] = 'o',
    [0x19] = 'p',

    [0x1A] = '[',
    [0x1B] = ']',
    [0x1C] = '\n',

    [0x1E] = 'a',
    [0x1F] = 's',
    [0x20] = 'd',
    [0x21] = 'f',
    [0x22] = 'g',
    [0x23] = 'h',
    [0x24] = 'j',
    [0x25] = 'k',
    [0x26] = 'l',

    [0x27] = ';',
    [0x28] = '\'',
    [0x29] = '`',

    [0x2B] = '\\',

    [0x2C] = 'z',
    [0x2D] = 'x',
    [0x2E] = 'c',
    [0x2F] = 'v',
    [0x30] = 'b',
    [0x31] = 'n',
    [0x32] = 'm',

    [0x33] = ',',
    [0x34] = '.',
    [0x35] = '/',

    [0x39] = ' '
};

static const char scancode_map_shift[128] =
{
    [0x02] = '!',
    [0x03] = '@',
    [0x04] = '#',
    [0x05] = '$',
    [0x06] = '%',
    [0x07] = '^',
    [0x08] = '&',
    [0x09] = '*',
    [0x0A] = '(',
    [0x0B] = ')',
    [0x0C] = '_',
    [0x0D] = '+',

    [0x0E] = '\b',
    [0x0F] = '\t',

    [0x10] = 'Q',
    [0x11] = 'W',
    [0x12] = 'E',
    [0x13] = 'R',
    [0x14] = 'T',
    [0x15] = 'Y',
    [0x16] = 'U',
    [0x17] = 'I',
    [0x18] = 'O',
    [0x19] = 'P',

    [0x1A] = '{',
    [0x1B] = '}',
    [0x1C] = '\n',

    [0x1E] = 'A',
    [0x1F] = 'S',
    [0x20] = 'D',
    [0x21] = 'F',
    [0x22] = 'G',
    [0x23] = 'H',
    [0x24] = 'J',
    [0x25] = 'K',
    [0x26] = 'L',

    [0x27] = ':',
    [0x28] = '"',
    [0x29] = '~',

    [0x2B] = '|',

    [0x2C] = 'Z',
    [0x2D] = 'X',
    [0x2E] = 'C',
    [0x2F] = 'V',
    [0x30] = 'B',
    [0x31] = 'N',
    [0x32] = 'M',

    [0x33] = '<',
    [0x34] = '>',
    [0x35] = '?',

    [0x39] = ' '
};

static const char scancode_map_ru[128] =
{
    [0x02] = '1',
    [0x03] = '2',
    [0x04] = '3',
    [0x05] = '4',
    [0x06] = '5',
    [0x07] = '6',
    [0x08] = '7',
    [0x09] = '8',
    [0x0A] = '9',
    [0x0B] = '0',
    [0x0C] = '-',
    [0x0D] = '=',

    [0x0E] = '\b',
    [0x0F] = '\t',

    [0x10] = (char)0xA9, // й
    [0x11] = (char)0xE6, // ц
    [0x12] = (char)0xE3, // у
    [0x13] = (char)0xAA, // к
    [0x14] = (char)0xA5, // е
    [0x15] = (char)0xAD, // н
    [0x16] = (char)0xA3, // г
    [0x17] = (char)0xE8, // ш
    [0x18] = (char)0xE9, // щ
    [0x19] = (char)0xA7, // з
    [0x1A] = (char)0xE5, // х
    [0x1B] = (char)0xEA, // ъ

    [0x1C] = '\n',

    [0x1E] = (char)0xE4, // ф
    [0x1F] = (char)0xEB, // ы
    [0x20] = (char)0xA2, // в
    [0x21] = (char)0xA0, // а
    [0x22] = (char)0xAF, // п
    [0x23] = (char)0xE0, // р
    [0x24] = (char)0xAE, // о
    [0x25] = (char)0xAB, // л
    [0x26] = (char)0xA4, // д
    [0x27] = (char)0xA6, // ж
    [0x28] = (char)0xED, // э

    [0x2B] = '\\',

    [0x2C] = (char)0xEF, // я
    [0x2D] = (char)0xE7, // ч
    [0x2E] = (char)0xE1, // с
    [0x2F] = (char)0xAC, // м
    [0x30] = (char)0xA8, // и
    [0x31] = (char)0xE2, // т
    [0x32] = (char)0xEC, // ь
    [0x33] = (char)0xA1, // б
    [0x34] = (char)0xEE, // ю
    [0x35] = '.',

    [0x39] = ' '
};

static const char scancode_map_ru_shift[128] =
{
    [0x02] = '!',
    [0x03] = '"',
    [0x04] = '#',
    [0x05] = ';',
    [0x06] = '%',
    [0x07] = ':',
    [0x08] = '?',
    [0x09] = '*',
    [0x0A] = '(',
    [0x0B] = ')',
    [0x0C] = '_',
    [0x0D] = '+',

    [0x0E] = '\b',
    [0x0F] = '\t',

    [0x10] = (char)0x89, // Й
    [0x11] = (char)0x96, // Ц
    [0x12] = (char)0x93, // У
    [0x13] = (char)0x8A, // К
    [0x14] = (char)0x85, // Е
    [0x15] = (char)0x8D, // Н
    [0x16] = (char)0x83, // Г
    [0x17] = (char)0x98, // Ш
    [0x18] = (char)0x99, // Щ
    [0x19] = (char)0x87, // З
    [0x1A] = (char)0x95, // Х
    [0x1B] = (char)0x9A, // Ъ

    [0x1C] = '\n',

    [0x1E] = (char)0x94, // Ф
    [0x1F] = (char)0x9B, // Ы
    [0x20] = (char)0x82, // В
    [0x21] = (char)0x80, // А
    [0x22] = (char)0x8F, // П
    [0x23] = (char)0x90, // Р
    [0x24] = (char)0x8E, // О
    [0x25] = (char)0x8B, // Л
    [0x26] = (char)0x84, // Д
    [0x27] = (char)0x86, // Ж
    [0x28] = (char)0x9D, // Э

    [0x2B] = '|',

    [0x2C] = (char)0x9F, // Я
    [0x2D] = (char)0x97, // Ч
    [0x2E] = (char)0x91, // С
    [0x2F] = (char)0x8C, // М
    [0x30] = (char)0x88, // И
    [0x31] = (char)0x92, // Т
    [0x32] = (char)0x9C, // Ь
    [0x33] = (char)0x81, // Б
    [0x34] = (char)0x9E, // Ю
    [0x35] = ',',

    [0x39] = ' '
};

/**
 * @brief Waits until the keyboard controller input buffer becomes empty.
 *
 * @return True if the input buffer became empty, false on timeout.
 */
static bool keyboard_wait_input_empty(void)
{
    for (uint32_t i = 0; i < 100000; i++)
    {
        if ((inb(KEYBOARD_STATUS_PORT) & 0x02) == 0)
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief Waits until the keyboard controller output buffer contains data.
 *
 * @return True if data became available, false on timeout.
 */
static bool keyboard_wait_output_full(void)
{
    for (uint32_t i = 0; i < 100000; i++)
    {
        if ((inb(KEYBOARD_STATUS_PORT) & 0x01) != 0)
        {
            return true;
        }
    }

    return false;
}

void keyboard_initialize(void)
{
    extended_scancode = false;
    shift_pressed = false;
    keyboard_buffer_head = 0;
    keyboard_buffer_tail = 0;

    // Enable the first PS/2 port.
    if (!keyboard_wait_input_empty())
    {
        return;
    }

    outb(KEYBOARD_COMMAND_PORT, KEYBOARD_CONTROLLER_ENABLE);

    // Read the keyboard controller configuration byte.
    if (!keyboard_wait_input_empty())
    {
        return;
    }

    outb(KEYBOARD_COMMAND_PORT, KEYBOARD_CONTROLLER_READ_CONFIG);

    if (!keyboard_wait_output_full())
    {
        return;
    }

    uint8_t config = inb(KEYBOARD_DATA_PORT);

    // Enable keyboard interrupts.
    config |= 0x01;

    // Enable scancode set 1 translation.
    config |= 0x40;

    // Enable the first PS/2 port clock.
    config &= ~(1 << 4);

    // Write the updated configuration.
    if (!keyboard_wait_input_empty())
    {
        return;
    }

    outb(KEYBOARD_COMMAND_PORT, KEYBOARD_CONTROLLER_WRITE_CONFIG);

    if (!keyboard_wait_input_empty())
    {
        return;
    }

    outb(KEYBOARD_DATA_PORT, config);

    // Enable keyboard scanning.
    if (!keyboard_wait_input_empty())
    {
        return;
    }

    outb(KEYBOARD_DATA_PORT, KEYBOARD_ENABLE_SCANNING);

    // Consume the keyboard acknowledgement.
    if (keyboard_wait_output_full())
    {
        const uint8_t response = inb(KEYBOARD_DATA_PORT);

        if (response != KEYBOARD_ACK)
        {
            return;
        }
    }


}

void keyboard_handle_interrupt(void)
{
    const uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Handle extended scancode prefix.
    if (scancode == 0xE0)
    {
        extended_scancode = true;
        return;
    }

    // Handle extended keys.
    if (extended_scancode)
    {
        extended_scancode = false;

        const bool released = (scancode & 0x80) != 0;
        const uint8_t keycode = scancode & 0x7F;

        if (released)
        {
            return;
        }

        switch (keycode)
        {
            case 0x47:
                keyboard_buffer_push(KEYBOARD_KEY_HOME);
                break;

            case 0x48:
                keyboard_buffer_push(KEYBOARD_KEY_UP);
                break;

            case 0x4B:
                keyboard_buffer_push(KEYBOARD_KEY_LEFT);
                break;

            case 0x4D:
                keyboard_buffer_push(KEYBOARD_KEY_RIGHT);
                break;

            case 0x4F:
                keyboard_buffer_push(KEYBOARD_KEY_END);
                break;

            case 0x50:
                keyboard_buffer_push(KEYBOARD_KEY_DOWN);
                break;

            case 0x53:
                keyboard_buffer_push(KEYBOARD_KEY_DELETE);
                break;
        }

        return;
    }

    const bool released = (scancode & 0x80) != 0;
    const uint8_t keycode = scancode & 0x7F;

    // Handle left and right Shift.
    if (keycode == 0x2A || keycode == 0x36)
    {
        shift_pressed = !released;
        return;
    }

    // Handle Caps Lock as a keyboard layout switch.
    if (keycode == 0x3A)
    {
        if (!released && !caps_lock_pressed)
        {
            russian_layout = !russian_layout;
            caps_lock_pressed = true;
        }
        else if (released)
        {
            caps_lock_pressed = false;
        }

        return;
    }

    // Ignore key release events.
    if (released)
    {
        return;
    }

    const char character = russian_layout
        ? (shift_pressed
            ? scancode_map_ru_shift[keycode]
            : scancode_map_ru[keycode])
        : (shift_pressed
            ? scancode_map_shift[keycode]
            : scancode_map[keycode]);

    if (character != '\0')
    {
        keyboard_buffer_push((uint8_t)character);
    }
}