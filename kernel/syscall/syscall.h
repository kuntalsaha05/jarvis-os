#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

typedef enum
{
    SYS_GET_TICKS = 0,
    SYS_ALLOCATE_MEMORY = 1,
    SYS_FREE_MEMORY = 2,
    SYS_CREATE_PROCESS = 3,
    SYS_GET_PROCESS_COUNT = 4,
    SYS_TERMINATE_PROCESS = 5
} syscall_number_t;

#define SYSCALL_MAX 5

/* Invokes system call via hardware/software interrupt int $0x80 */
uint64_t syscall(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3);

/* Dispatcher invoked by assembly stub syscall_stub */
uint64_t syscall_dispatch(
    uint64_t number,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3
);

/* Diagnostic demonstration of syscall entry */
void syscall_run_demo(void);

#endif /* SYSCALL_H */
