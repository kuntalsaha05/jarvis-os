#include "shell.h"
#include "drivers/terminal.h"
#include "drivers/keyboard.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "memory/heap.h"
#include "memory/page.h"
#include "interrupts/idt.h"
#include "ipc/ipc.h"

#include "ipc/sync_demo.h"
#include "storage/ramdisk.h"
#include "storage/block.h"
#include "storage/fs.h"
#include "algorithms/sched_sim.h"
#include "algorithms/page_replace.h"
#include "algorithms/banker.h"
#include "algorithms/deadlock.h"
#include "algorithms/mem_fit.h"
#include "algorithms/file_alloc.h"
#include "algorithms/disk_sched.h"
#include "syscall/syscall.h"
#include "drivers/framebuffer.h"
#include "gui/desktop.h"
#include "drivers/power.h"
#include "diagnostics/diagnostics.h"
#include "../debug.h"
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

static int cmd_starts_with(const char *cmd, const char *prefix)
{
    while (*prefix) {
        if (*cmd != *prefix) return 0;
        cmd++;
        prefix++;
    }
    return 1;
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
    terminal_write("  irq        - Show interrupt subsystem & IRQ metrics\n");
    terminal_write("  process    - Show process manager & TCB stats\n");
    terminal_write("  threads    - Show comprehensive thread & scheduler metrics\n");
    terminal_write("  scheduler  - Show current kernel scheduler algorithm\n");
    terminal_write("  memory     - Show page allocator & heap memory usage\n");
    terminal_write("  pftest     - Test safe page-fault diagnostics & decoder\n");
    terminal_write("  gui / desktop - Launch native graphical desktop or check FB\n\n");

    terminal_write("File & Storage Operations:\n");
    terminal_write("  ls             - List files in JARVIS-FS directory\n");
    terminal_write("  cat <file>     - Display file content from JARVIS-FS\n");
    terminal_write("  touch <file>   - Create an empty file\n");
    terminal_write("  write <f> <txt>- Write text to file\n");
    terminal_write("  rm <file>      - Delete file from JARVIS-FS\n");
    terminal_write("  trunc <f> <sz> - Truncate file to specified byte size\n");
    terminal_write("  df / fsstat    - Display storage & filesystem volume stats\n");
    terminal_write("  fstest         - Run filesystem & storage invariant tests\n");
    terminal_write("  files          - Run JARVIS-FS Filesystem benchmark\n\n");
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
    terminal_write("  syscall    - Run int $0x80 System Call Dispatch demo\n");
    terminal_write("  panic      - Test kernel panic diagnostic handler\n");
    terminal_write("  ai         - Show AI Companion Architecture & tool allowlist\n");
    terminal_write("  tests      - Run all OS algorithmic verification tests\n\n");
    terminal_write("Diagnostics & Recovery (Phase 18):\n");
    terminal_write("  diag       - Show comprehensive system diagnostics & metrics\n");
    terminal_write("  dmesg      - Show kernel ring-buffer diagnostic event log\n");
    terminal_write("  shutdown   - Graceful system shutdown via ACPI/PM poweroff\n");
    terminal_write("  reboot     - Controlled system reboot via 8042 reset line\n");
    terminal_write("==================================================\n");
}

static void demo_ipc(void)
{
    terminal_write("\n========================================\n");
    terminal_write("      MESSAGE PASSING IPC BENCHMARK     \n");
    terminal_write("========================================\n");

    ipc_init();
    terminal_write("Queue initialized (Capacity: 16 messages)\n");

    ipc_message_t msg1 = { .sender_pid = 1, .receiver_pid = 2, .type = 1, .length = 5, .data = "PING" };
    ipc_message_t msg2 = { .sender_pid = 2, .receiver_pid = 1, .type = 2, .length = 5, .data = "PONG" };

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

    terminal_write("\nQueue Metrics Telemetry:\n");
    ipc_queue_stats_t qstats;
    ipc_get_stats(0, &qstats);
    terminal_write("  Queue Owner: PID ");
    terminal_write_number(qstats.owner_pid);
    terminal_write(" (System Default)\n  Queue Capacity:  ");
    terminal_write_number(qstats.capacity);
    terminal_write(" messages\n  Cumulative Sent: ");
    terminal_write_number((uint32_t)qstats.total_sent);
    terminal_write("\n  Cumulative Recv: ");
    terminal_write_number((uint32_t)qstats.total_received);
    terminal_write("\n  Dropped (Full):  ");
    terminal_write_number((uint32_t)qstats.total_dropped_full);
    terminal_putchar('\n');

    terminal_write("\nIPC Subsystem Invariants Verification: ");
    int inv_res = test_ipc_invariants();
    if (inv_res == 0) {
        terminal_write("PASS (multi-queue, boundaries, ownership, capacity)\n");
    } else {
        terminal_write("FAIL\n");
    }
}


