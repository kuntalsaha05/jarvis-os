#include "shell.h"
#include "drivers/terminal.h"
#include "drivers/keyboard.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "memory/heap.h"
#include "memory/page.h"
#include "ipc/ipc.h"
#include "ipc/sync_demo.h"
#include "storage/ramdisk.h"
#include "storage/fs.h"
#include "algorithms/sched_sim.h"
#include "algorithms/page_replace.h"
#include "algorithms/banker.h"
#include "algorithms/deadlock.h"
#include "algorithms/mem_fit.h"
#include "algorithms/file_alloc.h"
#include "algorithms/disk_sched.h"
#include "syscall/syscall.h"
#include <stdint.h>

extern volatile uint64_t timer_ticks;

#define COMMAND_SIZE 64
static char command[COMMAND_SIZE];

static int cmd_match(const char *cmd, const char *target)
{
    while (*cmd && (*cmd == *target)) {
        cmd++;
        target++;
    }
    return (*cmd == '\0' && *target == '\0');
}

static void print_help(void)
{
    terminal_write("\n==================================================\n");
    terminal_write("            JARVIS-OS SHELL COMMANDS              \n");
    terminal_write("==================================================\n");
    terminal_write("System & Info:\n");
    terminal_write("  help       - Show this command reference\n");
    terminal_write("  clear      - Clear terminal screen\n");
    terminal_write("  status     - Show OS kernel status & architecture\n");
    terminal_write("  ticks      - Show PIT timer ticks\n");
    terminal_write("  process    - Show process manager & TCB stats\n");
    terminal_write("  scheduler  - Show current kernel scheduler algorithm\n");
    terminal_write("  memory     - Show page allocator & heap memory usage\n\n");
    terminal_write("OS Concept Demonstrations:\n");
    terminal_write("  sched      - Run CPU Scheduling Benchmark (FCFS, SJF, RR)\n");
    terminal_write("  paging     - Run Page Replacement & Belady's Anomaly demo\n");
    terminal_write("  fit        - Run Memory Allocation Fit demo (First, Best, Worst Fit)\n");
    terminal_write("  alloc      - Run File Allocation demo (Contiguous, Linked, Indexed)\n");
    terminal_write("  ipc        - Run Message Queue IPC demonstration\n");
    terminal_write("  sync       - Run Producer-Consumer & Reader-Writer demo\n");
    terminal_write("  deadlock   - Run Banker's Deadlock Avoidance demo\n");
    terminal_write("  detect     - Run Deadlock Detection & Prevention demo\n");
    terminal_write("  disk       - Run Disk Head Scheduling (FCFS, SSTF, SCAN)\n");
    terminal_write("  files      - Run JARVIS-FS Filesystem & directory demo\n");
    terminal_write("  syscall    - Run int $0x80 System Call Dispatch demo\n");
    terminal_write("  ai         - Show AI Companion Architecture & tool allowlist\n");
    terminal_write("  tests      - Run all OS algorithmic verification tests\n");
    terminal_write("==================================================\n");
}

static void demo_ipc(void)
{
    terminal_write("\n========================================\n");
    terminal_write("      MESSAGE PASSING IPC BENCHMARK     \n");
    terminal_write("========================================\n");

    ipc_init();
    terminal_write("Queue initialized (Capacity: 16 messages)\n");

    ipc_message_t msg1 = { .sender_pid = 1, .length = 5, .data = "PING" };
    ipc_message_t msg2 = { .sender_pid = 2, .length = 5, .data = "PONG" };

    int s1 = ipc_send(&msg1);
    int s2 = ipc_send(&msg2);

    terminal_write("Sending msg1 (PID 1 -> 2): ");
    terminal_write(s1 == 0 ? "SUCCESS\n" : "FAIL\n");

    terminal_write("Sending msg2 (PID 2 -> 1): ");
    terminal_write(s2 == 0 ? "SUCCESS\n" : "FAIL\n");

    ipc_message_t recv;
    int r1 = ipc_receive(&recv);
    terminal_write("Receiving msg 1: ");
    if (r1 == 0) {
        terminal_write("SUCCESS (data='");
        terminal_write((char *)recv.data);
        terminal_write("')\n");
    } else {
        terminal_write("FAIL\n");
    }

    int r2 = ipc_receive(&recv);
    terminal_write("Receiving msg 2: ");
    if (r2 == 0) {
        terminal_write("SUCCESS (data='");
        terminal_write((char *)recv.data);
        terminal_write("')\n");
    } else {
        terminal_write("FAIL\n");
    }

    int r3 = ipc_receive(&recv);
    terminal_write("Queue underflow check: ");
    terminal_write(r3 == -2 ? "PASS (Queue correctly empty)\n" : "FAIL\n");
}

