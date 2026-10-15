#include "multiboot.h"
#include "../drivers/terminal.h"

void multiboot_dump_memory_map(uint64_t magic, uint64_t mb_addr)
{
    terminal_write("Bootloader Hand-off: ");
    if ((uint32_t)magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        terminal_write("Standard Multiboot Header (Direct Boot)\n");
        return;
    }

    terminal_write("Multiboot 0.6.96 Verified (Magic 0x2BADB002)\n");

    if (mb_addr == 0) return;
    const multiboot_info_t *mbi = (const multiboot_info_t *)(uintptr_t)mb_addr;

    if (mbi->flags & MULTIBOOT_INFO_MEMORY) {
        terminal_write("Physical Memory: Lower ");
        terminal_write_number(mbi->mem_lower);
        terminal_write(" KiB, Upper ");
        terminal_write_number(mbi->mem_upper);
        terminal_write(" KiB (~");
        terminal_write_number((mbi->mem_upper + 1024) / 1024);
        terminal_write(" MiB total)\n");
    }

    if (mbi->flags & MULTIBOOT_INFO_MEM_MAP) {
        terminal_write("BIOS/GRUB Memory Map:\n");
        uintptr_t mmap_ptr = (uintptr_t)mbi->mmap_addr;
        uintptr_t mmap_end = mmap_ptr + mbi->mmap_length;

        int entry_count = 0;
        while (mmap_ptr < mmap_end && entry_count < 6) {
            const multiboot_mmap_entry_t *entry = (const multiboot_mmap_entry_t *)mmap_ptr;
            terminal_write("  [Region] Addr: 0x");
            terminal_write_hex((uint32_t)(entry->addr & 0xFFFFFFFF));
            terminal_write(" Len: ");
            terminal_write_number((uint32_t)(entry->len / 1024));
            terminal_write(" KiB Type: ");
            if (entry->type == 1) {
                terminal_write("1 (Usable RAM)\n");
            } else if (entry->type == 2) {
                terminal_write("2 (Reserved/ACPI)\n");
            } else {
                terminal_write_number(entry->type);
                terminal_putchar('\n');
            }

            mmap_ptr += entry->size + sizeof(entry->size);
            entry_count++;
        }
    }
}
