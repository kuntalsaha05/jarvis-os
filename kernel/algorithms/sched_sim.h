#ifndef SCHED_SIM_H
#define SCHED_SIM_H

#include <stdint.h>

#define SCHED_SIM_MAX_TASKS 8

typedef struct {
    uint32_t id;
    uint32_t arrival_time;
    uint32_t burst_time;
    uint32_t remaining_time;
    uint32_t completion_time;
    uint32_t turnaround_time;
    uint32_t waiting_time;
    uint32_t response_time;
    int started;
} sched_task_t;

typedef struct {
    uint32_t task_count;
    sched_task_t tasks[SCHED_SIM_MAX_TASKS];
    uint32_t total_turnaround_time;
    uint32_t total_waiting_time;
    uint32_t avg_turnaround_scaled; /* multiplied by 100 for integer precision */
    uint32_t avg_waiting_scaled;    /* multiplied by 100 for integer precision */
} sched_result_t;

/* Scheduling simulation functions */
void sched_sim_fcfs(const sched_task_t *inputs, uint32_t count, sched_result_t *result);
void sched_sim_sjf(const sched_task_t *inputs, uint32_t count, sched_result_t *result);
void sched_sim_rr(const sched_task_t *inputs, uint32_t count, uint32_t quantum, sched_result_t *result);

/* Formatted demonstration printer for terminal */
void sched_sim_run_demo(void);

#endif /* SCHED_SIM_H */

