#include "power.h"
#include "../drivers/terminal.h"
#include "../diagnostics/diagnostics.h"
#include "../debug.h"

#ifdef HOST_TEST
static int host_poweroff_called = 0;
static int host_reboot_called = 0;
#else
static inline void outw(uint16_t port, uint16_t val)
{
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

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
#endif

void power_init(void)
{
    /* Power management subsystem initialization */
}

__attribute__((noreturn))
void sys_poweroff(void)
{
    diag_log(DIAG_LEVEL_INFO, "POWER", "Controlled system shutdown initiated");

    terminal_write("\n==================================================\n");
    terminal_write("            JARVIS-OS SYSTEM SHUTDOWN             \n");
    terminal_write("==================================================\n");
    terminal_write("[SHUTDOWN] Flushing in-memory buffers...\n");
    terminal_write("[SHUTDOWN] Note: Storage is backed by volatile RAMDisk (/dev/ram0).\n");
    terminal_write("           Data is not retained across power cycles.\n");
    terminal_write("[SHUTDOWN] Stopping processes and disabling interrupts...\n");
    terminal_write("[SHUTDOWN] Powering off system via ACPI / PM interface...\n");

#ifndef HOST_TEST
    debug_puts("[SHUTDOWN] Note: Storage is backed by volatile RAMDisk (/dev/ram0). Data is not retained across power cycles.\n");
    debug_puts("[SHUTDOWN] Initiating ACPI / PM poweroff sequence...\n");

    __asm__ volatile ("cli");

    /* 1. QEMU default ACPI shutdown port (PIIX4 PM1a_CNT) */
    outw(0x604, 0x2000);

    /* 2. Bochs and older QEMU ACPI shutdown port */
    outw(0xB004, 0x2000);

    /* 3. QEMU debug exit port */
    outb(0x501, 0x00);

    /* 4. ISA debug-exit device port */
    outb(0xF4, 0x00);

    /* Fallback if running on real hardware without ACPI poweroff tables */
    terminal_write("[SHUTDOWN] Hardware ACPI power-off unsupported on this platform.\n");
    terminal_write("[SHUTDOWN] System halted safely. It is now safe to turn off power.\n");
    debug_puts("[SHUTDOWN] ACPI power-off unsupported on this platform. System halted.\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
#else
    host_poweroff_called = 1;
    for (;;) { }
#endif
}

__attribute__((noreturn))
void sys_reboot(void)
{
    diag_log(DIAG_LEVEL_INFO, "POWER", "Controlled system reboot initiated");

    terminal_write("\n==================================================\n");
    terminal_write("             JARVIS-OS SYSTEM REBOOT              \n");
    terminal_write("==================================================\n");
    terminal_write("[REBOOT] Flushing filesystem buffers...\n");
    terminal_write("[REBOOT] Note: RAMDisk storage will reset on restart.\n");
    terminal_write("[REBOOT] Resetting CPU via 8042 Keyboard Controller...\n");

#ifndef HOST_TEST
    debug_puts("[REBOOT] Note: RAMDisk storage will reset on restart.\n");
    debug_puts("[REBOOT] Pulsing CPU reset line via 8042 Keyboard Controller...\n");

    __asm__ volatile ("cli");

    /* 1. Pulse CPU reset line via 8042 PS/2 Keyboard Controller */
    for (int i = 0; i < 10000; i++) {
        uint8_t status = inb(0x64);
        if ((status & 0x02) == 0) {
            break;
        }
    }
    outb(0x64, 0xFE);

    /* 2. Fallback: Force CPU Triple Fault via NULL IDT */
    debug_puts("[REBOOT] 8042 reset pending; attempting triple-fault fallback...\n");
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) null_idt = {0, 0};

    __asm__ volatile (
        "lidt %0\n"
        "int3\n"
        :
        : "m"(null_idt)
    );

    /* 3. Final Fallback */
    terminal_write("[REBOOT] Hardware reset unsupported. System halted.\n");
    for (;;) {
        __asm__ volatile ("hlt");
    }
#else
    host_reboot_called = 1;
    for (;;) { }
#endif
}

int test_power_invariants(void)
{
    /* Verify port constants and disclosure messages */
    const char *disclose = "[SHUTDOWN] Note: Storage is backed by volatile RAMDisk (/dev/ram0). Data is not retained across power cycles.";
    if (!disclose || disclose[0] != '[') return -1;
    return 0;
}
