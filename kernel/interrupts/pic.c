#include <stdint.h>
#include "pic.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI      0x20

#define ICW1_INIT    0x10
#define ICW1_ICW4    0x01

#define ICW4_8086    0x01

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static void io_wait(void)
{
    outb(0x80, 0);
}

void pic_init(void)
{
    uint8_t master_mask = inb(PIC1_DATA);
    uint8_t slave_mask = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* Remap master PIC to vectors 32-39 */
    outb(PIC1_DATA, 0x20);
    io_wait();

    /* Remap slave PIC to vectors 40-47 */
    outb(PIC2_DATA, 0x28);
    io_wait();

    /* Tell master about slave at IRQ2 */
    outb(PIC1_DATA, 0x04);
    io_wait();

    /* Tell slave its cascade identity */
    outb(PIC2_DATA, 0x02);
    io_wait();

    /* Use 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();

    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Restore existing masks, then enable IRQ0 (PIT timer) and IRQ1 (Keyboard). */
    master_mask &= (uint8_t)~(0x01 | 0x02);

    outb(PIC1_DATA, master_mask);
    outb(PIC2_DATA, slave_mask);
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
    {
        outb(PIC2_COMMAND, PIC_EOI);
    }

    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_unmask_irq(uint8_t irq)
{
    uint16_t port;
    uint8_t value;

    if (irq < 8)
    {
        port = PIC1_DATA;
    }
    else
    {
        /* Ensure cascade line (IRQ2 on master) is also unmasked */
        uint8_t m_val = inb(PIC1_DATA) & (uint8_t)~(1 << 2);
        outb(PIC1_DATA, m_val);

        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) & (uint8_t)~(1 << irq);
    outb(port, value);
}

void pic_mask_irq(uint8_t irq)
{
    uint16_t port;
    uint8_t value;

    if (irq < 8)
    {
        port = PIC1_DATA;
    }
    else
    {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) | (uint8_t)(1 << irq);
    outb(port, value);
}