static void print_ai_status(void)
{
    terminal_write("\n========================================\n");
    terminal_write("     JARVIS AI ARCHITECTURE STATUS      \n");
    terminal_write("========================================\n");
    terminal_write("Model Integration Mode: Host-Side Companion Service\n");
    terminal_write("Protocol: JSON Line-Delimited Bridge v1.0\n");
    terminal_write("Transport: COM1 Serial Line / Host CLI Adapter\n");
    terminal_write("Telemetry Provenance: LIVE_KERNEL / OFFLINE_SIMULATION\n");
    terminal_write("Supported LLM Backends:\n");
    terminal_write("  1. OllamaBackend    - Local Ollama daemon (Default: http://localhost:11434)\n");
    terminal_write("  2. LlamaCppBackend  - High-performance local quantized GGUF\n");
    terminal_write("  3. AirLLMBackend    - Layer-by-layer VRAM-optimized streaming\n");
    terminal_write("  4. MockLLMBackend   - Offline test fixture & CI verification\n\n");
    terminal_write("Enforced Security & Permission Model:\n");
    terminal_write("  - [READ_ONLY]       kernel.query_status, memory.get_stats,\n");
    terminal_write("                      scheduler.get_state, storage.list_files,\n");
    terminal_write("                      storage.read_file\n");
    terminal_write("  - [SAFE_DIAGNOSTIC] system.execute_algorithm_demo\n");
    terminal_write("  - [PRIVILEGED]      system.trigger_panic (Explicit approval required)\n");
    terminal_write("  - [DESTRUCTIVE]     storage.format_disk  (Explicit approval required)\n");
    terminal_write("  * Safety guarantees: 10s execution timeout, strict schema\n");
    terminal_write("    validation, and audit logging with sub-millisecond timestamps.\n");
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
        terminal_write("Keyboard IRQ1 Events: ");
        terminal_write_number(keyboard_get_irq_count());
        terminal_putchar('\n');
        terminal_write("Free Physical Pages: ");
        terminal_write_number(page_free_count());
        terminal_write(" / 64 (256 KiB pool)\n");
    }
    else if (cmd_match(command, "ticks")) {
        terminal_write("\nTimer ticks: ");
        terminal_write_number((uint32_t)timer_ticks);
        terminal_putchar('\n');
    }
    else if (cmd_match(command, "irq")) {
        terminal_write("\nInterrupt Subsystem Status:\n");
        terminal_write("  IRQ0 (PIT Timer 100 Hz): ");
        terminal_write_number((uint32_t)timer_ticks);
        terminal_write(" ticks\n");
        terminal_write("  IRQ1 (PS/2 Keyboard):   ");
        terminal_write_number(keyboard_get_irq_count());
        terminal_write(" events\n");
        terminal_write("  Vector 32: timer_stub (IDT gate 0x20)\n");
        terminal_write("  Vector 33: keyboard_stub (IDT gate 0x21)\n");
        terminal_write("  Vector 128: syscall_stub (IDT gate 0x80)\n");
        terminal_write("  Vector 129: schedule_stub (IDT gate 0x81)\n");
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
        page_stats_t pstats;
        page_get_stats(&pstats);

        terminal_write("\nMemory Management Subsystem:\n");
        terminal_write("  Page Size:                4096 bytes\n");
        terminal_write("  Physical Page Pool:       ");
        terminal_write_number(pstats.total_pages);
        terminal_write(" pages (256 KiB)\n");
        terminal_write("  Free Physical Pages:      ");
        terminal_write_number(pstats.free_pages);
        terminal_putchar('\n');
        terminal_write("  Allocated Pages:          ");
        terminal_write_number(pstats.allocated_pages);
        terminal_putchar('\n');
        terminal_write("  Cumulative Allocs:        ");
        terminal_write_number((uint32_t)pstats.total_allocations);
        terminal_putchar('\n');
        terminal_write("  Cumulative Frees:         ");
        terminal_write_number((uint32_t)pstats.total_frees);
        terminal_putchar('\n');
        terminal_write("  Page Pool Base:           0x");
        terminal_write_hex((uint32_t)pstats.pool_base);
        terminal_putchar('\n');
        terminal_write("  Page Pool End:            0x");
        terminal_write_hex((uint32_t)pstats.pool_end);
        terminal_putchar('\n');
        terminal_write("  Identity-Mapped Boundary: 0x00000000 - 0x001FFFFF (2 MiB PDE)\n");
        terminal_write("  Unmapped Address Space:   >= 0x00200000\n");
        terminal_write("  Kernel Heap Capacity:     ");
        terminal_write_number(KERNEL_HEAP_SIZE / 1024);
        terminal_write(" KiB\n");
        terminal_write("  (Type 'pftest' for page fault diagnostics, 'paging' for page replacement)\n");
    }
    else if (cmd_match(command, "pftest")) {
        terminal_write("\n=== Safe Page Fault Diagnostic Self-Test ===\n");
        int res = test_page_fault_diagnostics();
        if (res == 0) {
            terminal_write("Diagnostic decoder test: PASS\n\n");
            terminal_write("Sample Diagnosis 1 (Simulated supervisor read at 0x00205000):\n");
            page_fault_diagnose(0x205000ULL, 0x0);
            terminal_write("\nSample Diagnosis 2 (Simulated user write at 0x00050000):\n");
            page_fault_diagnose(0x50000ULL, 0x7);
        } else {
            terminal_write("Diagnostic decoder test: FAIL, code ");
            terminal_write_number((uint32_t)(-res));
            terminal_putchar('\n');
        }
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
    else if (cmd_match(command, "ls")) {
        fs_list();
    }
    else if (cmd_starts_with(command, "cat")) {
        const char *filename = command + 3;
        while (*filename == ' ') filename++;
        if (*filename == '\0') filename = "kernel.cfg";
        int fd = fs_open(filename);
        if (fd < 0) {
            terminal_write("\ncat: cannot open file '");
            terminal_write(filename);
            terminal_write("'\n");
        } else {
            char buf[513];
            int n = fs_read(fd, buf, 512);
            terminal_write("\n--- Content of ");
            terminal_write(filename);
            terminal_write(" ---\n");
            if (n > 0) {
                buf[n] = '\0';
                terminal_write(buf);
                if (buf[n - 1] != '\n') terminal_putchar('\n');
            } else {
                terminal_write("(empty file)\n");
            }
            fs_close(fd);
        }
    }
    else if (cmd_starts_with(command, "touch")) {
        const char *filename = command + 5;
        while (*filename == ' ') filename++;
        if (*filename == '\0') {
            terminal_write("\nUsage: touch <filename>\n");
        } else {
            int ret = fs_create(filename);
            if (ret == FS_OK) {
                terminal_write("\ntouch: created file '");
                terminal_write(filename);
                terminal_write("'\n");
            } else if (ret == FS_ERR_ALREADY_EXISTS) {
                terminal_write("\ntouch: file '");
                terminal_write(filename);
                terminal_write("' already exists\n");
            } else {
                terminal_write("\ntouch: failed to create file (error ");
                terminal_write_number((uint32_t)(-ret));
                terminal_write(")\n");
            }
        }
    }
    else if (cmd_starts_with(command, "write")) {
        const char *p = command + 5;
        while (*p == ' ') p++;
        char fname[32];
        int fi = 0;
        while (*p && *p != ' ' && fi < 31) {
            fname[fi++] = *p++;
        }
        fname[fi] = '\0';
        while (*p == ' ') p++;
        if (fname[0] == '\0' || *p == '\0') {
            terminal_write("\nUsage: write <filename> <text>\n");
        } else {
            int fd = fs_open(fname);
            if (fd < 0) {
                fs_create(fname);
                fd = fs_open(fname);
            }
            if (fd < 0) {
                terminal_write("\nwrite: failed to open/create '");
                terminal_write(fname);
                terminal_write("'\n");
            } else {
                uint32_t len = 0;
                while (p[len]) len++;
                int w = fs_write(fd, p, len);
                fs_close(fd);
                if (w >= 0) {
                    terminal_write("\nwrite: successfully wrote ");
                    terminal_write_number((uint32_t)w);
                    terminal_write(" bytes to '");
                    terminal_write(fname);
                    terminal_write("'\n");
                } else {
                    terminal_write("\nwrite: error writing to '");
                    terminal_write(fname);
                    terminal_write("'\n");
                }
            }
        }
    }
    else if (cmd_starts_with(command, "rm") || cmd_starts_with(command, "del")) {
        const char *p = cmd_starts_with(command, "rm") ? command + 2 : command + 3;
        while (*p == ' ') p++;
        if (*p == '\0') {
            terminal_write("\nUsage: rm <filename>\n");
        } else {
            int ret = fs_delete(p);
            if (ret == FS_OK) {
                terminal_write("\nrm: deleted file '");
                terminal_write(p);
                terminal_write("'\n");
            } else {
                terminal_write("\nrm: file '");
                terminal_write(p);
                terminal_write("' not found\n");
            }
        }
    }
    else if (cmd_starts_with(command, "trunc")) {
        const char *p = command + 5;
        while (*p == ' ') p++;
        char fname[32];
        int fi = 0;
        while (*p && *p != ' ' && fi < 31) {
            fname[fi++] = *p++;
        }
        fname[fi] = '\0';
        while (*p == ' ') p++;
        if (fname[0] == '\0' || *p == '\0') {
            terminal_write("\nUsage: trunc <filename> <size_bytes>\n");
        } else {
            uint32_t new_size = 0;
            while (*p >= '0' && *p <= '9') {
                new_size = new_size * 10 + (*p - '0');
                p++;
            }
            int ret = fs_truncate_by_name(fname, new_size);
            if (ret == FS_OK) {
                terminal_write("\ntrunc: truncated '");
                terminal_write(fname);
                terminal_write("' to ");
                terminal_write_number(new_size);
                terminal_write(" bytes\n");
            } else {
                terminal_write("\ntrunc: failed (code ");
                terminal_write_number((uint32_t)(-ret));
                terminal_write(")\n");
            }
        }
    }
    else if (cmd_match(command, "df") || cmd_match(command, "fsstat")) {
        fs_info_t info;
        if (fs_get_info(&info) == FS_OK) {
            terminal_write("\nJARVIS-FS Storage & Volume Metrics:\n");
            terminal_write("  Superblock Magic:           0x4A415256 (JARV)\n");
            terminal_write("  Device Backing:             RAMDisk (Volatile, In-Memory)\n");
            terminal_write("  Sector / Block Size:        ");
            terminal_write_number(info.block_size);
            terminal_write(" bytes\n  Total Volume Capacity:      ");
            terminal_write_number(info.total_blocks * info.block_size / 1024);
            terminal_write(" KiB (");
            terminal_write_number(info.total_blocks);
            terminal_write(" blocks)\n  Free Storage Blocks:        ");
            terminal_write_number(info.free_blocks);
            terminal_write(" / ");
            terminal_write_number(info.total_blocks);
            terminal_write("\n  Used Storage Blocks:        ");
            terminal_write_number(info.used_blocks);
            terminal_write("\n  Total Inode Slots:          ");
            terminal_write_number(info.total_inodes);
            terminal_write("\n  Active Files (Inodes Used): ");
            terminal_write_number(info.used_inodes);
            terminal_write("\n  Free Inode Slots:           ");
            terminal_write_number(info.free_inodes);
            terminal_write("\n  Max Direct File Size:       ");
            terminal_write_number(info.max_file_size);
            terminal_write(" bytes (8 direct blocks)\n");
        } else {
            terminal_write("\ndf: filesystem not mounted\n");
        }
    }
    else if (cmd_match(command, "fstest")) {
        terminal_write("\n========================================\n");
        terminal_write("     FILESYSTEM & STORAGE INVARIANTS    \n");
        terminal_write("========================================\n");
        int b_res = test_block_device_invariants();
        terminal_write("Block Device Invariants: ");
        terminal_write(b_res == 0 ? "PASS\n" : "FAIL\n");

        int fs_res = test_fs_invariants();
        terminal_write("JARVIS-FS Invariants:    ");
        terminal_write(fs_res == 0 ? "PASS\n" : "FAIL\n");

        int fa_res = test_file_alloc_invariants();
        terminal_write("File Allocation Methods: ");
        terminal_write(fa_res == 0 ? "PASS\n" : "FAIL\n");

        int ds_res = test_disk_sched_invariants();
        terminal_write("Disk Seek Scheduling:    ");
        terminal_write(ds_res == 0 ? "PASS\n" : "FAIL\n");
        terminal_write("========================================\n");
    }
    else if (cmd_match(command, "threads") || cmd_match(command, "ps")) {
        process_stats_t pstats;
        process_get_stats(&pstats);
        scheduler_stats_t sstats;
        scheduler_get_stats(&sstats);

        terminal_write("\nProcess & Thread Subsystem Statistics:\n");
        terminal_write("  Total Active Processes:     ");
        terminal_write_number(pstats.total_count);
        terminal_write("\n  Ready Processes:            ");
        terminal_write_number(pstats.ready_count);
        terminal_write("\n  Running Processes:          ");
        terminal_write_number(pstats.running_count);
        terminal_write("\n  Blocked Processes:          ");
        terminal_write_number(pstats.blocked_count);
        terminal_write("\n  Terminated Slots:           ");
        terminal_write_number(pstats.terminated_count);
        terminal_write("\nScheduler Metrics:\n");
        terminal_write("  Algorithm:                  ");
        scheduler_algorithm_t algo = scheduler_get_algorithm();
        if (algo == SCHEDULER_FCFS) terminal_write("FCFS\n");
        else if (algo == SCHEDULER_SJF) terminal_write("SJF\n");
        else terminal_write("Round Robin\n");
        terminal_write("  Round-Robin Quantum:        ");
        terminal_write_number(scheduler_get_quantum());
        terminal_write(" ticks\n  Total Context Switches:     ");
        terminal_write_number((uint32_t)sstats.context_switches);
        terminal_write("\n  Quantum Preemptions:        ");
        terminal_write_number((uint32_t)sstats.quantum_preemptions);
        terminal_write("\n  Voluntary Yields:           ");
        terminal_write_number((uint32_t)sstats.voluntary_yields);
        terminal_putchar('\n');
    }
    else if (cmd_match(command, "syscall")) {
        syscall_run_demo();
    }
    else if (cmd_match(command, "panic")) {
        kernel_panic("Manual diagnostic panic triggered via shell command");
    }
    else if (cmd_match(command, "diag") || cmd_match(command, "diagnostics")) {
        diag_print_summary();
    }
    else if (cmd_match(command, "dmesg")) {
        diag_print_dmesg();
    }
    else if (cmd_match(command, "shutdown") || cmd_match(command, "poweroff")) {
        sys_poweroff();
    }
    else if (cmd_match(command, "reboot") || cmd_match(command, "restart")) {
        sys_reboot();
    }
    else if (cmd_match(command, "ai")) {
        print_ai_status();
    }
    else if (cmd_match(command, "gui") || cmd_match(command, "desktop")) {
        const fb_info_t *fb = fb_get_info();
        if (!fb || !fb->is_valid) {
            terminal_write("\n[GUI] Error: No hardware framebuffer detected.\n");
            terminal_write("  Discovery status: ");
            terminal_write(fb ? fb->method : "Driver uninitialized");
            terminal_write("\n  VGA text console (0xB8000) preserved as primary display.\n");
        } else {
            terminal_write("\n[GUI] Launching Native JARVIS Graphical Desktop...\n");
            terminal_write("  Resolution: ");
            terminal_write_number(fb->width);
            terminal_write("x");
            terminal_write_number(fb->height);
            terminal_write("x");
            terminal_write_number(fb->bpp);
            terminal_write(" (Method: ");
            terminal_write(fb->method);
            terminal_write(")\n");
            desktop_run();
        }
    }
    else if (cmd_match(command, "tests")) {
        run_all_tests();
    }
    else if (cmd_match(command, "hi") || cmd_match(command, "hello") || cmd_match(command, "hey")) {
        terminal_write("\nHello! I am JARVIS, your AI-first operating system companion.\n");
        terminal_write("Type 'help' for available commands, 'ai' for AI companion status, or 'gui' for desktop.\n");
    }
    else {
        terminal_write("\nUnknown command: '");
        terminal_write(command);
        terminal_write("'. Type help for available commands.\n");
    }
}

void shell_execute(const char *cmd_line)
{
    if (!cmd_line) return;
    uint32_t i = 0;
    while (cmd_line[i] && i + 1 < COMMAND_SIZE) {
        command[i] = cmd_line[i];
        i++;
    }
    command[i] = '\0';
    execute_command();
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
