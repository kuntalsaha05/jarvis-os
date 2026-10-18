#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
#include "gui/app.h"

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

void debug_puts(const char *s) { (void)s; }
void pic_send_eoi(uint8_t irq) { (void)irq; }
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

    printf("==================================================\n");
    printf("   ALL HOST UNIT TESTS COMPLETED SUCCESSFULLY!    \n");
    printf("==================================================\n");
    return 0;
}



