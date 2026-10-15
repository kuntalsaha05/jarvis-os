#include "deadlock.h"
#include "../drivers/terminal.h"

void deadlock_detect(const deadlock_detection_system_t *sys, deadlock_detection_result_t *res)
{
    res->deadlock_detected = 0;
    res->deadlocked_count = 0;

    if (!sys || sys->process_count == 0 || sys->resource_count == 0 ||
        sys->process_count > DEADLOCK_MAX_PROCS || sys->resource_count > DEADLOCK_MAX_RES) {
        return;
    }

    uint32_t work[DEADLOCK_MAX_RES];
    int finish[DEADLOCK_MAX_PROCS];

    for (uint32_t j = 0; j < sys->resource_count; j++) {
        work[j] = sys->available[j];
    }

    /* Initialize Finish vector: if process has no allocated resources, finish=1 */
    for (uint32_t i = 0; i < sys->process_count; i++) {
        int has_allocation = 0;
        for (uint32_t j = 0; j < sys->resource_count; j++) {
            if (sys->allocation[i][j] > 0) {
                has_allocation = 1;
                break;
            }
        }
        finish[i] = (has_allocation == 0) ? 1 : 0;
    }

    /* Search for uncompleted process whose request can be satisfied */
    while (1) {
        int found = 0;

        for (uint32_t i = 0; i < sys->process_count; i++) {
            if (!finish[i]) {
                int can_satisfy = 1;
                for (uint32_t j = 0; j < sys->resource_count; j++) {
                    if (sys->request[i][j] > work[j]) {
                        can_satisfy = 0;
                        break;
                    }
                }

                if (can_satisfy) {
                    for (uint32_t j = 0; j < sys->resource_count; j++) {
                        work[j] += sys->allocation[i][j];
                    }
                    finish[i] = 1;
                    found = 1;
                }
            }
        }

        if (!found) {
            break;
        }
    }

    /* Collect deadlocked processes */
    for (uint32_t i = 0; i < sys->process_count; i++) {
        if (!finish[i]) {
            res->deadlocked_procs[res->deadlocked_count++] = i;
        }
    }

    res->deadlock_detected = (res->deadlocked_count > 0);
}

void deadlock_detection_demo(void)
{
    terminal_write("\n--- Deadlock Detection Algorithm ---\n");

    /* Scenario 1: Non-deadlocked system */
    deadlock_detection_system_t sys1 = {
        .process_count = 3,
        .resource_count = 3,
        .available = { 0, 0, 0 },
        .allocation = {
            { 0, 1, 0 }, /* P0 */
            { 2, 0, 0 }, /* P1 */
            { 0, 0, 2 }  /* P2 */
        },
        .request = {
            { 0, 0, 0 }, /* P0 has no pending request, will finish and release */
            { 0, 1, 0 }, /* P1 needs resource held by P0 */
            { 0, 0, 0 }  /* P2 will finish and release */
        }
    };

    deadlock_detection_result_t res1;
    deadlock_detect(&sys1, &res1);

    terminal_write("Scenario 1 (Transient dependency): ");
    if (!res1.deadlock_detected) {
        terminal_write("PASS (No Deadlock Detected)\n");
    } else {
        terminal_write("FAIL\n");
    }

    /* Scenario 2: Deadlocked system with circular wait (P0 holds R0, requests R1; P1 holds R1, requests R0) */
    deadlock_detection_system_t sys2 = {
        .process_count = 2,
        .resource_count = 2,
        .available = { 0, 0 },
        .allocation = {
            { 1, 0 }, /* P0 holds R0 */
            { 0, 1 }  /* P1 holds R1 */
        },
        .request = {
            { 0, 1 }, /* P0 requests R1 */
            { 1, 0 }  /* P1 requests R0 */
        }
    };

    deadlock_detection_result_t res2;
    deadlock_detect(&sys2, &res2);

    terminal_write("Scenario 2 (Circular Wait Cycle):  ");
    if (res2.deadlock_detected && res2.deadlocked_count == 2) {
        terminal_write("PASS (Deadlock Detected in Processes: P");
        terminal_write_number(res2.deadlocked_procs[0]);
        terminal_write(", P");
        terminal_write_number(res2.deadlocked_procs[1]);
        terminal_write(")\n");
    } else {
        terminal_write("FAIL\n");
    }
}

void deadlock_prevention_demo(void)
{
    terminal_write("\n--- Deadlock Prevention (Havender's Resource Ordering) ---\n");
    terminal_write("Strategy: Eliminate Circular Wait via Global Linear Hierarchy:\n");
    terminal_write("  Priority Order: R0 (Disk=1) < R1 (Memory=2) < R2 (Printer=3)\n");
    terminal_write("Rule: A process may request R_j only if Order(R_j) > Order(R_held).\n\n");

    terminal_write("Simulating Process P0:\n");
    terminal_write("  Acquiring R0 (Priority 1)... OK\n");
    terminal_write("  Acquiring R1 (Priority 2 > 1)... OK\n");
    terminal_write("Simulating Process P1:\n");
    terminal_write("  Attempting to acquire R0 while holding R1 (Priority 1 < 2)...\n");
    terminal_write("  REJECTED by Prevention Invariant: Request violates ordering rule.\n");
    terminal_write("  Result: Circular wait cycle is mathematically prevented.\n");
}

void deadlock_run_demo(void)
{
    terminal_write("\n========================================\n");
    terminal_write("     DEADLOCK DETECTION & PREVENTION    \n");
    terminal_write("========================================\n");

    deadlock_detection_demo();
    deadlock_prevention_demo();
}

