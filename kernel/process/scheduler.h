#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "process.h"

#define SCHEDULER_RR_QUANTUM 5

typedef enum {
    SCHEDULER_FCFS,
    SCHEDULER_SJF,
    SCHEDULER_RR
} scheduler_algorithm_t;

void scheduler_init(void);
void scheduler_set_algorithm(scheduler_algorithm_t algorithm);
scheduler_algorithm_t scheduler_get_algorithm(void);
process_t *scheduler_select_next(void);
void scheduler_tick(void);
void scheduler_start(void);
void scheduler_yield(void);
process_t *scheduler_current_process(void);

#endif
