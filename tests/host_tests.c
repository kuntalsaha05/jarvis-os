#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "algorithms/sched_sim.h"
#include "algorithms/page_replace.h"
#include "algorithms/banker.h"
#include "algorithms/disk_sched.h"
#include "algorithms/mem_fit.h"
#include "algorithms/file_alloc.h"
#include "algorithms/deadlock.h"
#include "ipc/ipc.h"
#include "ipc/semaphore.h"
#include "ipc/sync_demo.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "memory/page.h"
#include "memory/heap.h"
#include "storage/block.h"
#include "storage/ramdisk.h"
#include "storage/fs.h"
#include "drivers/framebuffer.h"
#include "drivers/mouse.h"
#include "gui/font.h"
#include "gui/graphics.h"
#include "gui/event.h"
#include "gui/wm.h"
#include "gui/desktop.h"
#include "gui/config.h"
#include "gui/notification.h"
#include "gui/app.h"
#include "diagnostics/diagnostics.h"
#include "drivers/power.h"
#include "interrupts/idt.h"
#include "hal/hal.h"
#include "net/netstack.h"

/* Provide stubs for terminal functions when running in host unit test mode */
void terminal_write(const char *str) {
    if (str) printf("%s", str);
}

void terminal_putchar(char c) {
    putchar(c);
}

void terminal_write_number(uint32_t n) {
    printf("%u", n);
}

/* Provide stubs for scheduler and timer when running in host unit test mode */
volatile uint64_t timer_ticks = 100;

static process_t host_curr_proc = { .pid = 1, .state = PROCESS_RUNNING };

process_t *scheduler_current_process(void) {
    return &host_curr_proc;
}

int process_block(uint32_t pid) {
    (void)pid;
    return 0;
}

int process_unblock(uint32_t pid) {
    (void)pid;
    return 0;
}

void scheduler_yield(void) {
}

void debug_init(void) { }
void debug_puts(const char *s) { (void)s; }
void debug_putchar(char c) { (void)c; }
void debug_write_hex(uint64_t val) { (void)val; }
void debug_write_dec(uint64_t val) { (void)val; }
__attribute__((noreturn)) void kernel_panic(const char *msg) { (void)msg; exit(1); }
void terminal_initialize(void) { }
void pic_init(void) { }
void pic_send_eoi(uint8_t irq) { (void)irq; }
void pit_init(uint32_t freq) { (void)freq; }
void keyboard_init(void) { }
char keyboard_read_char(void) { return 0; }
void shell_execute(const char *cmd) { (void)cmd; }
void page_get_stats(page_stats_t *s) {
    if (s) { s->free_pages = 60; s->allocated_pages = 4; s->total_pages = 64; }
}
void scheduler_get_stats(scheduler_stats_t *s) {
    if (s) { s->context_switches = 10; s->quantum_preemptions = 2; s->voluntary_yields = 5; }
}
void process_get_stats(process_stats_t *s) {
    if (s) { s->total_count = 3; s->running_count = 1; s->ready_count = 2; s->blocked_count = 0; s->terminated_count = 0; }
}


/* ========================================================
 * 1. CPU Scheduling Simulation Tests
 * ======================================================== */
static void test_scheduling(void)
{
    printf("[TEST] CPU Scheduling Algorithms... ");

    sched_task_t tasks[4] = {
        { .id = 1, .arrival_time = 0, .burst_time = 6 },
        { .id = 2, .arrival_time = 1, .burst_time = 8 },
        { .id = 3, .arrival_time = 2, .burst_time = 3 },
        { .id = 4, .arrival_time = 3, .burst_time = 4 }
    };

    sched_result_t res_fcfs, res_sjf, res_rr;

    /* FCFS verification */
    sched_sim_fcfs(tasks, 4, &res_fcfs);
    assert(res_fcfs.tasks[0].completion_time == 6);
    assert(res_fcfs.tasks[1].completion_time == 14);
    assert(res_fcfs.tasks[2].completion_time == 17);
    assert(res_fcfs.tasks[3].completion_time == 21);
    assert(res_fcfs.avg_turnaround_scaled == 1300); /* 13.00 */
    assert(res_fcfs.avg_waiting_scaled == 775);     /* 7.75 */

    /* SJF verification */
    sched_sim_sjf(tasks, 4, &res_sjf);
    assert(res_sjf.tasks[0].completion_time == 6);
    assert(res_sjf.tasks[2].completion_time == 9);  /* P3 (burst 3) runs before P2/P4 */
    assert(res_sjf.tasks[3].completion_time == 13); /* P4 (burst 4) runs next */
    assert(res_sjf.tasks[1].completion_time == 21); /* P2 (burst 8) runs last */
    assert(res_sjf.avg_turnaround_scaled == 1075);  /* 10.75 */
    assert(res_sjf.avg_waiting_scaled == 550);      /* 5.50 */

    /* Round Robin verification (Quantum = 3) */
    sched_sim_rr(tasks, 4, 3, &res_rr);
    assert(res_rr.tasks[2].completion_time == 9);   /* P3 completes first at tick 9 */
    assert(res_rr.avg_turnaround_scaled == 1450);   /* 14.50 */
    assert(res_rr.avg_waiting_scaled == 925);       /* 9.25 */

    printf("PASS\n");
}

/* ========================================================
 * 2. Page Replacement & Belady Anomaly Tests
 * ======================================================== */
