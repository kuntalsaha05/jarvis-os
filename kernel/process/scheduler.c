#include "scheduler.h"
#include <stdint.h>
#include "../interrupts/pic.h"

extern volatile uint64_t timer_ticks;

static scheduler_algorithm_t current_algorithm = SCHEDULER_FCFS;
static process_t *current_process;
static uint64_t *bootstrap_frame;
static int scheduler_started;
static uint32_t rr_cursor;
static uint32_t quantum_ticks;
static uint32_t rr_quantum = SCHEDULER_RR_QUANTUM;
static scheduler_stats_t sched_stats = {0};

void scheduler_init(void)
{
    current_algorithm = SCHEDULER_FCFS;
    current_process = 0;
    bootstrap_frame = 0;
    scheduler_started = 0;
    rr_cursor = MAX_PROCESSES - 1;
    quantum_ticks = 0;
    rr_quantum = SCHEDULER_RR_QUANTUM;
    sched_stats = (scheduler_stats_t){0};
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

void scheduler_set_quantum(uint32_t quantum)
{
    if (quantum > 0 && quantum <= 100)
        rr_quantum = quantum;
}

uint32_t scheduler_get_quantum(void)
{
    return rr_quantum;
}

void scheduler_get_stats(scheduler_stats_t *stats)
{
    if (!stats) return;
    *stats = sched_stats;
    stats->timer_ticks = timer_ticks;
}

process_t *scheduler_select_next(void)
{
    process_t *selected = 0;

    if (current_algorithm == SCHEDULER_RR) {
        for (uint32_t offset = 1; offset <= MAX_PROCESSES; offset++) {
            uint32_t i = (rr_cursor + offset) % MAX_PROCESSES;
            process_t *p = process_get_by_index(i);
            if (p != 0 && p->state == PROCESS_READY) {
                rr_cursor = i;
                return p;
            }
        }
        return 0;
    }

    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        process_t *p = process_get_by_index(i);
        if (p == 0 || p->state != PROCESS_READY)
            continue;

        if (selected == 0 ||
            (current_algorithm == SCHEDULER_SJF &&
             p->cpu_time < selected->cpu_time) ||
            (current_algorithm == SCHEDULER_FCFS &&
             p->creation_tick < selected->creation_tick))
            selected = p;
    }
    return selected;
}

process_t *scheduler_current_process(void)
{
    return current_process;
}

/* Return the saved frame that the assembly stub must restore with IRETQ. */
uint64_t *scheduler_interrupt(uint64_t *frame, uint64_t from_timer)
{
    if (from_timer) {
        timer_ticks++;
        pic_send_eoi(0);
        sched_stats.timer_ticks = timer_ticks;

        /* Wake up sleeping processes whose sleep timeout has elapsed */
        for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
            process_t *p = process_get_by_index(i);
            if (p && p->state == PROCESS_BLOCKED && p->wake_tick > 0 && timer_ticks >= p->wake_tick) {
                p->state = PROCESS_READY;
                p->wake_tick = 0;
            }
        }
    } else {
        sched_stats.voluntary_yields++;
    }

    if (!scheduler_started) {
        if (from_timer)
            return frame;

        bootstrap_frame = frame;
        process_t *first = scheduler_select_next();
        if (first == 0)
            return frame;

        scheduler_started = 1;
        current_process = first;
        first->state = PROCESS_RUNNING;
        quantum_ticks = 0;
        sched_stats.context_switches++;
        return first->interrupt_frame;
    }

    process_t *previous = current_process;
    if (previous == 0)
        return bootstrap_frame != 0 ? bootstrap_frame : frame;

    if (from_timer && previous->state == PROCESS_RUNNING) {
        previous->cpu_time++;

        if (current_algorithm != SCHEDULER_RR)
            return frame;

        if (++quantum_ticks < rr_quantum)
            return frame;

        sched_stats.quantum_preemptions++;
    }

    if (previous->state != PROCESS_TERMINATED) {
        previous->interrupt_frame = frame;
        previous->saved_rsp = (uint64_t)(uintptr_t)frame;
        if (previous->state == PROCESS_RUNNING)
            previous->state = PROCESS_READY;
    }

    process_t *next = scheduler_select_next();

    if (next == 0) {
        if (previous->state == PROCESS_READY) {
            previous->state = PROCESS_RUNNING;
            quantum_ticks = 0;
            return frame;
        }

        current_process = 0;
        scheduler_started = 0;
        quantum_ticks = 0;
        return bootstrap_frame != 0 ? bootstrap_frame : frame;
    }

    if (next == previous) {
        previous->state = PROCESS_RUNNING;
        quantum_ticks = 0;
        return frame;
    }

    current_process = next;
    next->state = PROCESS_RUNNING;
    quantum_ticks = 0;
    sched_stats.context_switches++;
    return next->interrupt_frame;
}

void scheduler_start(void)
{
    if (!scheduler_started)
        __asm__ volatile ("int $0x81" ::: "memory");
}

void scheduler_yield(void)
{
    if (scheduler_started && current_process != 0)
        __asm__ volatile ("int $0x81" ::: "memory");
}

void scheduler_tick(void)
{
    if (current_process != 0 &&
        current_process->state == PROCESS_RUNNING)
        current_process->cpu_time++;
}

void process_task_trampoline(void)
{
    process_t *p = scheduler_current_process();
    if (p != 0) {
        void (*entry)(void *) = (void (*)(void *))(uintptr_t)p->entry_point;
        entry(p->arg);
        process_terminate(p->pid);
    }

    scheduler_yield();
    for (;;)
        __asm__ volatile ("cli; hlt");
}
