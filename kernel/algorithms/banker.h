#ifndef BANKER_H
#define BANKER_H

#include <stdint.h>

#define BANKER_MAX_PROCESSES 8
#define BANKER_MAX_RESOURCES 4

typedef struct {
    uint32_t process_count;
    uint32_t resource_count;
    uint32_t available[BANKER_MAX_RESOURCES];
    uint32_t max[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES];
    uint32_t allocation[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES];
    uint32_t need[BANKER_MAX_PROCESSES][BANKER_MAX_RESOURCES];
} banker_system_t;

typedef struct {
    int is_safe;
    uint32_t safe_sequence[BANKER_MAX_PROCESSES];
} banker_safety_result_t;

/* Banker functions */
void banker_init_system(banker_system_t *sys, uint32_t p_count, uint32_t r_count, const uint32_t *avail);
void banker_compute_need(banker_system_t *sys);
void banker_check_safety(const banker_system_t *sys, banker_safety_result_t *res);
int banker_request_resources(banker_system_t *sys, uint32_t pid, const uint32_t *request);

/* Educational demonstration */
void banker_run_demo(void);

#endif /* BANKER_H */