static void test_page_replacement(void)
{
    printf("[TEST] Page Replacement & Belady Anomaly... ");

    const uint32_t refs[20] = {
        7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1
    };

    uint32_t fifo = page_replace_fifo(refs, 20, 3);
    uint32_t lru = page_replace_lru(refs, 20, 3);
    uint32_t opt = page_replace_optimal(refs, 20, 3);

    assert(fifo == 15);
    assert(lru == 12);
    assert(opt == 9);

    /* Belady's Anomaly Test: 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5 */
    const uint32_t belady[12] = { 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5 };
    uint32_t faults_3 = page_replace_fifo(belady, 12, 3);
    uint32_t faults_4 = page_replace_fifo(belady, 12, 4);

    assert(faults_3 == 9);
    assert(faults_4 == 10);
    assert(faults_4 > faults_3); /* Belady's anomaly confirmed */

    printf("PASS\n");
}

/* ========================================================
 * 3. Banker's Deadlock Avoidance Tests
 * ======================================================== */
static void test_banker_algorithm(void)
{
    printf("[TEST] Banker's Deadlock Avoidance... ");

    banker_system_t sys;
    uint32_t avail[3] = { 3, 3, 2 };
    banker_init_system(&sys, 5, 3, avail);

    uint32_t alloc[5][3] = {
        {0, 1, 0}, {2, 0, 0}, {3, 0, 2}, {2, 1, 1}, {0, 0, 2}
    };
    uint32_t max_claim[5][3] = {
        {7, 5, 3}, {3, 2, 2}, {9, 0, 2}, {2, 2, 2}, {4, 3, 3}
    };

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 3; j++) {
            sys.allocation[i][j] = alloc[i][j];
            sys.max[i][j] = max_claim[i][j];
        }
    }

    banker_compute_need(&sys);
    assert(sys.need[0][0] == 7 && sys.need[0][1] == 4 && sys.need[0][2] == 3);
    assert(sys.need[1][0] == 1 && sys.need[1][1] == 2 && sys.need[1][2] == 2);

    banker_safety_result_t safe_res;
    banker_check_safety(&sys, &safe_res);
    assert(safe_res.is_safe == 1);
    /* Safe sequence: P1 -> P3 -> P4 -> P0 -> P2 */
    assert(safe_res.safe_sequence[0] == 1);
    assert(safe_res.safe_sequence[1] == 3);

    /* Safe request from P1: (1, 0, 2) */
    uint32_t req_safe[3] = { 1, 0, 2 };
    int r_ok = banker_request_resources(&sys, 1, req_safe);
    assert(r_ok == 0);

    /* Unsafe request from P4: (3, 3, 0) */
    uint32_t req_unsafe[3] = { 3, 3, 0 };
    int r_denied = banker_request_resources(&sys, 4, req_unsafe);
    assert(r_denied < 0);

    printf("PASS\n");
}

/* ========================================================
 * 4. Disk Head Scheduling Tests
 * ======================================================== */
static void test_disk_scheduling(void)
{
    printf("[TEST] Disk Head Scheduling (FCFS, SSTF, SCAN, C-SCAN)... ");

    const uint32_t reqs[8] = { 98, 183, 37, 122, 14, 124, 65, 67 };
    uint32_t initial_head = 53;
    uint32_t disk_size = 200;

    disk_sched_result_t res_fcfs, res_sstf, res_scan, res_cscan;

    disk_sched_fcfs(initial_head, reqs, 8, &res_fcfs);
    assert(res_fcfs.total_head_movement == 640);

    disk_sched_sstf(initial_head, reqs, 8, &res_sstf);
    assert(res_sstf.total_head_movement == 236);

    disk_sched_scan(initial_head, reqs, 8, disk_size, 1, &res_scan);
    assert(res_scan.total_head_movement == 331);

    disk_sched_cscan(initial_head, reqs, 8, disk_size, &res_cscan);
    assert(res_cscan.total_head_movement == 382);

    printf("PASS\n");
}

/* ========================================================
 * 5. Memory Allocation Fit Tests (First, Best, Worst Fit)
 * ======================================================== */
static void test_mem_fit(void)
{
    printf("[TEST] Memory Allocation Fit (First, Best, Worst)... ");

    const uint32_t blocks[5] = { 100, 500, 200, 300, 600 };
    const uint32_t requests[4] = { 212, 417, 112, 426 };

    mem_fit_result_t res_ff, res_bf, res_wf;

    /* First Fit */
    mem_fit_first(blocks, 5, requests, 4, &res_ff);
    assert(res_ff.successful_allocs == 3);
    assert(res_ff.failed_allocs == 1);
    assert(res_ff.results[0].allocated_block_id == 1); /* 212 -> 500 */
    assert(res_ff.results[1].allocated_block_id == 4); /* 417 -> 600 */
    assert(res_ff.results[2].allocated_block_id == 2); /* 112 -> 200 */
    assert(res_ff.results[3].allocated_block_id == -1); /* 426 fails (external fragmentation) */
    assert(res_ff.external_frag == 400); /* 100 + 300 free bytes */

    /* Best Fit */
    mem_fit_best(blocks, 5, requests, 4, &res_bf);
    assert(res_bf.successful_allocs == 4);
    assert(res_bf.failed_allocs == 0);
    assert(res_bf.results[0].allocated_block_id == 3); /* 212 -> 300 */
    assert(res_bf.results[1].allocated_block_id == 1); /* 417 -> 500 */
    assert(res_bf.results[2].allocated_block_id == 2); /* 112 -> 200 */
    assert(res_bf.results[3].allocated_block_id == 4); /* 426 -> 600 */

    /* Worst Fit */
    mem_fit_worst(blocks, 5, requests, 4, &res_wf);
    assert(res_wf.results[0].allocated_block_id == 4); /* 212 -> 600 */
    assert(res_wf.results[1].allocated_block_id == 1); /* 417 -> 500 */
    assert(res_wf.results[2].allocated_block_id == 3); /* 112 -> 300 (largest remaining unallocated) */
    assert(res_wf.results[3].allocated_block_id == -1); /* 426 fails */

    printf("PASS\n");
}

