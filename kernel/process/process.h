#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define MAX_PROCESSES 32
#define PROCESS_STACK_SIZE 4096

typedef enum {
    PROCESS_NEW,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
} process_state_t;

typedef struct {
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rip;
    uint64_t rflags;
} cpu_context_t;

typedef struct {
    uint32_t pid;
    process_state_t state;
    uint32_t priority;
    cpu_context_t context;
    uint64_t creation_tick;
    uint64_t cpu_time;
    uint64_t entry_point;
    uint64_t saved_rsp;
} process_t;

void process_init(void);
int process_create(uint64_t entry_point, uint32_t priority);
process_t *process_get(uint32_t pid);
process_t *process_get_by_index(uint32_t index);
int process_terminate(uint32_t pid);
int process_set_state(uint32_t pid, process_state_t state);
uint32_t process_get_count(void);

#endif
