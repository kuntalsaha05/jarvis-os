#include "sched_sim.h"
#include "../drivers/terminal.h"

static void copy_tasks(const sched_task_t *src, sched_task_t *dst, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        dst[i] = src[i];
        dst[i].remaining_time = src[i].burst_time;
        dst[i].completion_time = 0;
        dst[i].turnaround_time = 0;
        dst[i].waiting_time = 0;
        dst[i].response_time = 0;
        dst[i].started = 0;
    }
}

static void compute_metrics(sched_result_t *res)
{
    res->total_turnaround_time = 0;
    res->total_waiting_time = 0;

    for (uint32_t i = 0; i < res->task_count; i++) {
        sched_task_t *t = &res->tasks[i];
        t->turnaround_time = t->completion_time - t->arrival_time;
        t->waiting_time = t->turnaround_time - t->burst_time;
        res->total_turnaround_time += t->turnaround_time;
        res->total_waiting_time += t->waiting_time;
    }

    if (res->task_count > 0) {
        res->avg_turnaround_scaled = (res->total_turnaround_time * 100) / res->task_count;
        res->avg_waiting_scaled = (res->total_waiting_time * 100) / res->task_count;
    }
}

void sched_sim_fcfs(const sched_task_t *inputs, uint32_t count, sched_result_t *result)
{
    if (count > SCHED_SIM_MAX_TASKS) count = SCHED_SIM_MAX_TASKS;
    result->task_count = count;
    copy_tasks(inputs, result->tasks, count);

    uint32_t current_time = 0;
    for (uint32_t i = 0; i < count; i++) {
        sched_task_t *t = &result->tasks[i];
        if (current_time < t->arrival_time) {
            current_time = t->arrival_time;
        }
        t->response_time = current_time - t->arrival_time;
        current_time += t->burst_time;
        t->completion_time = current_time;
    }

    compute_metrics(result);
}

void sched_sim_sjf(const sched_task_t *inputs, uint32_t count, sched_result_t *result)
{
    if (count > SCHED_SIM_MAX_TASKS) count = SCHED_SIM_MAX_TASKS;
    result->task_count = count;
    copy_tasks(inputs, result->tasks, count);

    uint32_t completed = 0;
    uint32_t current_time = 0;

    while (completed < count) {
        int best_idx = -1;
        uint32_t min_burst = 0xFFFFFFFF;

        for (uint32_t i = 0; i < count; i++) {
            sched_task_t *t = &result->tasks[i];
            if (t->completion_time == 0 && t->arrival_time <= current_time) {
                if (t->burst_time < min_burst) {
                    min_burst = t->burst_time;
                    best_idx = (int)i;
                }
            }
        }

        if (best_idx == -1) {
            current_time++;
            continue;
        }

        sched_task_t *sel = &result->tasks[best_idx];
        sel->response_time = current_time - sel->arrival_time;
        current_time += sel->burst_time;
        sel->completion_time = current_time;
        completed++;
    }

    compute_metrics(result);
}

void sched_sim_rr(const sched_task_t *inputs, uint32_t count, uint32_t quantum, sched_result_t *result)
{
    if (count > SCHED_SIM_MAX_TASKS) count = SCHED_SIM_MAX_TASKS;
    if (quantum == 0) quantum = 1;
    result->task_count = count;
    copy_tasks(inputs, result->tasks, count);

    uint32_t completed = 0;
    uint32_t current_time = 0;

    while (completed < count) {
        int work_done = 0;

        for (uint32_t i = 0; i < count; i++) {
            sched_task_t *t = &result->tasks[i];

            if (t->remaining_time > 0 && t->arrival_time <= current_time) {
                work_done = 1;

                if (!t->started) {
                    t->response_time = current_time - t->arrival_time;
                    t->started = 1;
                }

                uint32_t slice = (t->remaining_time > quantum) ? quantum : t->remaining_time;
                current_time += slice;
                t->remaining_time -= slice;

                if (t->remaining_time == 0) {
                    t->completion_time = current_time;
                    completed++;
                }
            }
        }

        if (!work_done) {
            current_time++;
        }
    }

    compute_metrics(result);
}

static void print_table(const char *name, const sched_result_t *res)
{
    terminal_write("\n--- Scheduling Algorithm: ");
    terminal_write(name);
    terminal_write(" ---\n");
    terminal_write("ID  Arrival  Burst  Completion  Turnaround  Waiting\n");

    for (uint32_t i = 0; i < res->task_count; i++) {
        const sched_task_t *t = &res->tasks[i];
        terminal_write("P");
        terminal_write_number(t->id);
        terminal_write("   ");
        terminal_write_number(t->arrival_time);
        terminal_write("        ");
        terminal_write_number(t->burst_time);
        terminal_write("      ");
        terminal_write_number(t->completion_time);
        terminal_write("           ");
        terminal_write_number(t->turnaround_time);
        terminal_write("           ");
        terminal_write_number(t->waiting_time);
        terminal_putchar('\n');
    }

    terminal_write("Average Turnaround Time: ");
    terminal_write_number(res->avg_turnaround_scaled / 100);
    terminal_putchar('.');
    terminal_write_number(res->avg_turnaround_scaled % 100);
    terminal_write(" ticks\n");

    terminal_write("Average Waiting Time:    ");
    terminal_write_number(res->avg_waiting_scaled / 100);
    terminal_putchar('.');
    terminal_write_number(res->avg_waiting_scaled % 100);
    terminal_write(" ticks\n");
}

void sched_sim_run_demo(void)
{
    sched_task_t workload[4] = {
        { .id = 1, .arrival_time = 0, .burst_time = 6 },
        { .id = 2, .arrival_time = 1, .burst_time = 8 },
        { .id = 3, .arrival_time = 2, .burst_time = 3 },
        { .id = 4, .arrival_time = 3, .burst_time = 4 }
    };

    sched_result_t res;

    terminal_write("\n========================================\n");
    terminal_write("    CPU SCHEDULING ALGORITHM BENCHMARK  \n");
    terminal_write("========================================\n");
    terminal_write("Workload: 4 tasks (P1..P4)\n");

    sched_sim_fcfs(workload, 4, &res);
    print_table("First-Come, First-Served (FCFS)", &res);

    sched_sim_sjf(workload, 4, &res);
    print_table("Shortest Job First (SJF)", &res);

    sched_sim_rr(workload, 4, 3, &res);
    print_table("Round Robin (RR, Quantum=3)", &res);
}