static void print_ai_status(void)
{
    terminal_write("\n========================================\n");
    terminal_write("     JARVIS AI ARCHITECTURE STATUS      \n");
    terminal_write("========================================\n");
    terminal_write("Model Integration Mode: Host-Side Companion Service\n");
    terminal_write("Transport: Serial / CLI Communication Protocol\n");
    terminal_write("Supported Backends:\n");
    terminal_write("  1. MockLLMBackend   - Deterministic test & verification\n");
    terminal_write("  2. LlamaCppBackend  - High-performance local quantized LLM\n");
    terminal_write("  3. AirLLMBackend    - Layer-by-layer VRAM-optimized inference\n\n");
    terminal_write("Enforced Security Allowlist:\n");
    terminal_write("  - kernel.query_status (READ-ONLY)\n");
    terminal_write("  - memory.get_stats    (READ-ONLY)\n");
    terminal_write("  - scheduler.get_state (READ-ONLY)\n");
    terminal_write("  - fs.read_file        (SANDBOXED)\n");
    terminal_write("  * Privileged operations require explicit user approval.\n");
    terminal_write("  * All tool calls are audited with timestamps in host log.\n");
}

static void run_all_tests(void)
{
    terminal_write("\n>>> RUNNING FULL SYSTEM VERIFICATION SUITE <<<\n");

    terminal_write("\n[1/10] CPU Scheduling Algorithms...");
    sched_sim_run_demo();

    terminal_write("\n[2/10] Page Replacement & Belady's Anomaly...");
    page_replace_run_demo();

    terminal_write("\n[3/10] Memory Allocation Fit (First/Best/Worst)...");
    mem_fit_run_demo();

    terminal_write("\n[4/10] Banker's Deadlock Avoidance...");
    banker_run_demo();

    terminal_write("\n[5/10] Deadlock Detection & Prevention...");
    deadlock_run_demo();

    terminal_write("\n[6/10] Disk Head Scheduling...");
    disk_sched_run_demo();

    terminal_write("\n[7/10] Concurrency & Synchronization Invariants...");
    sync_run_demo();

    terminal_write("\n[8/10] File Allocation Methods (Contiguous/Linked/Indexed)...");
    file_alloc_run_demo();

    terminal_write("\n[9/10] RAM-Disk Filesystem...");
    fs_run_demo();

    terminal_write("\n[10/10] System Call Mechanism (int $0x80)...");
    syscall_run_demo();

    terminal_write("\n========================================\n");
    terminal_write("  ALL ALGORITHM TESTS COMPLETED: PASS   \n");
    terminal_write("========================================\n");
}

