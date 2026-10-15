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
    void *arg;
    uint64_t wake_tick;
    uint64_t saved_rsp;
    uint64_t *interrupt_frame;
} process_t;

typedef struct {
    uint32_t total_count;
    uint32_t ready_count;
    uint32_t running_count;
    uint32_t blocked_count;
    uint32_t terminated_count;
} process_stats_t;

void process_init(void);
int process_create(uint64_t entry_point, uint32_t priority);
int thread_create(uint64_t entry_point, void *arg, uint32_t priority);
process_t *process_get(uint32_t pid);
process_t *process_get_by_index(uint32_t index);
int process_terminate(uint32_t pid);
int process_set_state(uint32_t pid, process_state_t state);
int process_block(uint32_t pid);
int process_unblock(uint32_t pid);
int process_sleep(uint64_t ticks);
uint32_t process_get_count(void);
void process_get_stats(process_stats_t *stats);

#endif
