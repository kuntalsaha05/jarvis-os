#include "scheduler.h"
#include <stddef.h>
#include <stdint.h>

extern void context_switch(uint64_t *old_rsp, uint64_t new_rsp);
extern void process_task_trampoline(void);

static scheduler_algorithm_t current_algorithm = SCHEDULER_FCFS;
static process_t *current_process;
static uint64_t bootstrap_rsp;
static int scheduler_started;

void scheduler_init(void)
{
    current_algorithm = SCHEDULER_FCFS;
    current_process = NULL;
    bootstrap_rsp = 0;
    scheduler_started = 0;
}

void scheduler_set_algorithm(scheduler_algorithm_t algorithm)
{
    if (algorithm >= SCHEDULER_FCFS && algorithm <= SCHEDULER_RR)
        current_algorithm = algorithm;
}

scheduler_algorithm_t scheduler_get_algorithm(void)
{
    return current_algorithm;
}

process_t *scheduler_select_next(void)
{
    process_t *selected = NULL;

    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        process_t *p = process_get_by_index(i);

        if (p == NULL || p->state != PROCESS_READY)
            continue;

        if (selected == NULL ||
            (current_algorithm == SCHEDULER_SJF &&
             p->cpu_time < selected->cpu_time) ||
            (current_algorithm == SCHEDULER_FCFS &&
             p->creation_tick < selected->creation_tick)) {
            selected = p;
        }
    }

    return selected;
}

process_t *scheduler_current_process(void)
{
    return current_process;
}

void scheduler_start(void)
{
    if (scheduler_started)
        return;

    process_t *next = scheduler_select_next();
    if (next == NULL)
        return;

    scheduler_started = 1;
    current_process = next;
    next->state = PROCESS_RUNNING;

    context_switch(&bootstrap_rsp, next->saved_rsp);

    current_process = NULL;
    scheduler_started = 0;
}

void scheduler_yield(void)
{
    if (!scheduler_started || current_process == NULL)
        return;

    process_t *previous = current_process;

    if (previous->state == PROCESS_RUNNING)
        previous->state = PROCESS_READY;

    process_t *next = scheduler_select_next();

    if (next == NULL) {
        if (previous->state == PROCESS_READY) {
            previous->state = PROCESS_RUNNING;
            return;
        }

        current_process = NULL;
        context_switch(&previous->saved_rsp, bootstrap_rsp);
        return;
    }

    if (next == previous) {
        previous->state = PROCESS_RUNNING;
        return;
    }

    current_process = next;
    next->state = PROCESS_RUNNING;
    next->cpu_time++;

    context_switch(&previous->saved_rsp, next->saved_rsp);
}

void scheduler_tick(void)
{
    /* Timer ticks do not preempt tasks in this cooperative stage. */
    if (current_process != NULL &&
        current_process->state == PROCESS_RUNNING)
        current_process->cpu_time++;
}

void process_task_trampoline(void)
{
    process_t *p = scheduler_current_process();

    if (p != NULL) {
        void (*entry)(void) = (void (*)(void))(uintptr_t)p->entry_point;
        entry();
        process_terminate(p->pid);
    }

    scheduler_yield();

    for (;;)
        __asm__ volatile ("cli; hlt");
}
