#include <stdint.h>
#include <stdint.h>
#include "interrupts/idt.h"
#include "interrupts/pic.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static volatile uint16_t *vga_buffer = (uint16_t *)0xB8000;
static uint8_t terminal_color = 0x07;
static uint8_t terminal_row = 0;
static uint8_t terminal_column = 0;

static uint16_t vga_entry(unsigned char c, uint8_t color)
{
    return (uint16_t)c | (uint16_t)color << 8;
}

static void terminal_clear(void)
{
    for (uint8_t row = 0; row < VGA_HEIGHT; row++)
    {
        for (uint8_t column = 0; column < VGA_WIDTH; column++)
        {
            const uint16_t index = row * VGA_WIDTH + column;
            vga_buffer[index] = vga_entry(' ', terminal_color);
        }
    }

    terminal_row = 0;
    terminal_column = 0;
}

static void terminal_putchar(char c)
{
    if (c == '\n')
    {
        terminal_column = 0;
        terminal_row++;
        return;
    }

    const uint16_t index = terminal_row * VGA_WIDTH + terminal_column;

    vga_buffer[index] = vga_entry(c, terminal_color);

    terminal_column++;

    if (terminal_column == VGA_WIDTH)
    {
        terminal_column = 0;
        terminal_row++;
    }
}

static void terminal_write(const char *str)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        terminal_putchar(str[i]);
    }
}

void kernel_main(void)
{
    terminal_clear();

    idt_init();
    pic_init();

    terminal_write("========================================\n");
    terminal_write("              JARVIS-OS\n");
    terminal_write("========================================\n");
    terminal_write("\n");
    terminal_write("Kernel: x86-64\n");
    terminal_write("Status: ONLINE\n");
    terminal_write("\n");
    terminal_write("JARVIS-OS kernel initialized.\n");

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}
