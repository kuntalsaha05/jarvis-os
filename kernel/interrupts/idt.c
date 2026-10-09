#include <stdint.h>
#include "idt.h"
#include "pic.h"

extern void interrupt_stub(void);
extern void timer_stub(void);

void interrupt_handler(void)
{
}

volatile uint64_t timer_ticks = 0;

void timer_handler(void)
{
    timer_ticks++;

    pic_send_eoi(0);
}

struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idt_descriptor;

static void idt_set_gate(
    uint8_t vector,
    uint64_t handler,
    uint16_t selector,
    uint8_t type_attr)
{
    idt[vector].offset_low = handler & 0xFFFF;
    idt[vector].selector = selector;
    idt[vector].zero = 0;
    idt[vector].type_attr = type_attr;
    idt[vector].offset_mid = (handler >> 16) & 0xFFFF;
    idt[vector].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

void idt_init(void)
{
    for (int i = 0; i < 256; i++)
    {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].reserved = 0;
    }
    
    idt_set_gate(
    	32,
    	(uint64_t)timer_stub,
    	0x08,
    	0x8E
    );

    idt_descriptor.limit = sizeof(idt) - 1;
    idt_descriptor.base = (uint64_t)&idt[0];

    __asm__ volatile (
        "lidt %0"
        :
        : "m"(idt_descriptor)
    );
}