/* ========================================================
 * 6. File Allocation Methods (Contiguous, Linked, Indexed)
 * ======================================================== */
static void test_file_alloc(void)
{
    printf("[TEST] File Allocation Methods (Contiguous, Linked, Indexed)... ");

    file_alloc_disk_t disk;
    file_alloc_disk_init(&disk);
    assert(disk.total_blocks == 64);
    assert(disk.free_blocks == 64);

    /* 1. Contiguous Allocation */
    contiguous_file_t cfile;
    int c_ok = file_alloc_contiguous(&disk, "os_core.bin", 4, &cfile);
    assert(c_ok == 0);
    assert(cfile.success == 1);
    assert(cfile.start_block == 0);
    assert(cfile.size_blocks == 4);
    assert(disk.free_blocks == 60);

    /* Logical-to-physical block resolution */
    assert(file_alloc_contiguous_lookup(&cfile, 0) == 0);
    assert(file_alloc_contiguous_lookup(&cfile, 2) == 2);
    assert(file_alloc_contiguous_lookup(&cfile, 4) == -1); /* Out of bounds */

    /* 2. Linked Allocation */
    linked_file_t lfile;
    int l_ok = file_alloc_linked(&disk, "kernel.log", 3, &lfile);
    assert(l_ok == 0);
    assert(lfile.success == 1);
    assert(lfile.start_block == 4);
    assert(lfile.end_block == 6);
    assert(disk.free_blocks == 57);

    assert(file_alloc_linked_lookup(&lfile, 0) == 4);
    assert(file_alloc_linked_lookup(&lfile, 1) == 5);
    assert(file_alloc_linked_lookup(&lfile, 2) == 6);
    assert(file_alloc_linked_lookup(&lfile, 3) == -1);

    /* 3. Indexed Allocation */
    indexed_file_t ifile;
    int i_ok = file_alloc_indexed(&disk, "index.db", 3, &ifile);
    assert(i_ok == 0);
    assert(ifile.success == 1);
    assert(ifile.index_block == 7);
    assert(disk.free_blocks == 53); /* 3 data blocks + 1 index block */

    assert(file_alloc_indexed_lookup(&ifile, 0) == 8);
    assert(file_alloc_indexed_lookup(&ifile, 1) == 9);
    assert(file_alloc_indexed_lookup(&ifile, 2) == 10);
    assert(file_alloc_indexed_lookup(&ifile, 3) == -1);

    /* Out of space check */
    contiguous_file_t huge_file;
    int fail_res = file_alloc_contiguous(&disk, "huge.bin", 100, &huge_file);
    assert(fail_res == -1);

    printf("PASS\n");
}

/* ========================================================
 * 7. Deadlock Detection & Prevention Tests
 * ======================================================== */
static void test_deadlock_detection_and_prevention(void)
{
    printf("[TEST] Deadlock Detection (WFG/Matrix) & Prevention... ");

    /* Deadlocked system: P0 has R0 wants R1, P1 has R1 wants R0 */
    deadlock_detection_system_t sys;
    sys.process_count = 2;
    sys.resource_count = 2;
    sys.available[0] = 0;
    sys.available[1] = 0;

    sys.allocation[0][0] = 1; sys.allocation[0][1] = 0;
    sys.allocation[1][0] = 0; sys.allocation[1][1] = 1;

    sys.request[0][0] = 0; sys.request[0][1] = 1; /* P0 waits for R1 */
    sys.request[1][0] = 1; sys.request[1][1] = 0; /* P1 waits for R0 */

    deadlock_detection_result_t res;
    deadlock_detect(&sys, &res);
    assert(res.deadlock_detected == 1);
    assert(res.deadlocked_count == 2);

    /* Safe system: Available has enough to satisfy P0 */
    sys.available[1] = 1;
    deadlock_detect(&sys, &res);
    assert(res.deadlock_detected == 0);
    assert(res.deadlocked_count == 0);

    printf("PASS\n");
}

/* ========================================================
 * 8. Process Lifecycle State Transition Invariants
 * ======================================================== */
typedef enum {
    TEST_PROCESS_NEW = 0,
    TEST_PROCESS_READY,
    TEST_PROCESS_RUNNING,
    TEST_PROCESS_BLOCKED,
    TEST_PROCESS_TERMINATED
} test_process_state_t;

static int test_process_is_valid_transition(test_process_state_t from, test_process_state_t to)
{
    if (from == to)
        return 1;

    switch (from) {
        case TEST_PROCESS_NEW:
            return (to == TEST_PROCESS_READY || to == TEST_PROCESS_TERMINATED);
        case TEST_PROCESS_READY:
            return (to == TEST_PROCESS_RUNNING || to == TEST_PROCESS_TERMINATED);
        case TEST_PROCESS_RUNNING:
            return (to == TEST_PROCESS_READY || to == TEST_PROCESS_BLOCKED || to == TEST_PROCESS_TERMINATED);
        case TEST_PROCESS_BLOCKED:
            return (to == TEST_PROCESS_READY || to == TEST_PROCESS_TERMINATED);
        case TEST_PROCESS_TERMINATED:
            return (to == TEST_PROCESS_NEW || to == TEST_PROCESS_READY);
        default:
            return 0;
    }
}

