#include "idt.h"
#include "pic.h"
#include "process/scheduler.h"
#include "../debug.h"

#include "../diagnostics/diagnostics.h"

extern void terminal_write(const char *text);
extern void terminal_putchar(char c);

#ifndef HOST_TEST
extern void interrupt_stub(void);
extern void timer_stub(void);
extern void schedule_stub(void);
extern void syscall_stub(void);
extern void keyboard_stub(void);
extern void mouse_stub(void);
#else
void interrupt_stub(void) {}
void timer_stub(void) {}
void schedule_stub(void) {}
void syscall_stub(void) {}
void keyboard_stub(void) {}
void mouse_stub(void) {}
#endif

#ifndef HOST_TEST
extern void isr_exc_0(void);
extern void isr_exc_1(void);
extern void isr_exc_2(void);
extern void isr_exc_3(void);
extern void isr_exc_4(void);
extern void isr_exc_5(void);
extern void isr_exc_6(void);
extern void isr_exc_7(void);
extern void isr_exc_8(void);
extern void isr_exc_9(void);
extern void isr_exc_10(void);
extern void isr_exc_11(void);
extern void isr_exc_12(void);
extern void isr_exc_13(void);
extern void isr_exc_14(void);
extern void isr_exc_15(void);
extern void isr_exc_16(void);
extern void isr_exc_17(void);
extern void isr_exc_18(void);
extern void isr_exc_19(void);
extern void isr_exc_20(void);
extern void isr_exc_21(void);
extern void isr_exc_22(void);
extern void isr_exc_23(void);
extern void isr_exc_24(void);
extern void isr_exc_25(void);
extern void isr_exc_26(void);
extern void isr_exc_27(void);
extern void isr_exc_28(void);
extern void isr_exc_29(void);
extern void isr_exc_30(void);
extern void isr_exc_31(void);

static void (* const isr_exc_table[32])(void) = {
    isr_exc_0, isr_exc_1, isr_exc_2, isr_exc_3,
    isr_exc_4, isr_exc_5, isr_exc_6, isr_exc_7,
    isr_exc_8, isr_exc_9, isr_exc_10, isr_exc_11,
    isr_exc_12, isr_exc_13, isr_exc_14, isr_exc_15,
    isr_exc_16, isr_exc_17, isr_exc_18, isr_exc_19,
    isr_exc_20, isr_exc_21, isr_exc_22, isr_exc_23,
    isr_exc_24, isr_exc_25, isr_exc_26, isr_exc_27,
    isr_exc_28, isr_exc_29, isr_exc_30, isr_exc_31
};
#else
static void dummy_exc(void) {}
static void (* const isr_exc_table[32])(void) = {
    dummy_exc, dummy_exc, dummy_exc, dummy_exc,
    dummy_exc, dummy_exc, dummy_exc, dummy_exc,
    dummy_exc, dummy_exc, dummy_exc, dummy_exc,
    dummy_exc, dummy_exc, dummy_exc, dummy_exc,
    dummy_exc, dummy_exc, dummy_exc, dummy_exc,
    dummy_exc, dummy_exc, dummy_exc, dummy_exc,
    dummy_exc, dummy_exc, dummy_exc, dummy_exc,
    dummy_exc, dummy_exc, dummy_exc, dummy_exc
};
#endif

static const char * const exception_names[32] = {
    "#DE Divide Error",
    "#DB Debug Exception",
    "NMI Non-Maskable Interrupt",
    "#BP Breakpoint",
    "#OF Overflow",
    "#BR Bound Range Exceeded",
    "#UD Invalid Opcode",
    "#NM Device Not Available",
    "#DF Double Fault",
    "Coprocessor Segment Overrun",
    "#TS Invalid TSS",
    "#NP Segment Not Present",
    "#SS Stack-Segment Fault",
    "#GP General Protection Fault",
    "#PF Page Fault",
    "Reserved Exception (#15)",
    "#MF x87 Floating-Point Exception",
    "#AC Alignment Check",
    "#MC Machine Check",
    "#XM SIMD Floating-Point Exception",
    "#VE Virtualization Exception",
    "#CP Control Protection Exception",
    "Reserved Exception (#22)",
    "Reserved Exception (#23)",
    "Reserved Exception (#24)",
    "Reserved Exception (#25)",
    "Reserved Exception (#26)",
    "Reserved Exception (#27)",
    "Reserved Exception (#28)",
    "Reserved Exception (#29)",
    "Security Exception (#30)",
    "Reserved Exception (#31)"
};

const char *exception_name(uint64_t vector)
{
    if (vector < 32) {
        return exception_names[vector];
    }
    return "Unknown Exception";
}

void interrupt_handler(void)
{
}

__attribute__((noreturn))
void generic_exception_handler(uint64_t *frame)
{
    (void)frame;
    kernel_panic("Fatal CPU Exception");
}

