#include <stdint.h>
#include "syscall.h"
#include "interrupts/idt.h"
#include "memory/heap.h"
#include "process/process.h"

extern volatile uint64_t timer_ticks;

uint64_t syscall_dispatch(
    uint64_t number,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3)
{
    (void)arg3;

    switch (number)
    {
        case SYS_GET_TICKS:
            return timer_ticks;

        case SYS_ALLOCATE_MEMORY:
            return (uint64_t)(uintptr_t)kmalloc((size_t)arg1);

        case SYS_FREE_MEMORY:
            kfree((void *)(uintptr_t)arg1);
            return 0;

        case SYS_CREATE_PROCESS:
        {
            int pid = process_create(arg1, (uint32_t)arg2);
            return (uint64_t)(int64_t)pid;
        }

        case SYS_GET_PROCESS_COUNT:
            return process_get_count();

        case SYS_TERMINATE_PROCESS:
            return (uint64_t)process_terminate((uint32_t)arg1);

        default:
            return (uint64_t)-1;
    }
}