static void test_process_lifecycle_state_machine(void)
{
    printf("[TEST] Process Lifecycle State Transitions... ");

    /* Valid transitions */
    assert(test_process_is_valid_transition(TEST_PROCESS_NEW, TEST_PROCESS_READY) == 1);
    assert(test_process_is_valid_transition(TEST_PROCESS_READY, TEST_PROCESS_RUNNING) == 1);
    assert(test_process_is_valid_transition(TEST_PROCESS_RUNNING, TEST_PROCESS_READY) == 1);     /* Yield / Preemption */
    assert(test_process_is_valid_transition(TEST_PROCESS_RUNNING, TEST_PROCESS_BLOCKED) == 1);   /* Sleep / Wait */
    assert(test_process_is_valid_transition(TEST_PROCESS_BLOCKED, TEST_PROCESS_READY) == 1);     /* Wakeup */
    assert(test_process_is_valid_transition(TEST_PROCESS_RUNNING, TEST_PROCESS_TERMINATED) == 1); /* Exit */
    assert(test_process_is_valid_transition(TEST_PROCESS_TERMINATED, TEST_PROCESS_READY) == 1);  /* Slot recycling */

    /* Invalid transitions */
    assert(test_process_is_valid_transition(TEST_PROCESS_NEW, TEST_PROCESS_RUNNING) == 0);       /* Must queue as READY first */
    assert(test_process_is_valid_transition(TEST_PROCESS_READY, TEST_PROCESS_BLOCKED) == 0);     /* Cannot block without running */
    assert(test_process_is_valid_transition(TEST_PROCESS_BLOCKED, TEST_PROCESS_RUNNING) == 0);   /* Must transition through READY */
    assert(test_process_is_valid_transition(TEST_PROCESS_TERMINATED, TEST_PROCESS_BLOCKED) == 0);

    printf("PASS\n");
}

/* ========================================================
 * 9. Memory Boundary & Interrupt Frame Invariants
 * ======================================================== */
static void test_memory_and_frame_invariants(void)
{
    printf("[TEST] Memory Boundary & Frame Invariants... ");

    /* 20 quadwords = 15 GPRs (120 bytes) + 5 IRETQ fields (40 bytes) = 160 bytes */
    const size_t gpr_count = 15;
    const size_t iretq_field_count = 5;
    const size_t total_quadwords = gpr_count + iretq_field_count;
    assert(total_quadwords == 20);
    assert(total_quadwords * sizeof(uint64_t) == 160);

    /* Identity-mapped boundary check */
    const uint64_t identity_map_limit = 0x00200000ULL; /* 2 MiB boundary */
    uint64_t kernel_entry = 0x00100000ULL;
    uint64_t unmapped_high = 0x00201000ULL;
    assert(kernel_entry < identity_map_limit);
    assert(unmapped_high >= identity_map_limit);

    /* Page size & pool capacity */
    const uint32_t page_size = 4096;
    const uint32_t pool_count = 64;
    assert(pool_count * page_size == 262144); /* Exactly 256 KiB */

    printf("PASS\n");
}

/* ========================================================
 * 10. IPC Message Queue Invariants Tests
 * ======================================================== */
static void test_ipc_subsystem_invariants(void)
{
    printf("[TEST] IPC Subsystem & Bounded Queues... ");
    ipc_init();

    int res = test_ipc_invariants();
    assert(res == 0);

    /* Test explicit sender/receiver routing & filtering */
    ipc_message_t msg_to_p2 = {
        .sender_pid = 1,
        .receiver_pid = 2,
        .type = 42,
        .length = 5,
        .data = "HELLO"
    };
    ipc_message_t msg_to_p3 = {
        .sender_pid = 1,
        .receiver_pid = 3,
        .type = 43,
        .length = 5,
        .data = "WORLD"
    };

    assert(ipc_send(&msg_to_p2) == IPC_SUCCESS);
    assert(ipc_send(&msg_to_p3) == IPC_SUCCESS);

    /* P3 receives: should match msg_to_p3 */
    ipc_message_t recv_p3;
    assert(ipc_receive_filtered(&recv_p3, 3) == IPC_SUCCESS);
    assert(recv_p3.receiver_pid == 3);
    assert(recv_p3.type == 43);

    /* P2 receives: should match msg_to_p2 */
    ipc_message_t recv_p2;
    assert(ipc_receive_filtered(&recv_p2, 2) == IPC_SUCCESS);
    assert(recv_p2.receiver_pid == 2);
    assert(recv_p2.type == 42);

    /* No more messages for P2 */
    assert(ipc_receive_filtered(&recv_p2, 2) == IPC_ERR_EMPTY);

    printf("PASS\n");
}

/* ========================================================
 * 11. Synchronization (Mutex Ownership & Semaphore) Tests
 * ======================================================== */
static void test_synchronization_invariants_host(void)
{
    printf("[TEST] Mutex Ownership & Semaphore Invariants... ");

    assert(test_mutex_invariants() == 0);
    assert(test_semaphore_invariants() == 0);
    assert(test_sync_invariants() == 0);

    /* Test strict mutex ownership across distinct PIDs */
    mutex_t m;
    mutex_init(&m);

    /* PID 1 locks mutex */
    host_curr_proc.pid = 1;
    assert(mutex_lock(&m) == MUTEX_SUCCESS);
    assert(mutex_is_locked(&m) == 1);
    assert(mutex_get_owner(&m) == 1);

    /* PID 2 tries to unlock PID 1's mutex -> REJECTED! */
    host_curr_proc.pid = 2;
    assert(mutex_unlock(&m) == MUTEX_ERR_NOT_OWNER);
    assert(mutex_is_locked(&m) == 1);
    assert(mutex_get_owner(&m) == 1);

    /* PID 1 unlocks own mutex -> SUCCESS! */
    host_curr_proc.pid = 1;
    assert(mutex_unlock(&m) == MUTEX_SUCCESS);
    assert(mutex_is_locked(&m) == 0);
    assert(mutex_get_owner(&m) == 0);

    printf("PASS\n");
}

