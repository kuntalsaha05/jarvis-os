#include "banker.h"
#include "../drivers/terminal.h"

void banker_init_system(banker_system_t *sys, uint32_t p_count, uint32_t r_count, const uint32_t *avail)
{
    if (p_count > BANKER_MAX_PROCESSES) p_count = BANKER_MAX_PROCESSES;
    if (r_count > BANKER_MAX_RESOURCES) r_count = BANKER_MAX_RESOURCES;

    sys->process_count = p_count;
    sys->resource_count = r_count;

    for (uint32_t j = 0; j < r_count; j++) {
        sys->available[j] = avail[j];
    }

    for (uint32_t i = 0; i < p_count; i++) {
        for (uint32_t j = 0; j < r_count; j++) {
            sys->max[i][j] = 0;
            sys->allocation[i][j] = 0;
            sys->need[i][j] = 0;
        }
    }
}

void banker_compute_need(banker_system_t *sys)
{
    for (uint32_t i = 0; i < sys->process_count; i++) {
        for (uint32_t j = 0; j < sys->resource_count; j++) {
            if (sys->max[i][j] >= sys->allocation[i][j]) {
                sys->need[i][j] = sys->max[i][j] - sys->allocation[i][j];
            } else {
                sys->need[i][j] = 0;
            }
        }
    }
}

void banker_check_safety(const banker_system_t *sys, banker_safety_result_t *res)
{
    uint32_t work[BANKER_MAX_RESOURCES];
    int finish[BANKER_MAX_PROCESSES];

    for (uint32_t j = 0; j < sys->resource_count; j++) {
        work[j] = sys->available[j];
    }
    for (uint32_t i = 0; i < sys->process_count; i++) {
        finish[i] = 0;
    }

    uint32_t count = 0;

    while (count < sys->process_count) {
        int found = 0;

        for (uint32_t i = 0; i < sys->process_count; i++) {
            if (!finish[i]) {
                int can_allocate = 1;
                for (uint32_t j = 0; j < sys->resource_count; j++) {
                    if (sys->need[i][j] > work[j]) {
                        can_allocate = 0;
                        break;
                    }
                }

                if (can_allocate) {
                    for (uint32_t j = 0; j < sys->resource_count; j++) {
                        work[j] += sys->allocation[i][j];
                    }
                    res->safe_sequence[count++] = i;
                    finish[i] = 1;
                    found = 1;
                }
            }
        }

        if (!found) {
            break;
        }
    }

    res->is_safe = (count == sys->process_count);
}

int banker_request_resources(banker_system_t *sys, uint32_t pid, const uint32_t *request)
{
    if (pid >= sys->process_count) return -1;

    /* Condition 1: Request <= Need */
    for (uint32_t j = 0; j < sys->resource_count; j++) {
        if (request[j] > sys->need[pid][j]) {
            return -1; /* Exceeded maximum claim */
        }
    }

    /* Condition 2: Request <= Available */
    for (uint32_t j = 0; j < sys->resource_count; j++) {
        if (request[j] > sys->available[j]) {
            return -2; /* Resources not available, must wait */
        }
    }

    /* Pretend allocation */
    for (uint32_t j = 0; j < sys->resource_count; j++) {
        sys->available[j] -= request[j];
        sys->allocation[pid][j] += request[j];
        sys->need[pid][j] -= request[j];
    }

    banker_safety_result_t check;
    banker_check_safety(sys, &check);

    if (check.is_safe) {
        return 0; /* Approved, system remains in safe state */
    }

    /* Rollback because state is unsafe */
    for (uint32_t j = 0; j < sys->resource_count; j++) {
        sys->available[j] += request[j];
        sys->allocation[pid][j] -= request[j];
        sys->need[pid][j] += request[j];
    }

    return -3; /* Denied: grant would lead to unsafe state */
}

