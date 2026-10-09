#include <stdint.h>
#include <stdint.h>
#include "interrupts/idt.h"
#include "interrupts/pic.h"
#include "interrupts/pit.h"

extern volatile uint64_t timer_ticks;

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

static void display_timer_ticks(void)
{
    volatile uint16_t *video = (volatile uint16_t *)0xB8000;

    const char *label = "Timer ticks: ";
    int position = 6 * VGA_WIDTH;

    for (int i = 0; label[i] != '\0'; i++)
    {
        video[position + i] = vga_entry(label[i], terminal_color);
    }

    uint64_t value = timer_ticks;
    int digits_start = position + 13;

    for (int i = 0; i < 10; i++)
    {
        video[digits_start + i] = vga_entry(' ', terminal_color);
    }

    if (value == 0)
    {
        video[digits_start] = vga_entry('0', terminal_color);
        return;
    }

    char digits[20];
    int count = 0;

    while (value > 0)
    {
        digits[count++] = '0' + (value % 10);
        value /= 10;
    }

    for (int i = 0; i < count; i++)
    {
        video[digits_start + i] =
            vga_entry(digits[count - i - 1], terminal_color);
    }
}

void kernel_main(void)
{
    terminal_clear();

    idt_init();
    pic_init();
    pit_init(100);
    
    __asm__ volatile ("sti");

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
    display_timer_ticks();
    __asm__ volatile ("hlt");
    }
}
	