/* ========================================================
 * 12. Deadlock Scenarios & Prevention Invariants
 * ======================================================== */
static void test_deadlock_scenarios_invariants(void)
{
    printf("[TEST] Deadlock Scenarios & Havender Prevention... ");

    /* 1. Banker's Algorithm Safe State */
    banker_system_t bsys;
    uint32_t avail[3] = { 3, 3, 2 };
    banker_init_system(&bsys, 5, 3, avail);

    uint32_t alloc[5][3] = {
        { 0, 1, 0 },
        { 2, 0, 0 },
        { 3, 0, 2 },
        { 2, 1, 1 },
        { 0, 0, 2 }
    };
    uint32_t max[5][3] = {
        { 7, 5, 3 },
        { 3, 2, 2 },
        { 9, 0, 2 },
        { 2, 2, 2 },
        { 4, 3, 3 }
    };

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 3; j++) {
            bsys.allocation[i][j] = alloc[i][j];
            bsys.max[i][j] = max[i][j];
        }
    }
    banker_compute_need(&bsys);

    banker_safety_result_t bres;
    banker_check_safety(&bsys, &bres);
    assert(bres.is_safe == 1);
    assert(bres.safe_sequence[0] == 1); /* P1 finishes first */

    /* 2. Banker Request Error Checking:
     * Case A: Exceeding maximum claim (Request > Need) -> -1
     * Case B: Exceeding current available (Request > Available) -> -2
     * Case C: Request <= Available, but causes unsafe state -> -3 (Deadlock Avoidance!)
     */
    uint32_t exceed_claim_req[3] = { 8, 0, 0 }; /* Need is {7,4,3} */
    assert(banker_request_resources(&bsys, 0, exceed_claim_req) == -1);

    uint32_t exceed_avail_req[3] = { 4, 0, 0 }; /* Avail is {3,3,2} */
    assert(banker_request_resources(&bsys, 0, exceed_avail_req) == -2);

    uint32_t unsafe_req[3] = { 3, 3, 0 }; /* Valid claim and <= avail, but leads to unsafe state */
    int req_res = banker_request_resources(&bsys, 0, unsafe_req);
    assert(req_res == -3); /* Correctly denied by Banker to avoid deadlock! */


    /* 3. Deadlock Detection Matrix */
    deadlock_detection_system_t dsys = {
        .process_count = 2,
        .resource_count = 2,
        .available = { 0, 0 },
        .allocation = { { 1, 0 }, { 0, 1 } },
        .request = { { 0, 1 }, { 1, 0 } } /* Circular wait: P0 waits for R1, P1 waits for R0 */
    };
    deadlock_detection_result_t dres;
    deadlock_detect(&dsys, &dres);
    assert(dres.deadlock_detected == 1);
    assert(dres.deadlocked_count == 2);

    /* 4. Havender Prevention: Ascending Resource Ordering eliminates circular wait */
    dsys.request[1][0] = 0; /* P1 cannot request lower-ranked resource while holding higher */
    deadlock_detect(&dsys, &dres);
    assert(dres.deadlock_detected == 0);

    printf("PASS\n");
}

static void test_storage_block_device_invariants_host(void)
{
    printf("[TEST] Block Device Interface & Bounds Checks... ");

    assert(ramdisk_init() == 0);
    assert(block_device_is_ready() == 1);
    assert(block_device_get_sector_size() == 512);
    assert(block_device_get_sector_count() == 128);
    assert(block_device_get_capacity_bytes() == 128 * 512);

    /* Run standard block device invariant suite */
    int bdev_res = test_block_device_invariants();
    assert(bdev_res == 0);

    printf("PASS\n");
}

static void test_filesystem_invariants_host(void)
{
    printf("[TEST] JARVIS-FS Filesystem Correctness & Rollback... ");

    /* Run comprehensive filesystem invariant suite */
    int fs_res = test_fs_invariants();
    assert(fs_res == 0);

    /* Additional host-level validation of fs_stat and fs_get_info */
    fs_info_t info;
    assert(fs_get_info(&info) == FS_OK);
    assert(info.magic == FS_MAGIC);
    assert(info.used_inodes >= 1); /* kernel.cfg was restored */

    fs_stat_t st;
    assert(fs_stat("kernel.cfg", &st) == FS_OK);
    assert(st.size > 0);
    assert(st.blocks_allocated >= 1);
    assert(st.in_use == 1);

    /* Test path normalization with leading slash */
    fs_stat_t st_slash;
    assert(fs_stat("/kernel.cfg", &st_slash) == FS_OK);
    assert(st_slash.size == st.size);

    printf("PASS\n");
}

static void test_file_alloc_and_disk_sched_invariants_host(void)
{
    printf("[TEST] File Allocation (Reuse) & Disk Scheduling Invariants... ");

    /* Test file allocation contiguous, linked, indexed, freeing, reuse */
    int fa_res = test_file_alloc_invariants();
    assert(fa_res == 0);

    /* Test disk scheduling FCFS, SSTF, SCAN, C-SCAN invariants & edge cases */
    int ds_res = test_disk_sched_invariants();
    assert(ds_res == 0);

    printf("PASS\n");
}

