#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include "algorithms/sched_sim.h"
#include "algorithms/page_replace.h"
#include "algorithms/banker.h"
#include "algorithms/disk_sched.h"

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

int main(void)
{
    printf("==================================================\n");
    printf("   JARVIS-OS HOST ALGORITHM VERIFICATION SUITE    \n");
    printf("==================================================\n");

    test_scheduling();
    test_page_replacement();
    test_banker_algorithm();
    test_disk_scheduling();

    printf("==================================================\n");
    printf("   ALL HOST UNIT TESTS COMPLETED SUCCESSFULLY!    \n");
    printf("==================================================\n");
    return 0;
}