void banker_run_demo(void)
{
    terminal_write("\n========================================\n");
    terminal_write("      BANKER'S DEADLOCK AVOIDANCE       \n");
    terminal_write("========================================\n");
    terminal_write("System: 5 Processes (P0..P4), 3 Resources (A, B, C)\n");

    banker_system_t sys;
    uint32_t avail[3] = { 3, 3, 2 };
    banker_init_system(&sys, 5, 3, avail);

    /* Allocation Matrix */
    uint32_t alloc[5][3] = {
        {0, 1, 0}, /* P0 */
        {2, 0, 0}, /* P1 */
        {3, 0, 2}, /* P2 */
        {2, 1, 1}, /* P3 */
        {0, 0, 2}  /* P4 */
    };

    /* Max Matrix */
    uint32_t max_claim[5][3] = {
        {7, 5, 3}, /* P0 */
        {3, 2, 2}, /* P1 */
        {9, 0, 2}, /* P2 */
        {2, 2, 2}, /* P3 */
        {4, 3, 3}  /* P4 */
    };

    for (uint32_t i = 0; i < 5; i++) {
        for (uint32_t j = 0; j < 3; j++) {
            sys.allocation[i][j] = alloc[i][j];
            sys.max[i][j] = max_claim[i][j];
        }
    }

    banker_compute_need(&sys);

    terminal_write("\nAvailable Vector: [ ");
    for (int j = 0; j < 3; j++) {
        terminal_write_number(sys.available[j]);
        terminal_putchar(' ');
    }
    terminal_write("]\n");

    terminal_write("\nProcess  Allocation    Max Claim     Need (Max-Alloc)\n");
    for (uint32_t i = 0; i < 5; i++) {
        terminal_write("P");
        terminal_write_number(i);
        terminal_write("       ");

        terminal_write_number(sys.allocation[i][0]);
        terminal_putchar(' ');
        terminal_write_number(sys.allocation[i][1]);
        terminal_putchar(' ');
        terminal_write_number(sys.allocation[i][2]);
        terminal_write("       ");

        terminal_write_number(sys.max[i][0]);
        terminal_putchar(' ');
        terminal_write_number(sys.max[i][1]);
        terminal_putchar(' ');
        terminal_write_number(sys.max[i][2]);
        terminal_write("       ");

        terminal_write_number(sys.need[i][0]);
        terminal_putchar(' ');
        terminal_write_number(sys.need[i][1]);
        terminal_putchar(' ');
        terminal_write_number(sys.need[i][2]);
        terminal_putchar('\n');
    }

    banker_safety_result_t safe_res;
    banker_check_safety(&sys, &safe_res);

    terminal_write("\nSafety Analysis:\n");
    if (safe_res.is_safe) {
        terminal_write("Status: SAFE STATE\n");
        terminal_write("Safe sequence found: < ");
        for (uint32_t i = 0; i < sys.process_count; i++) {
            terminal_write("P");
            terminal_write_number(safe_res.safe_sequence[i]);
            if (i + 1 < sys.process_count) terminal_write(" -> ");
        }
        terminal_write(" >\n");
    } else {
        terminal_write("Status: UNSAFE STATE (Potential Deadlock)\n");
    }

    /* Test Request 1: Safe request from P1: (1, 0, 2) */
    terminal_write("\nTest 1: P1 requests (1, 0, 2)...\n");
    uint32_t req1[3] = { 1, 0, 2 };
    int r1 = banker_request_resources(&sys, 1, req1);
    if (r1 == 0) {
        terminal_write("Result: APPROVED (System transitioned to new safe state)\n");
    } else {
        terminal_write("Result: DENIED\n");
    }

    /* Test Request 2: Unsafe request from P4: (3, 3, 0) - exceeds available */
    terminal_write("\nTest 2: P4 requests (3, 3, 0)...\n");
    uint32_t req2[3] = { 3, 3, 0 };
    int r2 = banker_request_resources(&sys, 4, req2);
    if (r2 == -2) {
        terminal_write("Result: WAIT (Request exceeds currently available resources)\n");
    } else if (r2 == -3) {
        terminal_write("Result: REJECTED (Granting would enter UNSAFE state)\n");
    }
}