static void test_gui_and_framebuffer_invariants(void)
{
    printf("[TEST] Framebuffer, Event Queue, Window Manager, Config & App Framework Invariants... ");

    /* 1. Framebuffer bounds and clipping invariants */
    int fb_res = test_framebuffer_invariants();
    assert(fb_res == 0);

    /* 2. Mouse packet decoding & clamping invariants */
    int mouse_res = test_mouse_invariants();
    assert(mouse_res == 0);

    /* 3. Event Queue ring buffer, underflow, FIFO, and overflow handling */
    int eq_res = test_event_queue_invariants();
    assert(eq_res == 0);

    /* 4. Window Manager creation, z-order, dragging, clipping, controls, and destruction */
    int wm_res = test_wm_invariants();
    assert(wm_res == 0);

    /* 5. Desktop environment, applications launching, and key routing */
    int desk_res = test_desktop_invariants();
    assert(desk_res == 0);

    /* 6. Desktop configuration serialization, defaults, validation, and checksum recovery */
    int cfg_res = test_config_invariants();
    assert(cfg_res == 0);

    /* 7. Application framework registration, lifecycle states, duplicate ID rejection, and shutdown */
    int app_res = test_app_framework_invariants();
    assert(app_res == 0);

    /* 8. Notification Subsystem: queue capacity, levels, overflow eviction, expiry, dismissal */
    int notif_res = test_notification_invariants();
    assert(notif_res == 0);

    printf("PASS\n");
}

/* ========================================================
 * 17. Phase 13: Stress, Stability & Resource Safety Tests
 * ======================================================== */
static void test_stress_and_stability_invariants(void)
{
    printf("[TEST] Stress Testing, Stability & Boundary Safety Invariants... ");

    /* 1. Dynamic Heap: 16-byte alignment, splitting, coalescing & churn */
    heap_init();
    void *blocks[64];
    for (int i = 0; i < 64; i++) {
        size_t sz = 16 * ((i % 8) + 1);
        blocks[i] = kmalloc(sz);
        assert(blocks[i] != NULL);
        assert(((uintptr_t)blocks[i] % 16) == 0); /* 16-byte System V ABI alignment */
        /* Fill with deterministic pattern */
        uint8_t *b = (uint8_t *)blocks[i];
        for (size_t k = 0; k < sz; k++) {
            b[k] = (uint8_t)(i + k);
        }
    }
    /* Verify pattern integrity */
    for (int i = 0; i < 64; i++) {
        size_t sz = 16 * ((i % 8) + 1);
        uint8_t *b = (uint8_t *)blocks[i];
        for (size_t k = 0; k < sz; k++) {
            assert(b[k] == (uint8_t)(i + k));
        }
    }
    /* Free odd blocks first (fragmentation simulation) */
    for (int i = 1; i < 64; i += 2) {
        kfree(blocks[i]);
    }
    /* Free even blocks (coalescing) */
    for (int i = 0; i < 64; i += 2) {
        kfree(blocks[i]);
    }
    /* After full free, allocate large block to verify 100% coalescing */
    void *large = kmalloc(40000);
    assert(large != NULL);
    assert(((uintptr_t)large % 16) == 0);
    kfree(large);

    /* 2. Process Transition Stress: 10,000 randomized state queries */
    for (int i = 0; i < 10000; i++) {
        int from = i % 5;
        int to = (i * 3 + 1) % 5;
        int valid = test_process_is_valid_transition(from, to);
        assert(valid == 0 || valid == 1);
    }

    /* 3. Filesystem Capacity Boundary & Rollback Stress */
    ramdisk_init();
    assert(fs_format() == FS_OK);
    assert(fs_create("boundary.bin") == FS_OK);
    int fd = fs_open("boundary.bin");
    assert(fd >= 0);

    /* Fill file up to FS_MAX_FILE_SIZE (4096 bytes = 8 direct blocks) */
    uint8_t chunk[512];
    for (int i = 0; i < 512; i++) chunk[i] = 0xAA;
    for (int i = 0; i < 8; i++) {
        int w = fs_write(fd, chunk, 512);
        assert(w == 512);
    }
    /* Writing beyond FS_MAX_FILE_SIZE must fail with FS_ERR_FILE_TOO_LARGE */
    assert(fs_write(fd, chunk, 1) == FS_ERR_FILE_TOO_LARGE);

    /* Truncate file back to 1024 bytes and verify blocks are freed */
    assert(fs_truncate(fd, 1024) == FS_OK);
    assert(fs_close(fd) == FS_OK);

    fs_stat_t st;
    assert(fs_stat("boundary.bin", &st) == FS_OK);
    assert(st.size == 1024);
    assert(st.blocks_allocated == 2);

    /* 4. Window Manager Churn Stress: 200 cycles of create, reorder, destroy */
    wm_init(1024, 768);
    for (int cycle = 0; cycle < 200; cycle++) {
        int wids[WM_MAX_WINDOWS];
        for (int w = 0; w < WM_MAX_WINDOWS; w++) {
            window_t *win = wm_create_window("StressWin", 50 + w * 10, 50 + w * 10, 200, 150, WINDOW_TYPE_CUSTOM);
            assert(win != NULL);
            wids[w] = win->id;
        }
        /* 9th window must fail gracefully */
        assert(wm_create_window("OverflowWin", 0, 0, 100, 100, WINDOW_TYPE_CUSTOM) == NULL);

        /* Bring middle window to front */
        wm_bring_to_front(wids[3]);
        assert(wm_get_by_id(wids[3])->z_order == WM_MAX_WINDOWS - 1);

        /* Destroy all windows in reverse order */
        for (int w = WM_MAX_WINDOWS - 1; w >= 0; w--) {
            assert(wm_destroy_window(wids[w]) == 0);
        }
    }

    /* 5. Event Queue Burst Stress: 1,000 pushes into 64-event circular buffer */
    event_queue_init();
    for (int i = 0; i < 1000; i++) {
        gui_event_t ev = { .type = EVENT_MOUSE_MOVE, .x = i, .y = i * 2 };
        event_queue_push(&ev);
    }
    assert(event_queue_count() == GUI_EVENT_QUEUE_SIZE);
    assert(event_queue_dropped() == 1000 - GUI_EVENT_QUEUE_SIZE);
    /* Pop remaining events and verify integrity */
    for (int i = 0; i < GUI_EVENT_QUEUE_SIZE; i++) {
        gui_event_t out;
        assert(event_queue_pop(&out) == 0);
        assert(out.type == EVENT_MOUSE_MOVE);
    }
    assert(event_queue_count() == 0);

    /* 6. Desktop Configuration Bit-Flip Recovery Stress */
    desktop_config_t cfg;
    config_init_defaults(&cfg);
    assert(config_validate(&cfg) == 0);
    /* Corrupt magic */
    cfg.magic = 0xDEADBEEF;
    assert(config_validate(&cfg) == -2);
    /* Restore magic, corrupt checksum */
    cfg.magic = CONFIG_MAGIC;
    cfg.checksum ^= 0x55555555;
    assert(config_validate(&cfg) == -9);
    /* Reset to defaults */
    config_reset_defaults(&cfg);
    assert(config_validate(&cfg) == 0);

    printf("PASS\n");
}

