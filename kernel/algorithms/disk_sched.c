#include "disk_sched.h"
#include "../drivers/terminal.h"

static uint32_t abs_diff(uint32_t a, uint32_t b)
{
    return (a > b) ? (a - b) : (b - a);
}

void disk_sched_fcfs(uint32_t initial_head, const uint32_t *reqs, uint32_t count, disk_sched_result_t *res)
{
    if (count > DISK_MAX_REQUESTS) count = DISK_MAX_REQUESTS;
    res->count = count;
    res->total_head_movement = 0;

    uint32_t current = initial_head;
    for (uint32_t i = 0; i < count; i++) {
        res->sequence[i] = reqs[i];
        res->total_head_movement += abs_diff(current, reqs[i]);
        current = reqs[i];
    }
}

void disk_sched_sstf(uint32_t initial_head, const uint32_t *reqs, uint32_t count, disk_sched_result_t *res)
{
    if (count > DISK_MAX_REQUESTS) count = DISK_MAX_REQUESTS;
    res->count = count;
    res->total_head_movement = 0;

    int visited[DISK_MAX_REQUESTS];
    for (uint32_t i = 0; i < count; i++) visited[i] = 0;

    uint32_t current = initial_head;
    for (uint32_t step = 0; step < count; step++) {
        int best_idx = -1;
        uint32_t min_dist = 0xFFFFFFFF;

        for (uint32_t i = 0; i < count; i++) {
            if (!visited[i]) {
                uint32_t dist = abs_diff(current, reqs[i]);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_idx = (int)i;
                }
            }
        }

        if (best_idx != -1) {
            visited[best_idx] = 1;
            res->sequence[step] = reqs[best_idx];
            res->total_head_movement += min_dist;
            current = reqs[best_idx];
        }
    }
}

static void sort_array(uint32_t *arr, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            if (arr[j] < arr[i]) {
                uint32_t tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
        }
    }
}

void disk_sched_scan(uint32_t initial_head, const uint32_t *reqs, uint32_t count, uint32_t disk_size, int direction_up, disk_sched_result_t *res)
{
    (void)direction_up;
    if (count > DISK_MAX_REQUESTS) count = DISK_MAX_REQUESTS;

    uint32_t sorted[DISK_MAX_REQUESTS];
    for (uint32_t i = 0; i < count; i++) sorted[i] = reqs[i];
    sort_array(sorted, count);

    res->count = 0;
    res->total_head_movement = 0;
    uint32_t current = initial_head;

    /* Elevator moving up towards disk_size - 1 */
    for (uint32_t i = 0; i < count; i++) {
        if (sorted[i] >= initial_head) {
            res->sequence[res->count++] = sorted[i];
            res->total_head_movement += abs_diff(current, sorted[i]);
            current = sorted[i];
        }
    }

    if (current < disk_size - 1) {
        res->total_head_movement += abs_diff(current, disk_size - 1);
        current = disk_size - 1;
    }

    /* Reverse down */
    for (int i = (int)count - 1; i >= 0; i--) {
        if (sorted[i] < initial_head) {
            res->sequence[res->count++] = sorted[i];
            res->total_head_movement += abs_diff(current, sorted[i]);
            current = sorted[i];
        }
    }
}

void disk_sched_cscan(uint32_t initial_head, const uint32_t *reqs, uint32_t count, uint32_t disk_size, disk_sched_result_t *res)
{
    if (count > DISK_MAX_REQUESTS) count = DISK_MAX_REQUESTS;

    uint32_t sorted[DISK_MAX_REQUESTS];
    for (uint32_t i = 0; i < count; i++) sorted[i] = reqs[i];
    sort_array(sorted, count);

    res->count = 0;
    res->total_head_movement = 0;
    uint32_t current = initial_head;

    /* Move up to end of disk */
    for (uint32_t i = 0; i < count; i++) {
        if (sorted[i] >= initial_head) {
            res->sequence[res->count++] = sorted[i];
            res->total_head_movement += abs_diff(current, sorted[i]);
            current = sorted[i];
        }
    }

    if (current < disk_size - 1) {
        res->total_head_movement += abs_diff(current, disk_size - 1);
        current = disk_size - 1;
    }

    /* Circular jump to 0 */
    res->total_head_movement += (disk_size - 1);
    current = 0;

    /* Move up from 0 to initial_head */
    for (uint32_t i = 0; i < count; i++) {
        if (sorted[i] < initial_head) {
            res->sequence[res->count++] = sorted[i];
            res->total_head_movement += abs_diff(current, sorted[i]);
            current = sorted[i];
        }
    }
}

