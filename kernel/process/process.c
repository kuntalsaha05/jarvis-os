#include "process.h"
#include <stddef.h>
#include <stdint.h>
#include "../interrupts/idt.h"

extern volatile uint64_t timer_ticks;
extern void process_task_trampoline(void);
extern process_t *scheduler_current_process(void);
extern void scheduler_yield(void);

static process_t process_table[MAX_PROCESSES];
static uint8_t process_stacks[MAX_PROCESSES][PROCESS_STACK_SIZE]
    __attribute__((aligned(16)));
static uint32_t next_pid = 1;

int process_is_valid_transition(process_state_t from, process_state_t to)
{
    if (from == to)
        return 1;

    switch (from) {
        case PROCESS_NEW:
            return (to == PROCESS_READY || to == PROCESS_TERMINATED);
        case PROCESS_READY:
            return (to == PROCESS_RUNNING || to == PROCESS_TERMINATED);
        case PROCESS_RUNNING:
            return (to == PROCESS_READY || to == PROCESS_BLOCKED || to == PROCESS_TERMINATED);
        case PROCESS_BLOCKED:
            return (to == PROCESS_READY || to == PROCESS_TERMINATED);
        case PROCESS_TERMINATED:
            return (to == PROCESS_NEW || to == PROCESS_READY);
        default:
            return 0;
    }
}

static void idle_task_entry(void *arg)
{
    (void)arg;
    for (;;) {
        __asm__ volatile ("sti; hlt");
    }
}

void process_init(void)
{
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        process_table[i] = (process_t){0};
        process_table[i].state = PROCESS_TERMINATED;
    }

    next_pid = 1;

    /* Initialize idle task at index 0 (PID 0) */
    process_t *idle = &process_table[0];
    idle->pid = 0;
    idle->state = PROCESS_READY;
    idle->priority = 0;
    idle->creation_tick = 0;
    idle->cpu_time = 0;
    idle->entry_point = (uint64_t)(uintptr_t)idle_task_entry;
    idle->arg = NULL;
    idle->wake_tick = 0;

    uintptr_t top =
        (uintptr_t)&process_stacks[0][PROCESS_STACK_SIZE];
    top = (top & ~(uintptr_t)0xF) - 8;

    uint64_t *frame =
        (uint64_t *)(top - 20 * sizeof(uint64_t));
    for (uint32_t j = 0; j < 20; j++)
        frame[j] = 0;

    frame[15] = (uint64_t)(uintptr_t)idle_task_entry;
    frame[16] = 0x08;
    frame[17] = 0x202;
    frame[18] = (uint64_t)top;
    frame[19] = 0x10;

    idle->interrupt_frame = frame;
    idle->saved_rsp = (uint64_t)(uintptr_t)frame;
}

int thread_create(uint64_t entry_point, void *arg, uint32_t priority)
{
    if (entry_point == 0)
        return -1;

    for (uint32_t i = 1; i < MAX_PROCESSES; i++) {
        process_t *p = &process_table[i];

        if (p->state != PROCESS_TERMINATED)
            continue;

        p->pid = next_pid++;
        p->state = PROCESS_READY;
        p->priority = priority;
        p->creation_tick = timer_ticks;
        p->cpu_time = 0;
        p->entry_point = entry_point;
        p->arg = arg;
        p->wake_tick = 0;
        p->context = (cpu_context_t){0};
        p->interrupt_frame = NULL;

        uintptr_t top =
            (uintptr_t)&process_stacks[i][PROCESS_STACK_SIZE];
        top = (top & ~(uintptr_t)0xF) - 8;

        /*
         * x86-64 IRETQ stack layout:
         * frame[0..14] = 15 GPRs (r15..rax)
         * frame[8]     = RDI (first argument to entry_point)
         * frame[15]    = RIP
         * frame[16]    = CS (0x08)
         * frame[17]    = RFLAGS (0x202, IF=1)
         * frame[18]    = RSP (top)
         * frame[19]    = SS (0x10)
         */
        uint64_t *frame =
            (uint64_t *)(top - 20 * sizeof(uint64_t));
        for (uint32_t j = 0; j < 20; j++)
            frame[j] = 0;

        frame[8] = (uint64_t)(uintptr_t)arg;
        frame[15] = (uint64_t)(uintptr_t)process_task_trampoline;
        frame[16] = 0x08;
        frame[17] = 0x202;
        frame[18] = (uint64_t)top;
        frame[19] = 0x10;

        p->interrupt_frame = frame;
        p->saved_rsp = (uint64_t)(uintptr_t)frame;
        return (int)p->pid;
    }

    return -1;
}

int process_create(uint64_t entry_point, uint32_t priority)
{
    return thread_create(entry_point, NULL, priority);
}

process_t *process_get(uint32_t pid)
{
    if (pid == 0)
        return &process_table[0];

    for (uint32_t i = 1; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid &&
            process_table[i].state != PROCESS_TERMINATED)
            return &process_table[i];
    }

    return NULL;
}

process_t *process_get_by_index(uint32_t index)
{
    if (index >= MAX_PROCESSES ||
        process_table[index].state == PROCESS_TERMINATED)
        return NULL;

    return &process_table[index];
}

int process_terminate(uint32_t pid)
{
    process_t *p = process_get(pid);
    if (p == NULL)
        return -1;

    p->state = PROCESS_TERMINATED;
    p->context = (cpu_context_t){0};
    p->saved_rsp = 0;
    p->interrupt_frame = NULL;
    p->entry_point = 0;
    p->arg = NULL;
    p->wake_tick = 0;

    return 0;
}

int process_set_state(uint32_t pid, process_state_t state)
{
    process_t *p = process_get(pid);

    if (p == NULL || state < PROCESS_NEW || state > PROCESS_TERMINATED)
        return -1;

    if (!process_is_valid_transition(p->state, state))
        return -1;

    p->state = state;
    return 0;
}

int process_block(uint32_t pid)
{
    return process_set_state(pid, PROCESS_BLOCKED);
}

int process_unblock(uint32_t pid)
{
    return process_set_state(pid, PROCESS_READY);
}

int process_sleep(uint64_t ticks)
{
    process_t *p = scheduler_current_process();
    if (!p) return -1;

    p->wake_tick = timer_ticks + ticks;
    p->state = PROCESS_BLOCKED;
    scheduler_yield();
    return 0;
}

uint32_t process_get_count(void)
{
    uint32_t count = 0;

    for (uint32_t i = 1; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROCESS_TERMINATED)
            count++;
    }

    return count;
}

void process_get_stats(process_stats_t *stats)
{
    if (!stats) return;
    stats->total_count = 0;
    stats->ready_count = 0;
    stats->running_count = 0;
    stats->blocked_count = 0;
    stats->terminated_count = 0;

    for (uint32_t i = 1; i < MAX_PROCESSES; i++) {
        switch (process_table[i].state) {
            case PROCESS_READY:
                stats->ready_count++;
                stats->total_count++;
                break;
            case PROCESS_RUNNING:
                stats->running_count++;
                stats->total_count++;
                break;
            case PROCESS_BLOCKED:
                stats->blocked_count++;
                stats->total_count++;
                break;
            case PROCESS_TERMINATED:
                stats->terminated_count++;
                break;
            default:
                break;
        }
    }
}
