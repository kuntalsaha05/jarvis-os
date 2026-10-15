#include "keyboard.h"
#include "interrupts/pic.h"

#define DATA_PORT   0x60
#define STATUS_PORT 0x64
#define COMMAND_PORT 0x64

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

#define KEYBOARD_BUFFER_SIZE 128

typedef struct {
    uint8_t buffer[KEYBOARD_BUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
} keyboard_queue_t;

static keyboard_queue_t kbd_queue = { {0}, 0, 0, 0 };
static volatile uint32_t keyboard_irq_count = 0;

int keyboard_enqueue(uint8_t scancode)
{
    if (kbd_queue.count >= KEYBOARD_BUFFER_SIZE)
        return -1;

    kbd_queue.buffer[kbd_queue.head] = scancode;
    kbd_queue.head = (kbd_queue.head + 1) % KEYBOARD_BUFFER_SIZE;
    __sync_fetch_and_add(&kbd_queue.count, 1);
    return 0;
}

int keyboard_dequeue(uint8_t *scancode)
{
    if (kbd_queue.count == 0)
        return -1;

    *scancode = kbd_queue.buffer[kbd_queue.tail];
    kbd_queue.tail = (kbd_queue.tail + 1) % KEYBOARD_BUFFER_SIZE;
    __sync_fetch_and_sub(&kbd_queue.count, 1);
    return 0;
}

uint32_t keyboard_get_irq_count(void)
{
    return keyboard_irq_count;
}

void keyboard_handler(void)
{
    keyboard_irq_count++;

    uint8_t status = inb(STATUS_PORT);
    if (status & 0x01)
    {
        uint8_t scancode = inb(DATA_PORT);
        /* If not mouse data, enqueue to event buffer */
        if ((status & 0x20) == 0)
        {
            keyboard_enqueue(scancode);
        }
    }

    pic_send_eoi(1);
}


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
    kbd_queue.head = 0;
    kbd_queue.tail = 0;
    kbd_queue.count = 0;
    keyboard_irq_count = 0;

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
    if (controller_command(0x20) == 0 && wait_output_full() == 0)
    {
        uint8_t config = inb(DATA_PORT);

        /* Bit 0: First PS/2 port interrupt enabled (IRQ1)
         * Bit 4: First PS/2 port clock enabled (0 = enable)
         * Bit 6: First PS/2 port translation enabled */
        config |= 0x41;
        config &= (uint8_t)~0x10;

        if (controller_command(0x60) == 0)
        {
            controller_data(config);
        }
    }

    /* Enable the first port. */
    controller_command(0xAE);

    /* Unmask IRQ1 on the Master PIC. */
    pic_unmask_irq(1);
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
        /* Check serial COM1 port (LSR bit 0: Data Ready) */
        if (inb(0x3FD) & 0x01)
        {
            char c = (char)inb(0x3F8);
            if (c == '\r')
                c = '\n';
            return c;
        }

        /* Check the interrupt-driven keyboard event queue */
        uint8_t scancode = 0;
        if (keyboard_dequeue(&scancode) == 0)
        {
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

        /* Fallback polling in case interrupts are disabled */
        uint8_t status = inb(STATUS_PORT);
        if (status & 0x01)
        {
            /* Check source: if not mouse (bit 5 == 0), process */
            if ((status & 0x20) == 0)
            {
                uint8_t raw = inb(DATA_PORT);
                if ((raw & 0x80) == 0)
                {
                    char c = keyboard_scancode_to_ascii(raw);
                    if (c != 0)
                    {
                        return c;
                    }
                }
            }
            else
            {
                (void)inb(DATA_PORT);
            }
        }

        __asm__ volatile ("pause");
    }
}

uint8_t keyboard_read_scancode(void)
{
    uint8_t scancode = 0;
    while (1)
    {
        if (keyboard_dequeue(&scancode) == 0)
        {
            return scancode;
        }

        uint8_t status = inb(STATUS_PORT);
        if (status & 0x01)
        {
            return inb(DATA_PORT);
        }

        __asm__ volatile ("pause");
    }
}

