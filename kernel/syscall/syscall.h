#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

typedef enum
{
    SYS_GET_TICKS = 0,
    SYS_ALLOCATE_MEMORY,
    SYS_FREE_MEMORY,
    SYS_CREATE_PROCESS,
    SYS_GET_PROCESS_COUNT,
    SYS_TERMINATE_PROCESS
} syscall_number_t;

uint64_t syscall_dispatch(
    uint64_t number,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3
);

#endif
