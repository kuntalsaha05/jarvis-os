#include "terminal.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static volatile uint16_t *const vga_buffer =
    (volatile uint16_t *)0xB8000;

static uint8_t terminal_color = 0x07;
static uint8_t terminal_row = 0;
static uint8_t terminal_column = 0;

static uint16_t vga_entry(char c, uint8_t color)
{
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

void terminal_set_color(uint8_t color)
{
    terminal_color = color;
}

void terminal_clear(void)
{
    for (uint32_t row = 0; row < VGA_HEIGHT; row++)
    {
        for (uint32_t column = 0; column < VGA_WIDTH; column++)
        {
            vga_buffer[row * VGA_WIDTH + column] =
                vga_entry(' ', terminal_color);
        }
    }

    terminal_row = 0;
    terminal_column = 0;
}

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_init(void)
{
    outb(0x3F9, 0x00);    /* Disable all interrupts */
    outb(0x3FB, 0x80);    /* Enable DLAB (set baud rate divisor) */
    outb(0x3F8, 0x03);    /* Set divisor to 3 (lo byte) 38400 baud */
    outb(0x3F9, 0x00);    /*                  (hi byte) */
    outb(0x3FB, 0x03);    /* 8 bits, no parity, one stop bit */
    outb(0x3FA, 0xC7);    /* Enable FIFO, clear them, with 14-byte threshold */
    outb(0x3FC, 0x0B);    /* IRQs enabled, RTS/DSR set */
}

static void serial_putchar(char c)
{
    /* Output to QEMU/Bochs debug port 0xE9 directly */
    outb(0xE9, (uint8_t)c);

    /* Output to COM1 serial port */
    for (int i = 0; i < 10000; i++)
    {
        if (inb(0x3FD) & 0x20)
            break;
    }
    outb(0x3F8, (uint8_t)c);
}

void terminal_initialize(void)
{
    serial_init();
    terminal_set_color(0x07);
    terminal_clear();
}

static void terminal_scroll(void)
{
    for (uint32_t row = 0; row < VGA_HEIGHT - 1; row++)
    {
        for (uint32_t col = 0; col < VGA_WIDTH; col++)
        {
            vga_buffer[row * VGA_WIDTH + col] =
                vga_buffer[(row + 1) * VGA_WIDTH + col];
        }
    }

    for (uint32_t col = 0; col < VGA_WIDTH; col++)
    {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + col] =
            vga_entry(' ', terminal_color);
    }
    terminal_row = VGA_HEIGHT - 1;
}

void terminal_putchar(char c)
{
    if (c == '\n')
    {
        serial_putchar('\r');
    }
    serial_putchar(c);

    if (c == '\b')
    {
        if (terminal_column > 0)
        {
            terminal_column--;
        }
        else if (terminal_row > 0)
        {
            terminal_row--;
            terminal_column = VGA_WIDTH - 1;
        }

        vga_buffer[terminal_row * VGA_WIDTH + terminal_column] =
            vga_entry(' ', terminal_color);
        return;
    }

    if (c == '\n')
    {
        terminal_column = 0;

        if (terminal_row < VGA_HEIGHT - 1)
        {
            terminal_row++;
        }
        else
        {
            terminal_scroll();
        }

        return;
    }

    vga_buffer[terminal_row * VGA_WIDTH + terminal_column] =
        vga_entry(c, terminal_color);

    terminal_column++;

    if (terminal_column >= VGA_WIDTH)
    {
        terminal_column = 0;

        if (terminal_row < VGA_HEIGHT - 1)
        {
            terminal_row++;
        }
        else
        {
            terminal_scroll();
        }
    }
}

void terminal_write(const char *str)
{
    if (str == 0)
    {
        return;
    }

    for (uint32_t i = 0; str[i] != '\0'; i++)
    {
        terminal_putchar(str[i]);
    }
}

void terminal_write_number(uint32_t number)
{
    char buffer[10];
    uint32_t count = 0;

    if (number == 0)
    {
        terminal_putchar('0');
        return;
    }

    while (number > 0)
    {
        buffer[count++] = '0' + (number % 10);
        number /= 10;
    }

    while (count > 0)
    {
        terminal_putchar(buffer[--count]);
    }
}

void terminal_write_hex(uint32_t number)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    char buffer[8];
    for (int i = 7; i >= 0; i--) {
        buffer[i] = hex_digits[number & 0xF];
        number >>= 4;
    }
    for (int i = 0; i < 8; i++) {
        terminal_putchar(buffer[i]);
    }
}

void terminal_write_at(
    uint8_t row,
    uint8_t column,
    const char *text)
{
    if (text == 0 ||
        row >= VGA_HEIGHT ||
        column >= VGA_WIDTH)
    {
        return;
    }

    uint32_t position = row * VGA_WIDTH + column;

    for (uint32_t i = 0;
         text[i] != '\0' && column + i < VGA_WIDTH;
         i++)
    {
        vga_buffer[position + i] =
            vga_entry(text[i], terminal_color);
    }
}
