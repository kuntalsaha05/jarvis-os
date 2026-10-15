#ifndef DISK_SCHED_H
#define DISK_SCHED_H

#include <stdint.h>

#define DISK_MAX_REQUESTS 16

typedef struct {
    uint32_t sequence[DISK_MAX_REQUESTS + 2];
    uint32_t count;
    uint32_t total_head_movement;
} disk_sched_result_t;

/* Disk scheduling simulators */
void disk_sched_fcfs(uint32_t initial_head, const uint32_t *reqs, uint32_t count, disk_sched_result_t *res);
void disk_sched_sstf(uint32_t initial_head, const uint32_t *reqs, uint32_t count, disk_sched_result_t *res);
void disk_sched_scan(uint32_t initial_head, const uint32_t *reqs, uint32_t count, uint32_t disk_size, int direction_up, disk_sched_result_t *res);
void disk_sched_cscan(uint32_t initial_head, const uint32_t *reqs, uint32_t count, uint32_t disk_size, disk_sched_result_t *res);

/* Educational demonstration */
void disk_sched_run_demo(void);

#endif /* DISK_SCHED_H */