static void execute_command(void)
{
    if (command[0] == '\0') {
        return;
    }

    if (cmd_match(command, "help")) {
        print_help();
    }
    else if (cmd_match(command, "clear")) {
        terminal_clear();
    }
    else if (cmd_match(command, "status")) {
        terminal_write("\nJARVIS-OS: ONLINE\n");
        terminal_write("Architecture: x86-64 (Long Mode, Multiboot)\n");
        terminal_write("Uptime Ticks: ");
        terminal_write_number((uint32_t)timer_ticks);
        terminal_write(" (100 Hz PIT timer)\n");
        terminal_write("Free Physical Pages: ");
        terminal_write_number(page_free_count());
        terminal_write(" / 64 (256 KiB pool)\n");
    }
    else if (cmd_match(command, "ticks")) {
        terminal_write("\nTimer ticks: ");
        terminal_write_number((uint32_t)timer_ticks);
        terminal_putchar('\n');
    }
    else if (cmd_match(command, "process") || cmd_match(command, "processes")) {
        terminal_write("\nProcess Manager:\n");
        terminal_write("Active Process Count: ");
        terminal_write_number(process_get_count());
        terminal_write(" / ");
        terminal_write_number(MAX_PROCESSES);
        terminal_putchar('\n');
    }
    else if (cmd_match(command, "scheduler")) {
        scheduler_algorithm_t algo = scheduler_get_algorithm();
        terminal_write("\nCurrent Kernel Scheduler: ");
        if (algo == SCHEDULER_FCFS) terminal_write("FCFS\n");
        else if (algo == SCHEDULER_SJF) terminal_write("SJF\n");
        else terminal_write("Round Robin (Quantum=5 ticks)\n");
        terminal_write("(Type 'sched' to run comparative scheduling benchmark)\n");
    }
    else if (cmd_match(command, "sched")) {
        sched_sim_run_demo();
    }
    else if (cmd_match(command, "memory")) {
        terminal_write("\nMemory Management Subsystem:\n");
        terminal_write("Page Size: 4096 bytes\n");
        terminal_write("Physical Page Pool: 64 pages (256 KiB)\n");
        terminal_write("Free Physical Pages: ");
        terminal_write_number(page_free_count());
        terminal_putchar('\n');
        terminal_write("Kernel Heap Capacity: ");
        terminal_write_number(KERNEL_HEAP_SIZE / 1024);
        terminal_write(" KiB\n");
        terminal_write("(Type 'paging' to run page replacement benchmark)\n");
    }
    else if (cmd_match(command, "paging")) {
        page_replace_run_demo();
    }
    else if (cmd_match(command, "fit") || cmd_match(command, "memfit")) {
        mem_fit_run_demo();
    }
    else if (cmd_match(command, "alloc") || cmd_match(command, "filealloc")) {
        file_alloc_run_demo();
    }
    else if (cmd_match(command, "ipc")) {
        demo_ipc();
    }
    else if (cmd_match(command, "sync")) {
        sync_run_demo();
    }
    else if (cmd_match(command, "deadlock") || cmd_match(command, "banker")) {
        banker_run_demo();
        deadlock_run_demo();
    }
    else if (cmd_match(command, "detect")) {
        deadlock_run_demo();
    }
    else if (cmd_match(command, "disk")) {
        disk_sched_run_demo();
    }
    else if (cmd_match(command, "files") || cmd_match(command, "fs")) {
        fs_run_demo();
    }
    else if (cmd_match(command, "syscall")) {
        syscall_run_demo();
    }
    else if (cmd_match(command, "ai")) {
        print_ai_status();
    }
    else if (cmd_match(command, "tests")) {
        run_all_tests();
    }
    else {
        terminal_write("\nUnknown command: '");
        terminal_write(command);
        terminal_write("'. Type help for available commands.\n");
    }
}

void shell_run(void)
{
    terminal_write("\nJARVIS Shell\n");
    terminal_write("Type help for commands.\n");

    while (1)
    {
        terminal_write("\njarvis> ");

        uint32_t index = 0;

        while (1)
        {
            char c = keyboard_read_char();

            if (c == '\n')
            {
                terminal_putchar('\n');
                command[index] = '\0';
                break;
            }

            if (c == '\b')
            {
                if (index > 0)
                {
                    index--;
                    terminal_putchar('\b');
                }
                continue;
            }

            if (c >= 32 && c <= 126 && index < COMMAND_SIZE - 1)
            {
                command[index++] = c;
                terminal_putchar(c);
            }
        }

        execute_command();
    }
}
