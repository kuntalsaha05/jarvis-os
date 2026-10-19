#ifndef POWER_H
#define POWER_H

#include <stdint.h>

void power_init(void);

/*
 * Controlled system shutdown / poweroff:
 * - Logs shutdown event to diagnostic ring buffer
 * - Flushes filesystem caches
 * - Discloses volatile RAMDisk limitation explicitly
 * - Disables interrupts
 * - Issues ACPI / PM poweroff via QEMU I/O ports (0x604 / 0xB004 / 0x501 / 0xF4)
 * - Falls back to safe halting loop (cli; hlt) if hardware poweroff unsupported
 */
__attribute__((noreturn))
void sys_poweroff(void);

/*
 * Controlled system reboot / restart:
 * - Logs reboot event to diagnostic ring buffer
 * - Flushes filesystem caches
 * - Pulses CPU reset line via 8042 PS/2 Keyboard Controller (port 0x64, cmd 0xFE)
 * - Falls back to CPU triple-fault if 8042 reset fails
 * - Falls back to safe halting loop if reboot unsupported
 */
__attribute__((noreturn))
void sys_reboot(void);

/* Invariant verification for host test suite */
int test_power_invariants(void);

#endif /* POWER_H */

