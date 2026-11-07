#include "shell.h"
#include "keyboard.h"
#include "terminal.h"
#include "pit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SHELL_INPUT_SIZE 128

#define SHELL_PROMPT "Царь Государь"

static char input_buffer[SHELL_INPUT_SIZE];
static size_t input_length = 0;

static size_t input_start_row = 0;
static size_t input_start_column = 0;

static size_t input_cursor = 0;
static size_t input_display_length = 0;

#define SHELL_HISTORY_SIZE 16

static char command_history[SHELL_HISTORY_SIZE][SHELL_INPUT_SIZE];
static size_t history_head = 0;
static size_t history_count = 0;
static int history_position = -1;

static void shell_redraw_input(void);

static char shell_to_lower(char character)
{
    uint8_t value = (uint8_t)character;

    // Кириллические буквы А-П в CP866.
    if (value >= 0x80 && value <= 0x8F)
    {
        return (char)(value + 0x20);
    }

    // Кириллические буквы Р-Я в CP866.
    if (value >= 0x90 && value <= 0x9F)
    {
        return (char)(value + 0x50);
    }

    // Буква Ё в CP866.
    if (value == 0xF0)
    {
        return (char)0xF1;
    }

    return character;
}

static bool shell_string_equals(const char *left, const char *right)
{
    bool first_character = true;

    while (*left != '\0' && *right != '\0')
    {
        char left_character = *left;
        char right_character = *right;

        if (first_character)
        {
            left_character = shell_to_lower(left_character);
            right_character = shell_to_lower(right_character);
            first_character = false;
        }

        if (left_character != right_character)
        {
            return false;
        }

        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}


static bool shell_string_starts_with(const char *string, const char *prefix)
{
    bool first_character = true;

    while (*prefix != '\0')
    {
        char string_character = *string;
        char prefix_character = *prefix;

        if (first_character)
        {
            string_character = shell_to_lower(string_character);
            prefix_character = shell_to_lower(prefix_character);
            first_character = false;
        }

        if (string_character != prefix_character)
        {
            return false;
        }

        string++;
        prefix++;
    }

    return true;
}

static char *shell_trim(char *string)
{
    while (*string == ' ' || *string == '\t')
    {
        string++;
    }

    size_t length = 0;

    while (string[length] != '\0')
    {
        length++;
    }

    while (length > 0 &&
           (string[length - 1] == ' ' || string[length - 1] == '\t'))
    {
        string[--length] = '\0';
    }

    return string;
}

static void shell_history_save(const char *command)
{
    if (command[0] == '\0')
    {
        return;
    }

    size_t length = 0;

    while (command[length] != '\0' &&
           length < SHELL_INPUT_SIZE - 1)
    {
        command_history[history_head][length] = command[length];
        length++;
    }

    command_history[history_head][length] = '\0';

    history_head = (history_head + 1) % SHELL_HISTORY_SIZE;

    if (history_count < SHELL_HISTORY_SIZE)
    {
        history_count++;
    }
}

static void shell_history_replace(const char *command)
{
    size_t length = 0;

    while (command[length] != '\0' &&
           length < SHELL_INPUT_SIZE - 1)
    {
        input_buffer[length] = command[length];
        length++;
    }

    input_length = length;
    input_cursor = length;
    input_buffer[input_length] = '\0';

    shell_redraw_input();
}

static void shell_history_up(void)
{
    if (history_count == 0)
    {
        return;
    }

    if (history_position + 1 >= (int)history_count)
    {
        return;
    }

    history_position++;

    const size_t index =
        (history_head + SHELL_HISTORY_SIZE - 1 - history_position)
        % SHELL_HISTORY_SIZE;

    shell_history_replace(command_history[index]);
}

static void shell_history_down(void)
{
    if (history_position < 0)
    {
        return;
    }

    if (history_position == 0)
    {
        history_position = -1;
        shell_history_replace("");
        return;
    }

    history_position--;

    const size_t index =
        (history_head + SHELL_HISTORY_SIZE - 1 - history_position)
        % SHELL_HISTORY_SIZE;

    shell_history_replace(command_history[index]);
}

static void shell_write_string(const char *string)
{
    while (*string != '\0')
    {
        terminal_write_char(*string++);
    }
}

static void shell_print_prompt(void)
{
    terminal_write_char('[');
    shell_write_string(SHELL_PROMPT);
    terminal_write_char(']');
    terminal_write_char(' ');
    terminal_write_char('>');
    terminal_write_char(' ');

    terminal_get_cursor(&input_start_row, &input_start_column);
}

static void shell_update_cursor(void)
{
    size_t absolute_column = input_start_column + input_cursor;

    size_t row = input_start_row + absolute_column / 80;
    size_t column = absolute_column % 80;

    terminal_set_cursor(row, column);
}

static void shell_redraw_input(void)
{
    terminal_set_cursor(input_start_row, input_start_column);

    for (size_t i = 0; i < input_display_length; i++)
    {
        terminal_write_char(' ');
    }

    terminal_set_cursor(input_start_row, input_start_column);

    for (size_t i = 0; i < input_length; i++)
    {
        terminal_write_char(input_buffer[i]);
    }

    input_display_length = input_length;

    shell_update_cursor();
}

static void shell_insert_character(char character)
{
    if (input_length >= SHELL_INPUT_SIZE - 1)
    {
        return;
    }

    for (size_t i = input_length; i > input_cursor; i--)
    {
        input_buffer[i] = input_buffer[i - 1];
    }

    input_buffer[input_cursor] = character;

    input_length++;
    input_cursor++;

    input_buffer[input_length] = '\0';

    shell_redraw_input();
}

static void shell_delete_before_cursor(void)
{
    if (input_cursor == 0)
    {
        return;
    }

    for (size_t i = input_cursor - 1; i < input_length - 1; i++)
    {
        input_buffer[i] = input_buffer[i + 1];
    }

    input_length--;
    input_cursor--;

    input_buffer[input_length] = '\0';

    shell_redraw_input();
}

static void shell_delete_at_cursor(void)
{
    if (input_cursor >= input_length)
    {
        return;
    }

    for (size_t i = input_cursor; i < input_length - 1; i++)
    {
        input_buffer[i] = input_buffer[i + 1];
    }

    input_length--;
    input_buffer[input_length] = '\0';

    shell_redraw_input();
}

static void shell_print_help(void)
{
    terminal_write("Повеления:");
    terminal_write("Грамота - перечень повелений");
    terminal_write("Очисти - очистить экран");
    terminal_write("Молви - изречь слово");
    terminal_write("Сведомь - сведения о змие");
    terminal_write("Удары - число толчков");
    terminal_write("Почить - упокоить змия");
}

static void shell_print_info(void)
{
    terminal_write("ZmeyGorynichOS");
    terminal_write("Зодчество: x86_64");
    terminal_write("Сердце: Бьётся");
}

static void shell_print_ticks(void)
{
    terminal_write_hex("Биения", pit_get_ticks());
}

static void shell_execute_command(char *command)
{
    command = shell_trim(command);

    if (command[0] == '\0')
    {
        return;
    }

    if (shell_string_equals(command, "Грамота"))
    {
        shell_print_help();
    }
    else if (shell_string_equals(command, "Очисти"))
    {
        terminal_clear();
    }
    else if (shell_string_starts_with(command, "Молви") &&
             (command[5] == ' ' || command[5] == '\0'))
    {
        const char *argument = command + 5;

        while (*argument == ' ')
        {
            argument++;
        }

        if (*argument == '\0')
        {
            terminal_write("Что изречь, государь?");
        }
        else
        {
            terminal_write(argument);
        }
    }
    else if (shell_string_equals(command, "Сведомь"))
    {
        shell_print_info();
    }
    else if (shell_string_equals(command, "Удары"))
    {
        shell_print_ticks();
    }
    else if (shell_string_equals(command, "Почить"))
    {
        terminal_write("Змий уснул.");

        __asm__ volatile ("cli");

        for (;;)
        {
            __asm__ volatile ("hlt");
        }
    }
    else
    {
        terminal_write("Слово неведомо.");
    }
}

static void shell_handle_character(char character)
{
    if (character == '\n')
    {
        input_cursor = input_length;
        shell_update_cursor();

        terminal_write_char('\n');

        input_buffer[input_length] = '\0';

        char *command = shell_trim(input_buffer);

        shell_history_save(command);
        shell_execute_command(command);

        input_length = 0;
        input_cursor = 0;
        input_display_length = 0;
        history_position = -1;

        shell_print_prompt();

        return;
    }

    if (character == '\b')
    {
        shell_delete_before_cursor();
        history_position = -1;
        return;
    }

    if (character == '\t')
    {
        return;
    }

    history_position = -1;

    shell_insert_character(character);
}

void shell_initialize(void)
{
    input_length = 0;
    input_buffer[0] = '\0';
    input_cursor = 0;
    input_display_length = 0;
    history_position = -1;

    terminal_write("Береста Змия Горыныча");
    terminal_write("Введите 'Грамота' дабы узреть перечень повелений.");

    shell_print_prompt();
}

void shell_run(void)
{
    for (;;)
    {
        uint16_t key;

        /*
         * Disable interrupts before checking the queue.
         * This prevents a keyboard interrupt from arriving
         * between the empty check and the HLT instruction.
         */
        __asm__ volatile ("cli");

        if (!keyboard_read_key(&key))
        {
            /*
             * STI enables interrupts after the next instruction.
             * HLT therefore executes with interrupts enabled.
             */
            __asm__ volatile ("sti; hlt");
            continue;
        }

        __asm__ volatile ("sti");

        switch (key)
        {
            case KEYBOARD_KEY_UP:
                shell_history_up();
                break;

            case KEYBOARD_KEY_DOWN:
                shell_history_down();
                break;

            case KEYBOARD_KEY_LEFT:
                if (input_cursor > 0)
                {
                    input_cursor--;
                    shell_update_cursor();
                }
                break;

            case KEYBOARD_KEY_RIGHT:
                if (input_cursor < input_length)
                {
                    input_cursor++;
                    shell_update_cursor();
                }
                break;

            case KEYBOARD_KEY_HOME:
                input_cursor = 0;
                shell_update_cursor();
                break;

            case KEYBOARD_KEY_END:
                input_cursor = input_length;
                shell_update_cursor();
                break;

            case KEYBOARD_KEY_DELETE:
                shell_delete_at_cursor();
                history_position = -1;
                break;

            default:
                if (key <= 0xFF)
                {
                    shell_handle_character((char)key);
                }
                break;
        }
    }
}