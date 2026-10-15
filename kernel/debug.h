#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>

/* Early I/O ports for low-level diagnostics */
#define COM1_PORT 0x3F8
#define QEMU_DEBUG_PORT 0xE9

/* Early boot diagnostics independent of the full terminal/VGA drivers */
void debug_init(void);
void debug_putchar(char c);
void debug_puts(const char *str);
void debug_write_hex(uint64_t val);
void debug_write_dec(uint64_t val);

/*
 * Kernel panic handler: disables interrupts, prints diagnostic banner,
 * dumps CPU registers, and halts the system safely.
 */
__attribute__((noreturn))
void kernel_panic(const char *message);

#endif /* DEBUG_H */