#ifndef HOST_TEST
volatile uint64_t timer_ticks = 0;
#endif

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

void c_exception_handler(exception_frame_t *frame, uint64_t cr2)
{
    __asm__ volatile ("cli");

    if (!frame) {
        kernel_panic("Fatal CPU Exception: NULL Frame");
    }

    diag_record_irq((uint8_t)frame->vector);

    const char *name = exception_name(frame->vector);
    diag_log(DIAG_LEVEL_PANIC, "EXCEPTION", name);

    /* Serial crash dump over early COM1 port */
    debug_puts("\n\n========================================\n");
    debug_puts("      CPU EXCEPTION CRASH DUMP          \n");
    debug_puts("========================================\n");
    debug_puts("Exception:  ");
    debug_puts(name);
    debug_puts(" (Vector ");
    debug_write_dec(frame->vector);
    debug_puts(")\n");

    debug_puts("Fault RIP:  ");
    debug_write_hex(frame->rip);
    debug_puts(" (CS: ");
    debug_write_hex(frame->cs);
    debug_puts(")\n");

    debug_puts("Error Code: ");
    debug_write_hex(frame->error_code);
    debug_puts("\n");

    debug_puts("RFLAGS:     ");
    debug_write_hex(frame->rflags);
    debug_puts("\n");

    debug_puts("Fault RSP:  ");
    debug_write_hex(frame->rsp);
    debug_puts(" (SS: ");
    debug_write_hex(frame->ss);
    debug_puts(")\n");

    if (frame->vector == 14) {
        debug_puts("CR2 (Fault):");
        debug_write_hex(cr2);
        debug_puts("\n");

        page_fault_info_t pf_info;
        page_fault_decode(cr2, frame->error_code, &pf_info);
        debug_puts("PF Reason:  ");
        debug_puts(pf_info.is_present ? "Protection Violation (" : "Page Not Present (");
        debug_puts(pf_info.is_write ? "Write, " : "Read, ");
        debug_puts(pf_info.is_user ? "User Mode, " : "Supervisor Mode, ");
        debug_puts(pf_info.is_instruction_fetch ? "Inst Fetch)\n" : "Data Access)\n");
        debug_puts("PF Region:  ");
        debug_puts(pf_info.is_mapped_region ? "Identity Mapped 2 MiB Boundary\n" : "Unmapped Memory Space\n");
    }

    debug_puts("General Registers:\n");
    debug_puts("  RAX: "); debug_write_hex(frame->rax);
    debug_puts("  RBX: "); debug_write_hex(frame->rbx);
    debug_puts("\n  RCX: "); debug_write_hex(frame->rcx);
    debug_puts("  RDX: "); debug_write_hex(frame->rdx);
    debug_puts("\n  RSI: "); debug_write_hex(frame->rsi);
    debug_puts("  RDI: "); debug_write_hex(frame->rdi);
    debug_puts("\n  RBP: "); debug_write_hex(frame->rbp);
    debug_puts("  R8:  "); debug_write_hex(frame->r8);
    debug_puts("\n  R9:  "); debug_write_hex(frame->r9);
    debug_puts("  R10: "); debug_write_hex(frame->r10);
    debug_puts("\n  R11: "); debug_write_hex(frame->r11);
    debug_puts("  R12: "); debug_write_hex(frame->r12);
    debug_puts("\n  R13: "); debug_write_hex(frame->r13);
    debug_puts("  R14: "); debug_write_hex(frame->r14);
    debug_puts("\n  R15: "); debug_write_hex(frame->r15);
    debug_puts("\n========================================\n");

    /* Also print summary to VGA console if in text mode */
    terminal_write("\n[KERNEL PANIC] CPU Exception: ");
    terminal_write(name);
    terminal_write("\nFaulting RIP: ");
    write_hex64(frame->rip);
    terminal_write(" Error: ");
    write_hex64(frame->error_code);
    terminal_putchar('\n');

    if (frame->vector == 14) {
        page_fault_diagnose(cr2, frame->error_code);
    }

    kernel_panic(name);
}

__attribute__((noreturn))
void page_fault_handler(uint64_t address, uint64_t error_code)
{
    __asm__ volatile ("cli");

    terminal_write("\nJARVIS-OS KERNEL PAGE FAULT (#PF)\n");
    page_fault_diagnose(address, error_code);

    kernel_panic("Kernel Page Fault (#PF)");
}

int test_exception_invariants(void)
{
    if (sizeof(exception_frame_t) != 176) return -1;
    if (exception_names[0][1] != 'D') return -2;
    if (exception_names[13][1] != 'G') return -3;
    if (exception_names[14][1] != 'P') return -4;
    return test_page_fault_diagnostics();
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
        idt_set_gate(
            (uint8_t)i,
            (uint64_t)isr_exc_table[i],
            0x08,
            0x8E
        );
    }

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
        44,
        (uint64_t)mouse_stub,
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
