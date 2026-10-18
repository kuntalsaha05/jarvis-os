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
extern void keyboard_stub(void);

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

void page_fault_decode(uint64_t address, uint64_t error_code, page_fault_info_t *info)
{
    if (!info)
        return;
    info->fault_address = address;
    info->error_code = error_code;
    info->is_present = (error_code & 1) ? 1 : 0;
    info->is_write = (error_code & 2) ? 1 : 0;
    info->is_user = (error_code & 4) ? 1 : 0;
    info->is_rsvd_violation = (error_code & 8) ? 1 : 0;
    info->is_instruction_fetch = (error_code & 16) ? 1 : 0;
    /* Identity-mapped address space is first 2 MiB: 0x00000000 to 0x001FFFFF */
    info->is_mapped_region = (address < 0x200000ULL) ? 1 : 0;
}

void page_fault_diagnose(uint64_t address, uint64_t error_code)
{
    page_fault_info_t info;
    page_fault_decode(address, error_code, &info);

    terminal_write("Fault address: ");
    write_hex64(info.fault_address);

    terminal_write("\nError code:    ");
    write_hex64(info.error_code);

    terminal_write("\nReason:        ");
    terminal_write(info.is_present ? "Protection violation" : "Page not present");

    terminal_write("\nAccess:        ");
    terminal_write(info.is_write ? "Write" : "Read");

    terminal_write("\nMode:          ");
    terminal_write(info.is_user ? "User" : "Supervisor");

    terminal_write("\nReserved Bit:  ");
    terminal_write(info.is_rsvd_violation ? "Violation detected" : "None");

    terminal_write("\nInst Fetch:    ");
    terminal_write(info.is_instruction_fetch ? "Yes (Instruction Execution)" : "No (Data Access)");

    terminal_write("\nMemory Region: ");
    if (info.is_mapped_region) {
        terminal_write("Identity-mapped 2 MiB boundary (0x00000000 - 0x001FFFFF)");
    } else {
        terminal_write("Unmapped address space (>= 0x00200000)");
    }
    terminal_write("\n");
}

int test_page_fault_diagnostics(void)
{
    page_fault_info_t info;

    /* Case 1: Supervisor read of unmapped memory at 0x205000 */
    page_fault_decode(0x205000ULL, 0x0, &info);
    if (info.is_present != 0 || info.is_write != 0 || info.is_user != 0 || info.is_mapped_region != 0)
        return -1;

    /* Case 2: Supervisor write to identity-mapped page at 0x100000 */
    page_fault_decode(0x100000ULL, 0x2, &info);
    if (info.is_present != 0 || info.is_write != 1 || info.is_user != 0 || info.is_mapped_region != 1)
        return -2;

    /* Case 3: User mode protection violation on write at 0x50000 */
    page_fault_decode(0x50000ULL, 0x7, &info);
    if (info.is_present != 1 || info.is_write != 1 || info.is_user != 1 || info.is_mapped_region != 1)
        return -3;

    /* Case 4: Instruction fetch from unmapped high address 0xDEADBEEF000 */
    page_fault_decode(0xDEADBEEF000ULL, 0x10, &info);
    if (info.is_instruction_fetch != 1 || info.is_mapped_region != 0)
        return -4;

    return 0;
}

__attribute__((noreturn))
void page_fault_handler(uint64_t address, uint64_t error_code)
{
    __asm__ volatile ("cli");

    terminal_write("\nJARVIS-OS KERNEL PAGE FAULT (#PF)\n");
    page_fault_diagnose(address, error_code);

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
        33,
        (uint64_t)keyboard_stub,
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
