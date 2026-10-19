#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdint.h>
#include <stddef.h>

#define JARVIS_KERNEL_NAME    "JARVIS-OS"
#define JARVIS_KERNEL_VERSION "0.18.0"
#define JARVIS_KERNEL_ARCH    "x86_64"
#define JARVIS_KERNEL_MODE    "Ring 0 Monolithic"

/* Diagnostic Log Severity Levels */
#define DIAG_LEVEL_DEBUG 0
#define DIAG_LEVEL_INFO  1
#define DIAG_LEVEL_WARN  2
#define DIAG_LEVEL_ERROR 3
#define DIAG_LEVEL_PANIC 4

/* Ring Buffer Sizing */
#define DIAG_LOG_CAPACITY 32
#define DIAG_SUBSYS_LEN   12
#define DIAG_MSG_LEN      52

typedef struct {
    uint64_t timestamp_ticks;
    uint8_t  level;
    char     subsystem[DIAG_SUBSYS_LEN];
    char     message[DIAG_MSG_LEN];
} diag_log_entry_t;

typedef struct {
    uint64_t irq0_timer;
    uint64_t irq1_keyboard;
    uint64_t irq12_mouse;
    uint64_t syscalls;
    uint64_t yields;
    uint64_t page_faults;
    uint64_t exceptions;
    uint64_t total_interrupts;
} diag_irq_stats_t;

typedef struct {
    const char *kernel_name;
    const char *kernel_version;
    const char *compiler_version;
    const char *build_date;
    const char *build_time;
    const char *arch;
    uint64_t uptime_ticks;
    diag_irq_stats_t irq_stats;

    /* Scheduler stats */
    uint32_t active_processes;
    uint64_t context_switches;
    uint64_t quantum_preemptions;
    uint64_t voluntary_yields;
    const char *scheduler_algorithm;

    /* Memory stats */
    uint64_t total_frames;
    uint64_t used_frames;
    uint64_t free_frames;
    size_t heap_total;
    size_t heap_used;
    size_t heap_free;
    size_t heap_largest_chunk;

    /* Storage / Filesystem stats */
    uint32_t fs_total_blocks;
    uint32_t fs_used_blocks;
    uint32_t fs_free_blocks;
    uint32_t fs_used_inodes;
    uint32_t fs_free_inodes;
    uint32_t fs_ops;
    uint32_t fs_errors;
    uint32_t fs_rollbacks;

    /* GUI Desktop stats */
    int gui_detected;
    uint32_t screen_width;
    uint32_t screen_height;
    uint32_t screen_bpp;
    int gui_running;
    uint32_t active_windows;
    uint32_t notifications_active;

    /* AI Companion stats */
    const char *ai_protocol_version;
    const char *ai_status;
    uint32_t ai_query_count;

    /* Log Buffer stats */
    uint32_t log_entries_count;
    uint32_t log_overflow_count;
    uint64_t log_total_written;
} diag_system_summary_t;

/* Diagnostic Subsystem Lifecycle */
void diag_init(void);

/* Bounded Logging Interface */
void diag_log(uint8_t level, const char *subsystem, const char *message);
uint32_t diag_log_get_count(void);
uint32_t diag_log_get_overflow(void);
uint64_t diag_log_get_total_written(void);
int diag_log_get_entry(uint32_t index, diag_log_entry_t *out_entry);
void diag_print_dmesg(void);

/* Interrupt and Event Accounting */
void diag_record_irq(uint8_t vector);
void diag_get_irq_stats(diag_irq_stats_t *out);

/* System Telemetry & Reporting */
void diag_get_summary(diag_system_summary_t *out);
void diag_print_summary(void);

/* Invariant Verification for Host and CI Testing */
int test_diagnostics_invariants(void);

#endif /* DIAGNOSTICS_H */
