
#include <stdint.h>

#include "drivers/keyboard.h"
#include "drivers/terminal.h"
#include "interrupts/idt.h"
#include "interrupts/pic.h"
#include "interrupts/pit.h"
#include "ipc/ipc.h"
#include "memory/heap.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "shell/shell.h"
#include "storage/ramdisk.h"
#include "storage/block.h"

static int test_ramdisk(void)
{
    uint8_t write_buffer[512];
    uint8_t read_buffer[512];

    for (uint32_t i = 0; i < 512; i++)
    {
        write_buffer[i] = (uint8_t)(i & 0xFF);
        read_buffer[i] = 0;
    }

    if (ramdisk_init() != 0)
        return -1;

    if (block_write(0, 1, write_buffer) != 0)
        return -2;

    if (block_read(0, 1, read_buffer) != 0)
        return -3;

    for (uint32_t i = 0; i < 512; i++)
    {
        if (write_buffer[i] != read_buffer[i])
            return -4;
    }

    return 0;
}

static void task_a(void)
{
    for (int i = 1; i <= 3; i++) {
        terminal_write("Task A iteration ");
        terminal_write_number((uint32_t)i);
        terminal_putchar('\n');
        scheduler_yield();
    }
}

static void task_b(void)
{
    for (int i = 1; i <= 3; i++) {
        terminal_write("Task B iteration ");
        terminal_write_number((uint32_t)i);
        terminal_putchar('\n');
        scheduler_yield();
    }
}

void kernel_main(void)
{
    terminal_initialize();

    heap_init();

    int ramdisk_result = test_ramdisk();
    terminal_write("RAM disk test: ");
    if (ramdisk_result == 0)
    {
        terminal_write("PASS\n");
    }
    else
    {
        terminal_write("FAIL, code ");
        terminal_write_number((uint32_t)(-ramdisk_result));
        terminal_putchar('\n');
    }

    terminal_write("========================================\n");
    terminal_write("              JARVIS-OS\n");
    terminal_write("========================================\n\n");

    terminal_write("Kernel: x86-64\n");
    terminal_write("Status: INITIALIZING\n\n");

    idt_init();
    pic_init();
    pit_init(100);
    ipc_init();

    process_init();
    scheduler_init();

    terminal_write("Context-switch test starting...\n");

    int task_a_pid = process_create((uint64_t)task_a, 1);
    int task_b_pid = process_create((uint64_t)task_b, 1);

    if (task_a_pid > 0 && task_b_pid > 0)
    {
        scheduler_start();
        terminal_write("Context-switch test: returned to kernel\n");
    }
    else
    {
        terminal_write("Context-switch test: task creation failed\n");
    }

    terminal_write("Kernel initialization complete.\n");
    terminal_write("Status: ONLINE\n");

    __asm__ volatile ("sti");

    keyboard_init();
    shell_run();

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}