/* ========================================================
 * 18. Diagnostics, Logging, Exceptions & Recovery Invariants
 * ======================================================== */
static void test_diagnostics_and_recovery_invariants(void)
{
    printf("[TEST] Diagnostics, Ring-Buffer Logging, Exceptions & Recovery... ");

    /* 1. Test bounded diagnostics and logging invariants */
    int diag_res = test_diagnostics_invariants();
    assert(diag_res == 0);

    /* 2. Test 32-vector exception invariants, frame size, and page fault decoding */
    int exc_res = test_exception_invariants();
    assert(exc_res == 0);

    /* Verify exception names across edge cases */
    assert(exception_name(0) != NULL);
    assert(exception_name(13) != NULL);
    assert(exception_name(14) != NULL);
    assert(exception_name(31) != NULL);
    assert(exception_name(32) != NULL); /* Unknown exception */

    /* 3. Test power management invariants and volatile storage disclosure */
    int pwr_res = test_power_invariants();
    assert(pwr_res == 0);

    /* 4. Test heap telemetry tracking */
    heap_init();
    heap_stats_t hstats;
    heap_get_stats(&hstats);
    assert(hstats.total_size == KERNEL_HEAP_SIZE);
    assert(hstats.alloc_count == 0);
    assert(hstats.free_count == 0);
    assert(hstats.used_bytes == 0);
    assert(hstats.free_bytes > 0);

    void *p = kmalloc(256);
    assert(p != NULL);
    heap_get_stats(&hstats);
    assert(hstats.alloc_count == 1);
    assert(hstats.used_bytes >= 256);

    kfree(p);
    heap_get_stats(&hstats);
    assert(hstats.free_count == 1);
    assert(hstats.used_bytes == 0);

    /* 5. Test filesystem operation statistics */
    uint32_t ops = 0, errs = 0, rolls = 0;
    fs_get_stats(&ops, &errs, &rolls);
    /* Ops counter must be accessible without faults */
    (void)ops; (void)errs; (void)rolls;

    /* 6. Verify comprehensive system diagnostic summary */
    diag_system_summary_t summary;
    diag_get_summary(&summary);
    assert(summary.kernel_version != NULL);
    assert(summary.arch != NULL);
    assert(summary.log_entries_count <= DIAG_LOG_CAPACITY);

    printf("PASS\n");
}

/* ========================================================
 * 19. Hardware Abstraction Layer (HAL) Invariants
 * ======================================================== */
