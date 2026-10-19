#include "diagnostics.h"
#include "../drivers/terminal.h"
#include "../debug.h"

#ifdef HOST_TEST
/* In host test mode, timer_ticks is provided by the test runner */
extern volatile uint64_t timer_ticks;
#else
extern volatile uint64_t timer_ticks;
#include "../process/process.h"
#include "../process/scheduler.h"
#include "../memory/page.h"
#include "../memory/heap.h"
#include "../storage/fs.h"
#include "../drivers/framebuffer.h"
#include "../gui/desktop.h"
#endif

/* Static ring buffer: strictly bounded, no heap allocations */
static diag_log_entry_t diag_ring_buffer[DIAG_LOG_CAPACITY];
static uint32_t diag_log_head = 0;
static uint32_t diag_log_count = 0;
static uint32_t diag_overflow_count = 0;
static uint64_t diag_total_written = 0;

/* Interrupt and Exception telemetry */
static diag_irq_stats_t diag_irq_counters;

static void safe_strncpy(char *dst, const char *src, size_t max_len)
{
    if (!dst || max_len == 0) return;
    size_t i = 0;
    if (src) {
        while (src[i] != '\0' && (i + 1) < max_len) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

void diag_init(void)
{
    diag_log_head = 0;
    diag_log_count = 0;
    diag_overflow_count = 0;
    diag_total_written = 0;

    diag_irq_counters.irq0_timer = 0;
    diag_irq_counters.irq1_keyboard = 0;
    diag_irq_counters.irq12_mouse = 0;
    diag_irq_counters.syscalls = 0;
    diag_irq_counters.yields = 0;
    diag_irq_counters.page_faults = 0;
    diag_irq_counters.exceptions = 0;
    diag_irq_counters.total_interrupts = 0;

    for (size_t i = 0; i < DIAG_LOG_CAPACITY; i++) {
        diag_ring_buffer[i].timestamp_ticks = 0;
        diag_ring_buffer[i].level = DIAG_LEVEL_INFO;
        diag_ring_buffer[i].subsystem[0] = '\0';
        diag_ring_buffer[i].message[0] = '\0';
    }

    diag_log(DIAG_LEVEL_INFO, "KERNEL", "Diagnostic logging ring buffer initialized");
}

void diag_log(uint8_t level, const char *subsystem, const char *message)
{
    uint32_t insert_slot;

    if (diag_log_count < DIAG_LOG_CAPACITY) {
        insert_slot = (diag_log_head + diag_log_count) % DIAG_LOG_CAPACITY;
        diag_log_count++;
    } else {
        /* Bounded overflow: overwrite oldest entry and advance head */
        insert_slot = diag_log_head;
        diag_log_head = (diag_log_head + 1) % DIAG_LOG_CAPACITY;
        diag_overflow_count++;
    }

    diag_ring_buffer[insert_slot].timestamp_ticks = timer_ticks;
    diag_ring_buffer[insert_slot].level = level;
    safe_strncpy(diag_ring_buffer[insert_slot].subsystem, subsystem, DIAG_SUBSYS_LEN);
    safe_strncpy(diag_ring_buffer[insert_slot].message, message, DIAG_MSG_LEN);
    diag_total_written++;

    /* Mirror critical events directly to early serial debug port */
#ifndef HOST_TEST
    static int diag_in_test_mode = 0;
    if (!diag_in_test_mode && level >= DIAG_LEVEL_ERROR) {
        debug_puts("[DIAG] ");
        debug_puts(subsystem ? subsystem : "SYS");
        debug_puts(": ");
        debug_puts(message ? message : "");
        debug_putchar('\n');
    }
#endif
}

uint32_t diag_log_get_count(void)
{
    return diag_log_count;
}

uint32_t diag_log_get_overflow(void)
{
    return diag_overflow_count;
}

uint64_t diag_log_get_total_written(void)
{
    return diag_total_written;
}

int diag_log_get_entry(uint32_t index, diag_log_entry_t *out_entry)
{
    if (!out_entry || index >= diag_log_count) {
        return -1;
    }
    uint32_t actual_slot = (diag_log_head + index) % DIAG_LOG_CAPACITY;
    *out_entry = diag_ring_buffer[actual_slot];
    return 0;
}

void diag_record_irq(uint8_t vector)
{
    diag_irq_counters.total_interrupts++;

    switch (vector) {
        case 32: /* IRQ0 PIT Timer */
            diag_irq_counters.irq0_timer++;
            break;
        case 33: /* IRQ1 Keyboard */
            diag_irq_counters.irq1_keyboard++;
            break;
        case 44: /* IRQ12 Mouse */
            diag_irq_counters.irq12_mouse++;
            break;
        case 0x80: /* INT 0x80 System Call */
            diag_irq_counters.syscalls++;
            break;
        case 0x81: /* INT 0x81 Cooperative Yield */
            diag_irq_counters.yields++;
            break;
        case 14: /* Vector 14 Page Fault */
            diag_irq_counters.page_faults++;
            break;
        default:
            if (vector < 32) {
                diag_irq_counters.exceptions++;
            }
            break;
    }
}

void diag_get_irq_stats(diag_irq_stats_t *out)
{
    if (out) {
        *out = diag_irq_counters;
    }
}

static const char *level_to_str(uint8_t level)
{
    switch (level) {
        case DIAG_LEVEL_DEBUG: return "DEBUG";
        case DIAG_LEVEL_INFO:  return "INFO ";
        case DIAG_LEVEL_WARN:  return "WARN ";
        case DIAG_LEVEL_ERROR: return "ERROR";
        case DIAG_LEVEL_PANIC: return "PANIC";
        default:               return "UNKWN";
    }
}

void diag_print_dmesg(void)
{
    terminal_write("\n==================================================\n");
    terminal_write("        JARVIS-OS KERNEL DIAGNOSTIC LOG (DMESG)   \n");
    terminal_write("==================================================\n");
    terminal_write("Buffer: ");
    terminal_write_number(diag_log_count);
    terminal_write("/");
    terminal_write_number(DIAG_LOG_CAPACITY);
    terminal_write(" entries, Total Written: ");
    terminal_write_number((uint32_t)diag_total_written);
    terminal_write(", Overflows: ");
    terminal_write_number(diag_overflow_count);
    terminal_write("\n--------------------------------------------------\n");

    diag_log_entry_t entry;
    for (uint32_t i = 0; i < diag_log_count; i++) {
        if (diag_log_get_entry(i, &entry) == 0) {
            terminal_write("[");
            terminal_write_number((uint32_t)entry.timestamp_ticks);
            terminal_write("] [");
            terminal_write(level_to_str(entry.level));
            terminal_write("] [");
            terminal_write(entry.subsystem);
            terminal_write("] ");
            terminal_write(entry.message);
            terminal_putchar('\n');
        }
    }
    terminal_write("==================================================\n");
}

void diag_get_summary(diag_system_summary_t *out)
{
    if (!out) return;

    out->kernel_name = JARVIS_KERNEL_NAME;
    out->kernel_version = JARVIS_KERNEL_VERSION;
#ifdef __VERSION__
    out->compiler_version = __VERSION__;
#else
    out->compiler_version = "GCC (Generic)";
#endif
    out->build_date = __DATE__;
    out->build_time = __TIME__;
    out->arch = JARVIS_KERNEL_ARCH;
    out->uptime_ticks = timer_ticks;
    out->irq_stats = diag_irq_counters;

#ifndef HOST_TEST
    /* Scheduler live telemetry */
    scheduler_stats_t sstats;
    scheduler_get_stats(&sstats);
    out->active_processes = (uint32_t)process_get_count();
    out->context_switches = sstats.context_switches;
    out->quantum_preemptions = sstats.quantum_preemptions;
    out->voluntary_yields = sstats.voluntary_yields;
    out->scheduler_algorithm = (scheduler_get_algorithm() == SCHEDULER_RR) ? "Round-Robin (Preemptive)" : "FCFS";

    /* Memory live telemetry */
    page_stats_t pstats;
    page_get_stats(&pstats);
    out->total_frames = pstats.total_pages;
    out->used_frames = pstats.allocated_pages;
    out->free_frames = pstats.free_pages;

    heap_stats_t hstats;
    heap_get_stats(&hstats);
    out->heap_total = hstats.total_size;
    out->heap_used = hstats.used_bytes;
    out->heap_free = hstats.free_bytes;
    out->heap_largest_chunk = hstats.largest_free_chunk;

    /* Filesystem live telemetry */
    fs_info_t fsinfo;
    fs_get_info(&fsinfo);
    out->fs_total_blocks = fsinfo.total_blocks;
    out->fs_used_blocks = fsinfo.used_blocks;
    out->fs_free_blocks = fsinfo.free_blocks;
    out->fs_used_inodes = fsinfo.used_inodes;
    out->fs_free_inodes = fsinfo.free_inodes;
    fs_get_stats(&out->fs_ops, &out->fs_errors, &out->fs_rollbacks);

    /* GUI Desktop live telemetry */
    const fb_info_t *fb = fb_get_info();
    if (fb && fb->is_valid) {
        out->gui_detected = 1;
        out->screen_width = fb->width;
        out->screen_height = fb->height;
        out->screen_bpp = fb->bpp;
    } else {
        out->gui_detected = 0;
        out->screen_width = 0;
        out->screen_height = 0;
        out->screen_bpp = 0;
    }
    const desktop_env_t *env = desktop_get_env();
    if (env) {
        out->gui_running = env->is_running;
        out->active_windows = (uint32_t)wm_get_windows_by_z_order(NULL, 0);
        out->notifications_active = notif_get_active_count();
        out->ai_query_count = env->ai_requests_count;
    } else {
        out->gui_running = 0;
        out->active_windows = 0;
        out->notifications_active = 0;
        out->ai_query_count = 0;
    }

    /* AI Companion telemetry */
    out->ai_protocol_version = "v1.0 (Line-Delimited JSON)";
    out->ai_status = "HOST_CONNECTED_OR_FALLBACK";
#else
    /* Mocked/default values in host test mode */
    out->active_processes = 1;
    out->context_switches = 10;
    out->quantum_preemptions = 4;
    out->voluntary_yields = 6;
    out->scheduler_algorithm = "Round-Robin (Host Test)";
    out->total_frames = 256;
    out->used_frames = 64;
    out->free_frames = 192;
    out->heap_total = 65536;
    out->heap_used = 4096;
    out->heap_free = 61440;
    out->heap_largest_chunk = 61440;
    out->fs_total_blocks = 128;
    out->fs_used_blocks = 12;
    out->fs_free_blocks = 116;
    out->fs_used_inodes = 2;
    out->fs_free_inodes = 14;
    out->fs_ops = 24;
    out->fs_errors = 0;
    out->fs_rollbacks = 0;
    out->gui_detected = 1;
    out->screen_width = 1024;
    out->screen_height = 768;
    out->screen_bpp = 32;
    out->gui_running = 0;
    out->active_windows = 0;
    out->notifications_active = 0;
    out->ai_protocol_version = "v1.0 (Host Test)";
    out->ai_status = "HOST_TEST_MOCK";
    out->ai_query_count = 5;
#endif

    out->log_entries_count = diag_log_count;
    out->log_overflow_count = diag_overflow_count;
    out->log_total_written = diag_total_written;
}

void diag_print_summary(void)
{
    diag_system_summary_t s;
    diag_get_summary(&s);

    terminal_write("\n==================================================\n");
    terminal_write("         JARVIS-OS SYSTEM DIAGNOSTICS             \n");
    terminal_write("==================================================\n");

    terminal_write("Kernel:     ");
    terminal_write(s.kernel_name);
    terminal_write(" ");
    terminal_write(s.kernel_version);
    terminal_write(" (");
    terminal_write(s.arch);
    terminal_write(", ");
    terminal_write(JARVIS_KERNEL_MODE);
    terminal_write(")\nCompiler:   ");
    terminal_write(s.compiler_version);
    terminal_write("\nBuild:      ");
    terminal_write(s.build_date);
    terminal_write(" ");
    terminal_write(s.build_time);
    terminal_write("\nUptime:     ");
    terminal_write_number((uint32_t)s.uptime_ticks);
    terminal_write(" PIT ticks\n");

    terminal_write("\n--- Interrupt & Exception Statistics ---\n");
    terminal_write("  Total Interrupts:  ");
    terminal_write_number((uint32_t)s.irq_stats.total_interrupts);
    terminal_write("\n  IRQ0 (Timer):      ");
    terminal_write_number((uint32_t)s.irq_stats.irq0_timer);
    terminal_write("\n  IRQ1 (Keyboard):   ");
    terminal_write_number((uint32_t)s.irq_stats.irq1_keyboard);
    terminal_write("\n  IRQ12 (Mouse):     ");
    terminal_write_number((uint32_t)s.irq_stats.irq12_mouse);
    terminal_write("\n  Syscalls (0x80):   ");
    terminal_write_number((uint32_t)s.irq_stats.syscalls);
    terminal_write("\n  Yields (0x81):     ");
    terminal_write_number((uint32_t)s.irq_stats.yields);
    terminal_write("\n  Page Faults (#14): ");
    terminal_write_number((uint32_t)s.irq_stats.page_faults);
    terminal_write("\n  CPU Exceptions:    ");
    terminal_write_number((uint32_t)s.irq_stats.exceptions);
    terminal_putchar('\n');

    terminal_write("\n--- Process & Scheduler ---\n");
    terminal_write("  Algorithm:         ");
    terminal_write(s.scheduler_algorithm);
    terminal_write("\n  Active Processes:  ");
    terminal_write_number(s.active_processes);
    terminal_write("\n  Context Switches:  ");
    terminal_write_number((uint32_t)s.context_switches);
    terminal_write("\n  Preemptions:       ");
    terminal_write_number((uint32_t)s.quantum_preemptions);
    terminal_write("\n  Voluntary Yields:  ");
    terminal_write_number((uint32_t)s.voluntary_yields);
    terminal_putchar('\n');

    terminal_write("\n--- Memory Usage ---\n");
    terminal_write("  Physical Frames:   ");
    terminal_write_number((uint32_t)s.used_frames);
    terminal_write("/");
    terminal_write_number((uint32_t)s.total_frames);
    terminal_write(" used (");
    terminal_write_number((uint32_t)s.free_frames);
    terminal_write(" free)\n  Kernel Heap:       ");
    terminal_write_number((uint32_t)s.heap_used);
    terminal_write("/");
    terminal_write_number((uint32_t)s.heap_total);
    terminal_write(" B used (");
    terminal_write_number((uint32_t)s.heap_free);
    terminal_write(" B free, largest ");
    terminal_write_number((uint32_t)s.heap_largest_chunk);
    terminal_write(" B)\n");

    terminal_write("\n--- Storage (JARVIS-FS RAMDisk) ---\n");
    terminal_write("  Blocks:            ");
    terminal_write_number(s.fs_used_blocks);
    terminal_write("/");
    terminal_write_number(s.fs_total_blocks);
    terminal_write(" used (");
    terminal_write_number(s.fs_free_blocks);
    terminal_write(" free)\n  Inodes:            ");
    terminal_write_number(s.fs_used_inodes);
    terminal_write("/");
    terminal_write_number(s.fs_used_inodes + s.fs_free_inodes);
    terminal_write(" used\n  FS Operations:     ");
    terminal_write_number(s.fs_ops);
    terminal_write(" (Errors: ");
    terminal_write_number(s.fs_errors);
    terminal_write(", Rollbacks: ");
    terminal_write_number(s.fs_rollbacks);
    terminal_write(")\n");
    terminal_write("  [Notice: Volatile RAMDisk /dev/ram0 - reset on reboot]\n");

    terminal_write("\n--- Graphical Desktop & AI Subsystem ---\n");
    terminal_write("  Framebuffer:       ");
    if (s.gui_detected) {
        terminal_write_number(s.screen_width);
        terminal_write("x");
        terminal_write_number(s.screen_height);
        terminal_write("x");
        terminal_write_number(s.screen_bpp);
        terminal_write(" bpp\n");
    } else {
        terminal_write("None (VGA text console fallback)\n");
    }
    terminal_write("  GUI State:         ");
    terminal_write(s.gui_running ? "RUNNING\n" : "STOPPED / CONSOLE SHELL\n");
    terminal_write("  Active Windows:    ");
    terminal_write_number(s.active_windows);
    terminal_write(" (Notifications: ");
    terminal_write_number(s.notifications_active);
    terminal_write(")\n  AI Companion:      ");
    terminal_write(s.ai_protocol_version);
    terminal_write(" [Queries: ");
    terminal_write_number(s.ai_query_count);
    terminal_write("]\n");

    terminal_write("\n--- Diagnostic Log Status ---\n");
    terminal_write("  Log Entries:       ");
    terminal_write_number(s.log_entries_count);
    terminal_write("/");
    terminal_write_number(DIAG_LOG_CAPACITY);
    terminal_write("\n  Total Written:     ");
    terminal_write_number((uint32_t)s.log_total_written);
    terminal_write("\n  Overflow Evictions:");
    terminal_write_number(s.log_overflow_count);
    terminal_write("\n==================================================\n");
}

int test_diagnostics_invariants(void)
{
    diag_init();

    /* Check initial state */
    if (diag_log_get_count() != 1) return -1; /* Initial kernel log entry */
    if (diag_log_get_overflow() != 0) return -2;

    /* Fill buffer up to capacity */
    for (int i = 0; i < DIAG_LOG_CAPACITY - 1; i++) {
        diag_log(DIAG_LEVEL_DEBUG, "TEST", "Filling log slot");
    }
    if (diag_log_get_count() != DIAG_LOG_CAPACITY) return -3;
    if (diag_log_get_overflow() != 0) return -4;

    /* Now trigger overflow */
    diag_log(DIAG_LEVEL_WARN, "TEST", "Overflow item 1");
    if (diag_log_get_count() != DIAG_LOG_CAPACITY) return -5;
    if (diag_log_get_overflow() != 1) return -6;

    diag_log(DIAG_LEVEL_ERROR, "TEST", "Overflow item 2");
    if (diag_log_get_count() != DIAG_LOG_CAPACITY) return -7;
    if (diag_log_get_overflow() != 2) return -8;

    /* Check that the latest entry is readable at index (DIAG_LOG_CAPACITY - 1) */
    diag_log_entry_t last_entry;
    if (diag_log_get_entry(DIAG_LOG_CAPACITY - 1, &last_entry) != 0) return -9;
    if (last_entry.level != DIAG_LEVEL_ERROR) return -10;

    /* Verify IRQ accounting */
    diag_record_irq(32);
    diag_record_irq(32);
    diag_record_irq(33);
    diag_record_irq(44);
    diag_record_irq(0x80);
    diag_record_irq(14);
    diag_record_irq(6);

    diag_irq_stats_t irq;
    diag_get_irq_stats(&irq);
    if (irq.irq0_timer != 2) return -11;
    if (irq.irq1_keyboard != 1) return -12;
    if (irq.irq12_mouse != 1) return -13;
    if (irq.syscalls != 1) return -14;
    if (irq.page_faults != 1) return -15;
    if (irq.exceptions != 1) return -16;
    if (irq.total_interrupts != 7) return -17;

    return 0;
}