static void print_disk_result(const char *algo, const disk_sched_result_t *res)
{
    terminal_write(algo);
    terminal_write(" Seek Sequence: ");
    for (uint32_t i = 0; i < res->count; i++) {
        terminal_write_number(res->sequence[i]);
        if (i + 1 < res->count) terminal_write(" -> ");
    }
    terminal_write("\nTotal Head Movement: ");
    terminal_write_number(res->total_head_movement);
    terminal_write(" cylinders\n\n");
}

void disk_sched_run_demo(void)
{
    static const uint32_t reqs[8] = { 98, 183, 37, 122, 14, 124, 65, 67 };
    uint32_t initial_head = 53;
    uint32_t disk_size = 200;

    terminal_write("\n========================================\n");
    terminal_write("      DISK SCHEDULING BENCHMARK         \n");
    terminal_write("========================================\n");
    terminal_write("Initial Head Position: Cylinder 53\n");
    terminal_write("Queue: [98, 183, 37, 122, 14, 124, 65, 67] (Disk: 0..199)\n\n");

    disk_sched_result_t res;

    disk_sched_fcfs(initial_head, reqs, 8, &res);
    print_disk_result("1. FCFS", &res);

    disk_sched_sstf(initial_head, reqs, 8, &res);
    print_disk_result("2. SSTF", &res);

    disk_sched_scan(initial_head, reqs, 8, disk_size, 1, &res);
    print_disk_result("3. SCAN (Elevator)", &res);

    disk_sched_cscan(initial_head, reqs, 8, disk_size, &res);
    print_disk_result("4. C-SCAN", &res);
}

int test_disk_sched_invariants(void)
{
    static const uint32_t reqs[8] = { 98, 183, 37, 122, 14, 124, 65, 67 };
    uint32_t initial_head = 53;
    uint32_t disk_size = 200;
    disk_sched_result_t res;

    /* 1. FCFS benchmark */
    disk_sched_fcfs(initial_head, reqs, 8, &res);
    if (res.count != 8) return -1;
    if (res.total_head_movement != 640) return -2;

    /* 2. SSTF benchmark */
    disk_sched_sstf(initial_head, reqs, 8, &res);
    if (res.count != 8) return -3;
    if (res.total_head_movement != 236) return -4;

    /* 3. SCAN benchmark */
    disk_sched_scan(initial_head, reqs, 8, disk_size, 1, &res);
    if (res.count != 8) return -5;
    if (res.total_head_movement != 331) return -6;

    /* 4. C-SCAN benchmark */
    disk_sched_cscan(initial_head, reqs, 8, disk_size, &res);
    if (res.count != 8) return -7;
    if (res.total_head_movement != 382) return -8;

    /* 5. Edge Cases: 0 requests */
    disk_sched_fcfs(initial_head, reqs, 0, &res);
    if (res.count != 0 || res.total_head_movement != 0) return -9;

    disk_sched_sstf(initial_head, reqs, 0, &res);
    if (res.count != 0 || res.total_head_movement != 0) return -10;

    /* 6. Single request matching current head position (seek = 0) */
    uint32_t single_req[1] = { 53 };
    disk_sched_fcfs(53, single_req, 1, &res);
    if (res.total_head_movement != 0) return -11;

    return 0;
}


