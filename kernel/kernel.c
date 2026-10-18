
#include <stdint.h>

#include "drivers/keyboard.h"
#include "drivers/terminal.h"
#include "interrupts/idt.h"
#include "interrupts/pic.h"
#include "interrupts/pit.h"
#include "ipc/ipc.h"

#include "ipc/semaphore.h"
#include "ipc/sync_demo.h"
#include "memory/heap.h"

#include "process/process.h"
#include "process/scheduler.h"
#include "shell/shell.h"
#include "storage/ramdisk.h"
#include "storage/block.h"
#include "storage/fs.h"
#include "memory/page.h"
#include "memory/multiboot.h"
#include "debug.h"

extern volatile uint64_t timer_ticks;

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

static volatile uint32_t p1_count = 0;
static volatile uint32_t p2_count = 0;

static void preempt_task_1(void)
{
    uint64_t start = timer_ticks;
    terminal_write("[PREEMPT 1] Spinning in CPU busy loop (no yield)...\n");
    while (timer_ticks < start + 12) {
        p1_count++;
        for (volatile int k = 0; k < 20000; k++) { }
    }
    terminal_write("[PREEMPT 1] Preempted across timer quanta, completed!\n");
}

static void preempt_task_2(void)
{
    uint64_t start = timer_ticks;
    terminal_write("[PREEMPT 2] Spinning in CPU busy loop (no yield)...\n");
    while (timer_ticks < start + 12) {
        p2_count++;
        for (volatile int k = 0; k < 20000; k++) { }
    }
    terminal_write("[PREEMPT 2] Preempted across timer quanta, completed!\n");
}

static volatile int reg_test_passed = 0;
static volatile int reg_test_iterations_1 = 0;
static volatile int reg_test_iterations_2 = 0;

static void reg_test_task_1(void)
{
    volatile uint64_t stack_canary = 0xCAFEBABE11111111ULL;

    for (int i = 0; i < 3; i++) {
        __asm__ volatile (
            "movq $0x1111222233334444, %%rbx\n"
            "movq $0xA1A1A1A1B1B1B1B1, %%r12\n"
            "movq $0xA2A2A2A2B2B2B2B2, %%r13\n"
            "movq $0xA3A3A3A3B3B3B3B3, %%r14\n"
            "movq $0xA4A4A4A4B4B4B4B4, %%r15\n"
            :
            :
            : "rbx", "r12", "r13", "r14", "r15"
        );

        scheduler_yield();

        uint64_t cur_rbx, cur_r12, cur_r13, cur_r14, cur_r15;
        __asm__ volatile (
            "movq %%rbx, %0\n"
            "movq %%r12, %1\n"
            "movq %%r13, %2\n"
            "movq %%r14, %3\n"
            "movq %%r15, %4\n"
            : "=r"(cur_rbx), "=r"(cur_r12), "=r"(cur_r13), "=r"(cur_r14), "=r"(cur_r15)
        );

        if (cur_rbx != 0x1111222233334444ULL ||
            cur_r12 != 0xA1A1A1A1B1B1B1B1ULL ||
            cur_r13 != 0xA2A2A2A2B2B2B2B2ULL ||
            cur_r14 != 0xA3A3A3A3B3B3B3B3ULL ||
            cur_r15 != 0xA4A4A4A4B4B4B4B4ULL ||
            stack_canary != 0xCAFEBABE11111111ULL) {
            reg_test_passed = -1;
            return;
        }
        reg_test_iterations_1++;
    }
}

static void reg_test_task_2(void)
{
    volatile uint64_t stack_canary = 0xDEADBEEF22222222ULL;

    for (int i = 0; i < 3; i++) {
        __asm__ volatile (
            "movq $0x5555666677778888, %%rbx\n"
            "movq $0xC1C1C1C1D1D1D1D1, %%r12\n"
            "movq $0xC2C2C2C2D2D2D2D2, %%r13\n"
            "movq $0xC3C3C3C3D3D3D3D3, %%r14\n"
            "movq $0xC4C4C4C4D4D4D4D4, %%r15\n"
            :
            :
            : "rbx", "r12", "r13", "r14", "r15"
        );

        scheduler_yield();

        uint64_t cur_rbx, cur_r12, cur_r13, cur_r14, cur_r15;
        __asm__ volatile (
            "movq %%rbx, %0\n"
            "movq %%r12, %1\n"
            "movq %%r13, %2\n"
            "movq %%r14, %3\n"
            "movq %%r15, %4\n"
            : "=r"(cur_rbx), "=r"(cur_r12), "=r"(cur_r13), "=r"(cur_r14), "=r"(cur_r15)
        );

        if (cur_rbx != 0x5555666677778888ULL ||
            cur_r12 != 0xC1C1C1C1D1D1D1D1ULL ||
            cur_r13 != 0xC2C2C2C2D2D2D2D2ULL ||
            cur_r14 != 0xC3C3C3C3D3D3D3D3ULL ||
            cur_r15 != 0xC4C4C4C4D4D4D4D4ULL ||
            stack_canary != 0xDEADBEEF22222222ULL) {
            reg_test_passed = -2;
            return;
        }
        reg_test_iterations_2++;
    }
}

