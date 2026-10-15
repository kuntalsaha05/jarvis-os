#include "idt.h"
#include "pic.h"
#include "process/scheduler.h"
#include "../debug.h"

extern void page_fault_stub(void);
extern void terminal_write(const char *text);
extern void interrupt_stub(void);
extern void timer_stub(void);
extern void schedule_stub(void);
extern void generic_exception_stub(void);
extern void syscall_stub(void);

void interrupt_handler(void)
{
}

__attribute__((noreturn))
void generic_exception_handler(uint64_t *frame)
{
    (void)frame;
    kernel_panic("Fatal CPU Exception");
}

volatile uint64_t timer_ticks = 0;

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

static void write_hex64(uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char buffer[19];

    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 0; i < 16; i++) {
        unsigned int shift = (15 - i) * 4;
        buffer[i + 2] = digits[(value >> shift) & 0xF];
    }

    buffer[18] = '\0';
    terminal_write(buffer);
}

__attribute__((noreturn))
void page_fault_handler(uint64_t address, uint64_t error_code)
{
    __asm__ volatile ("cli");

    terminal_write("\nJARVIS-OS KERNEL PAGE FAULT\n");

    terminal_write("Fault address: ");
    write_hex64(address);

    terminal_write("\nError code: ");
    write_hex64(error_code);

    terminal_write("\nReason: ");
    terminal_write((error_code & 1) ?
                   "Protection violation" :
                   "Page not present");

    terminal_write("\nAccess: ");
    terminal_write((error_code & 2) ?
                   "Write" : "Read");

    terminal_write("\nMode: ");
    terminal_write((error_code & 4) ?
                   "User" : "Supervisor");

    kernel_panic("Kernel Page Fault (#PF)");
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
    
    for (int i = 0; i < 32; i++)
    {
        if (i != 14)
        {
            idt_set_gate(
                (uint8_t)i,
                (uint64_t)generic_exception_stub,
                0x08,
                0x8E
            );
        }
    }

    idt_set_gate(
        14,
        (uint64_t)page_fault_stub,
        0x08,
        0x8E
    );

    idt_set_gate(
        32,
        (uint64_t)timer_stub,
        0x08,
        0x8E
    );

    idt_set_gate(
        0x80,
        (uint64_t)syscall_stub,
        0x08,
        0xEE
    );

    idt_set_gate(
        0x81,
        (uint64_t)schedule_stub,
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
