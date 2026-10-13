#include "keyboard.h"

#define DATA_PORT   0x60
#define STATUS_PORT 0x64
#define COMMAND_PORT 0x64

static const char scancode_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' '
};

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0"
                      : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1"
                      : : "a"(value), "Nd"(port));
}

static int wait_input_clear(void)
{
    for (uint32_t i = 0; i < 100000; i++)
    {
        if ((inb(STATUS_PORT) & 0x02) == 0)
            return 0;
    }

    return -1;
}

static int wait_output_full(void)
{
    for (uint32_t i = 0; i < 100000; i++)
    {
        if (inb(STATUS_PORT) & 0x01)
            return 0;
    }

    return -1;
}

static int controller_command(uint8_t command)
{
    if (wait_input_clear() != 0)
        return -1;

    outb(COMMAND_PORT, command);
    return 0;
}

static int controller_data(uint8_t value)
{
    if (wait_input_clear() != 0)
        return -1;

    outb(DATA_PORT, value);
    return 0;
}

void keyboard_init(void)
{
    /* Disable both ports during setup. */
    controller_command(0xAD);
    controller_command(0xA7);

    /* Drain pending output. */
    for (uint32_t i = 0; i < 32; i++)
    {
        if ((inb(STATUS_PORT) & 1) == 0)
            break;

        (void)inb(DATA_PORT);
    }

    /* Read controller configuration. */
    if (controller_command(0x20) != 0)
        return;

    if (wait_output_full() != 0)
        return;

    uint8_t config = inb(DATA_PORT);

    /* Enable translation; disable keyboard IRQ for polling. */
    config |= 0x40;
    config &= (uint8_t)~0x01;

    if (controller_command(0x60) != 0)
        return;

    if (controller_data(config) != 0)
        return;

    /* Enable the first port. */
    controller_command(0xAE);
}

char keyboard_scancode_to_ascii(uint8_t scancode)
{
    if (scancode >= sizeof(scancode_map) ||
        (scancode & 0x80))
        return 0;

    return scancode_map[scancode];
}

char keyboard_read_char(void)
{
    while (1)
    {
        uint8_t status = inb(STATUS_PORT);

        if ((status & 0x01) == 0)
        {
            __asm__ volatile ("pause");
            continue;
        }

        /* Check the source before consuming the data byte. */
        if (status & 0x20)
        {
            (void)inb(DATA_PORT);
            continue;
        }

        uint8_t scancode = inb(DATA_PORT);

        /* Ignore key-release events. */
        if (scancode & 0x80)
        {
            continue;
        }

        char c = keyboard_scancode_to_ascii(scancode);

        if (c != 0)
        {
            return c;
        }
    }
}

uint8_t keyboard_read_scancode(void)
{
    while ((inb(STATUS_PORT) & 0x01) == 0)
    {
        __asm__ volatile ("pause");
    }

    return inb(DATA_PORT);
}