static void test_page_allocator(void)
{
    page_init();

    void *a = page_alloc();
    void *b = page_alloc();

    int passed =
        a != NULL &&
        b != NULL &&
        a != b &&
        ((uintptr_t)a % PAGE_SIZE) == 0 &&
        ((uintptr_t)b % PAGE_SIZE) == 0;

    if (passed) {
        passed =
            page_free(a) == 0 &&
            page_free(b) == 0 &&
            page_free_count() == PAGE_POOL_COUNT;
    }

    terminal_write(
        passed ?
        "Page allocator test: PASS\n" :
        "Page allocator test: FAIL\n"
    );

    int inv_res = test_page_allocator_invariants();
    if (inv_res == 0) {
        terminal_write("Page allocator invariants: PASS (64-page pool, bounds, double-free, alignment)\n");
    } else {
        terminal_write("Page allocator invariants: FAIL, code ");
        terminal_write_number((uint32_t)(-inv_res));
        terminal_putchar('\n');
    }

    int pf_res = test_page_fault_diagnostics();
    if (pf_res == 0) {
        terminal_write("Page fault diagnostics: PASS (safe error decoding & 2 MiB boundary check)\n");
    } else {
        terminal_write("Page fault diagnostics: FAIL, code ");
        terminal_write_number((uint32_t)(-pf_res));
        terminal_putchar('\n');
    }
}


void kernel_main(uint64_t magic, uint64_t mb_info_addr)
{
    debug_init();
    debug_puts("[JARVIS-OS] Early serial debug online.\n");

    terminal_initialize();
    multiboot_dump_memory_map(magic, mb_info_addr);

    heap_init();
    test_page_allocator();

    int ramdisk_result = test_ramdisk();
    terminal_write("RAM disk test: ");
    if (ramdisk_result == 0)
    {
        terminal_write("PASS\n");
        fs_init();
    }
    else
    {
        terminal_write("FAIL, code ");
        terminal_write_number((uint32_t)(-ramdisk_result));
        terminal_putchar('\n');
    }

    terminal_write("Block device invariants: ");
    int bdev_res = test_block_device_invariants();
    if (bdev_res == 0) {
        terminal_write("PASS (bounds, buffers, capacity)\n");
    } else {
        terminal_write("FAIL, code ");
        terminal_write_number((uint32_t)(-bdev_res));
        terminal_putchar('\n');
    }

    terminal_write("Filesystem invariants: ");
    int fs_inv_res = test_fs_invariants();
    if (fs_inv_res == 0) {
        terminal_write("PASS (rollback, truncation, stat, reuse)\n");
    } else {
        terminal_write("FAIL, code ");
        terminal_write_number((uint32_t)(-fs_inv_res));
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

    terminal_write("IPC queue invariants: ");
    int ipc_res = test_ipc_invariants();
    if (ipc_res == 0) {
        terminal_write("PASS (multi-queue, boundaries, ownership, capacity)\n");
    } else {
        terminal_write("FAIL, code ");
        terminal_write_number((uint32_t)(-ipc_res));
        terminal_putchar('\n');
    }

    terminal_write("Synchronization invariants: ");
    int sync_res = test_sync_invariants();
    if (sync_res == 0) {
        terminal_write("PASS (mutex ownership, semaphore bounds, PC, RW)\n");
    } else {
        terminal_write("FAIL, code ");
        terminal_write_number((uint32_t)(-sync_res));
        terminal_putchar('\n');
    }

    process_init();

    scheduler_init();
    scheduler_set_algorithm(SCHEDULER_RR);

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

    terminal_write("Register/stack integrity test starting...\n");
    reg_test_passed = 0;
    reg_test_iterations_1 = 0;
    reg_test_iterations_2 = 0;

    int r1 = process_create((uint64_t)reg_test_task_1, 1);
    int r2 = process_create((uint64_t)reg_test_task_2, 1);
    if (r1 > 0 && r2 > 0)
    {
        scheduler_start();
        if (reg_test_passed == 0 && reg_test_iterations_1 == 3 && reg_test_iterations_2 == 3)
        {
            terminal_write("Register/stack integrity test: PASS (callee-saved regs & stack canaries preserved)\n");
        }
        else
        {
            terminal_write("Register/stack integrity test: FAIL, code ");
            terminal_write_number((uint32_t)(-reg_test_passed));
            terminal_putchar('\n');
        }
    }
    else
    {
        terminal_write("Register/stack integrity test: task creation failed\n");
    }

    terminal_write("PIT timer preemption test starting (tasks do not yield)...\n");

    scheduler_stats_t stats_pre;
    scheduler_get_stats(&stats_pre);
    p1_count = 0;
    p2_count = 0;

    int p1 = process_create((uint64_t)preempt_task_1, 1);
    int p2 = process_create((uint64_t)preempt_task_2, 1);
    if (p1 > 0 && p2 > 0)
    {
        scheduler_start();
        scheduler_stats_t stats_post;
        scheduler_get_stats(&stats_post);

        if (p1_count > 0 && p2_count > 0 && stats_post.quantum_preemptions > stats_pre.quantum_preemptions)
        {
            terminal_write("PIT timer preemption test: PASS (involuntary timer preemption verified)\n");
        }
        else
        {
            terminal_write("PIT timer preemption test: FAIL (preemption condition not met)\n");
        }
    }
    else
    {
        terminal_write("PIT timer preemption test: task creation failed\n");
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
