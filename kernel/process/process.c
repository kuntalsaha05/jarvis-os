#include "process.h"
#include <stddef.h>
#include <stdint.h>
#include "../interrupts/idt.h"

extern volatile uint64_t timer_ticks;
extern void process_task_trampoline(void);

static process_t process_table[MAX_PROCESSES];
static uint8_t process_stacks[MAX_PROCESSES][PROCESS_STACK_SIZE]
    __attribute__((aligned(16)));
static uint32_t next_pid = 1;

void process_init(void)
{
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        process_table[i] = (process_t){0};
        process_table[i].state = PROCESS_TERMINATED;
    }

    next_pid = 1;
}

int process_create(uint64_t entry_point, uint32_t priority)
{
    if (entry_point == 0)
        return -1;

    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        process_t *p = &process_table[i];

        if (p->state != PROCESS_TERMINATED)
            continue;

        p->pid = next_pid++;
        p->state = PROCESS_READY;
        p->priority = priority;
        p->creation_tick = timer_ticks;
        p->cpu_time = 0;
        p->entry_point = entry_point;
        p->context = (cpu_context_t){0};
        p->interrupt_frame = NULL;

        uintptr_t top =
            (uintptr_t)&process_stacks[i][PROCESS_STACK_SIZE];
        top = (top & ~(uintptr_t)0xF) - 8;

        /*
         * x86-64 IRETQ stack layout:
         * frame[0..14] = 15 GPRs (r15..rax)
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

process_t *process_get(uint32_t pid)
{
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
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

    return 0;
}

int process_set_state(uint32_t pid, process_state_t state)
{
    process_t *p = process_get(pid);

    if (p == NULL || state < PROCESS_NEW || state > PROCESS_TERMINATED)
        return -1;

    p->state = state;
    return 0;
}

uint32_t process_get_count(void)
{
    uint32_t count = 0;

    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROCESS_TERMINATED)
            count++;
    }

    return count;
}