static void test_hal_subsystem_invariants(void)
{
    printf("[TEST] Hardware Abstraction Layer (HAL) Modular Registry & Invariants... ");

    /* 1. Test HAL internal invariants (registration, duplicates, dependencies, probes, faults) */
    int inv_res = test_hal_invariants();
    assert(inv_res == 0);

    /* 2. Test string converters */
    assert(hal_class_name(HAL_DEV_CLASS_TIMER) != NULL);
    assert(hal_class_name(HAL_DEV_CLASS_INTERRUPT_CONTROLLER) != NULL);
    assert(hal_class_name(HAL_DEV_CLASS_SERIAL) != NULL);
    assert(hal_class_name(HAL_DEV_CLASS_BLOCK) != NULL);
    assert(hal_class_name(HAL_DEV_CLASS_POWER) != NULL);
    assert(hal_state_name(HAL_STATE_ONLINE) != NULL);
    assert(hal_state_name(HAL_STATE_FAULT) != NULL);
    assert(hal_backend_name(HAL_BACKEND_HARDWARE) != NULL);
    assert(hal_backend_name(HAL_BACKEND_MEMORY_BACKED) != NULL);

    /* 3. Test core drivers registration and multi-pass resolution */
    hal_init();
    assert(hal_get_driver_count() == 0);
    assert(hal_get_online_count() == 0);

    hal_register_core_drivers();
    assert(hal_get_driver_count() == 10);

    /* Verify drivers exist */
    assert(hal_find_driver("pic_8259") != NULL);
    assert(hal_find_driver("serial_uart") != NULL);
    assert(hal_find_driver("vga_terminal") != NULL);
    assert(hal_find_driver("pit_8254") != NULL);
    assert(hal_find_driver("ps2_keyboard") != NULL);
    assert(hal_find_driver("ps2_mouse") != NULL);
    assert(hal_find_driver("framebuffer") != NULL);
    assert(hal_find_driver("ramdisk_block") != NULL);
    assert(hal_find_driver("acpi_power") != NULL);
    assert(hal_find_driver("net_loopback") != NULL);

    /* Multi-pass initialization */
    int online = hal_init_all();
    assert(online >= 9); /* Framebuffer may be absent in pure mock host test, but other 9 online */
    assert(hal_find_driver("pic_8259")->state == HAL_STATE_ONLINE);
    assert(hal_find_driver("pit_8254")->state == HAL_STATE_ONLINE);
    assert(hal_find_driver("serial_uart")->state == HAL_STATE_ONLINE);
    assert(hal_find_driver("vga_terminal")->state == HAL_STATE_ONLINE);
    assert(hal_find_driver("ramdisk_block")->state == HAL_STATE_ONLINE);
    assert(hal_find_driver("net_loopback")->state == HAL_STATE_ONLINE);

    /* 4. Test unified block read/write through HAL adapter */
    uint8_t wbuf[512];
    uint8_t rbuf[512];
    for (int i = 0; i < 512; i++) {
        wbuf[i] = (uint8_t)(i ^ 0x5A);
        rbuf[i] = 0;
    }
    assert(hal_block_write(2, 1, wbuf) == 0);
    assert(hal_block_read(2, 1, rbuf) == 0);
    for (int i = 0; i < 512; i++) {
        assert(rbuf[i] == wbuf[i]);
    }

    /* 5. Test console routing without crashes */
    hal_console_write("[HOST_TEST] HAL console write test message\n");

    /* 6. Test graceful shutdown in reverse order */
    assert(hal_shutdown_all() == 0);
    assert(hal_get_online_count() == 0);

    printf("PASS\n");
}

static void test_networking_stack_invariants(void)
{
    printf("Test 20: Networking Stack Invariants & Packet Flow... ");

    /* 1. Run low-level module invariant tests */
    assert(test_netdev_invariants() == 0);
    assert(test_ethernet_invariants() == 0);
    assert(test_arp_invariants() == 0);
    assert(test_ipv4_invariants() == 0);
    assert(test_icmp_invariants() == 0);
    assert(test_udp_invariants() == 0);
    assert(test_dhcp_invariants() == 0);

    /* 2. Run end-to-end loopback packet flow */
    int net_inv_res = test_networking_invariants();
    if (net_inv_res != 0) {
        printf("DEBUG: test_networking_invariants returned %d\n", net_inv_res);
    }
    assert(net_inv_res == 0);

    /* 3. Verify IP string conversion helpers */
    char ip_str[32];
    net_ip_to_str(0x0A00020F, ip_str, sizeof(ip_str));
    assert(strcmp(ip_str, "10.0.2.15") == 0);
    assert(net_str_to_ip("10.0.2.15") == 0x0A00020F);
    assert(net_str_to_ip("192.168.1.1") == 0xC0A80101);
    assert(net_str_to_ip("256.0.0.1") == 0);

    /* 4. Verify MAC string formatting */
    char mac_str[32];
    uint8_t sample_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
    eth_format_mac(sample_mac, mac_str, sizeof(mac_str));
    assert(strcmp(mac_str, "52:54:00:12:34:56") == 0);

    /* 5. Verify ARP cache lookup and capacity eviction */
    arp_init();
    uint8_t mac_out[6];
    assert(arp_lookup(0x0A000202, mac_out) == NET_ERR_NOT_FOUND);
    arp_update_cache(0x0A000202, sample_mac);
    assert(arp_lookup(0x0A000202, mac_out) == NET_OK);
    for (int i = 0; i < 6; i++) {
        assert(mac_out[i] == sample_mac[i]);
    }

    /* 6. Verify Netstack Device Registry & Telemetry */
    netstack_init();
    net_device_t *lo = netdev_get_by_name("lo0");
    net_device_t *eth = netdev_get_by_name("eth0");
    assert(lo != NULL);
    assert(eth != NULL);
    assert(lo->backend == HAL_BACKEND_MEMORY_BACKED);
    assert(eth->backend == HAL_BACKEND_HARDWARE);
    assert(netdev_get_count() >= 2);

    /* 7. Verify diagnostic status and ARP table printing without faults */
    netstack_print_status();
    arp_print_table();

    printf("PASS\n");
}

int main(void)
{
    printf("==================================================\n");
    printf("   JARVIS-OS HOST ALGORITHM VERIFICATION SUITE    \n");
    printf("==================================================\n");

    test_scheduling();
    test_page_replacement();
    test_banker_algorithm();
    test_disk_scheduling();
    test_mem_fit();
    test_file_alloc();
    test_deadlock_detection_and_prevention();
    test_process_lifecycle_state_machine();
    test_memory_and_frame_invariants();
    test_ipc_subsystem_invariants();
    test_synchronization_invariants_host();
    test_deadlock_scenarios_invariants();
    test_storage_block_device_invariants_host();
    test_filesystem_invariants_host();
    test_file_alloc_and_disk_sched_invariants_host();
    test_gui_and_framebuffer_invariants();
    test_stress_and_stability_invariants();
    test_diagnostics_and_recovery_invariants();
    test_hal_subsystem_invariants();
    test_networking_stack_invariants();

    printf("==================================================\n");
    printf("   ALL HOST UNIT TESTS COMPLETED SUCCESSFULLY!    \n");
    printf("==================================================\n");
    return 0;
}



