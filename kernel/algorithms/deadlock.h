#ifndef DEADLOCK_H
#define DEADLOCK_H

#include <stdint.h>

#define DEADLOCK_MAX_PROCS 8
#define DEADLOCK_MAX_RES   4

/* Deadlock Detection Structures */
typedef struct {
    uint32_t process_count;
    uint32_t resource_count;
    uint32_t available[DEADLOCK_MAX_RES];
    uint32_t allocation[DEADLOCK_MAX_PROCS][DEADLOCK_MAX_RES];
    uint32_t request[DEADLOCK_MAX_PROCS][DEADLOCK_MAX_RES];
} deadlock_detection_system_t;

typedef struct {
    int deadlock_detected;
    uint32_t deadlocked_procs[DEADLOCK_MAX_PROCS];
    uint32_t deadlocked_count;
} deadlock_detection_result_t;

/* Detection API */
void deadlock_detect(const deadlock_detection_system_t *sys, deadlock_detection_result_t *res);

/* Prevention Demonstration */
void deadlock_prevention_demo(void);
void deadlock_detection_demo(void);
void deadlock_run_demo(void);

#endif /* DEADLOCK_H */

