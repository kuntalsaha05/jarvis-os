#include "../debug.h"
#include <stddef.h>

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void debug_init(void)
{
    outb(COM1_PORT + 1, 0x00); /* Disable all interrupts */
    outb(COM1_PORT + 3, 0x80); /* Enable DLAB (set baud rate divisor) */
    outb(COM1_PORT + 0, 0x03); /* Set divisor to 3 (lo byte) 38400 baud */
    outb(COM1_PORT + 1, 0x00); /*                  (hi byte) */
    outb(COM1_PORT + 3, 0x03); /* 8 bits, no parity, one stop bit */
    outb(COM1_PORT + 2, 0xC7); /* Enable FIFO, clear them, with 14-byte threshold */
    outb(COM1_PORT + 4, 0x0B); /* IRQs enabled, RTS/DSR set */
}

void debug_putchar(char c)
{
    /* Send to QEMU/Bochs debug port 0xE9 */
    outb(QEMU_DEBUG_PORT, (uint8_t)c);

    /* Wait for COM1 transmit buffer to be empty */
    while ((inb(COM1_PORT + 5) & 0x20) == 0) { }
    outb(COM1_PORT, (uint8_t)c);
}

void debug_puts(const char *str)
{
    if (!str) return;
    while (*str) {
        if (*str == '\n') {
            debug_putchar('\r');
        }
        debug_putchar(*str);
        str++;
    }
}

void debug_write_hex(uint64_t val)
{
    static const char hex_chars[] = "0123456789ABCDEF";
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        unsigned int shift = (15 - i) * 4;
        buf[i + 2] = hex_chars[(val >> shift) & 0xF];
    }
    buf[18] = '\0';
    debug_puts(buf);
}

void debug_write_dec(uint64_t val)
{
    if (val == 0) {
        debug_putchar('0');
        return;
    }
    char buf[21];
    int idx = 20;
    buf[idx--] = '\0';
    while (val > 0) {
        buf[idx--] = '0' + (val % 10);
        val /= 10;
    }
    debug_puts(&buf[idx + 1]);
}

__attribute__((noreturn))
void kernel_panic(const char *message)
{
    __asm__ volatile ("cli");

    /* Output to early serial / debug channels */
    debug_puts("\n\n========================================\n");
    debug_puts("            KERNEL PANIC                \n");
    debug_puts("========================================\n");
    debug_puts("Reason: ");
    debug_puts(message ? message : "Unspecified kernel panic");
    debug_puts("\n");

    uint64_t cr2, cr3, rsp;
    __asm__ volatile ("movq %%cr2, %0" : "=r"(cr2));
    __asm__ volatile ("movq %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("movq %%rsp, %0" : "=r"(rsp));

    debug_puts("CR2 (Fault Addr): ");
    debug_write_hex(cr2);
    debug_puts("\nCR3 (Page Table): ");
    debug_write_hex(cr3);
    debug_puts("\nRSP (Stack Ptr) : ");
    debug_write_hex(rsp);
    debug_puts("\nSystem halted to prevent data corruption.\n");
    debug_puts("========================================\n");

    /* Also write red banner directly to VGA text memory if accessible */
    volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
    const char *banner = " KERNEL PANIC: System Halted. See Serial Console ";
    for (int i = 0; banner[i] != '\0' && i < 80; i++) {
        vga[i] = (uint16_t)(0x4F00 | (uint8_t)banner[i]); /* White on Red */
    }

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
