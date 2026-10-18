#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdint.h>

#define SEM_WAIT_QUEUE_CAPACITY 8

typedef struct
{
    volatile int32_t value;
    int32_t max_value;
    uint32_t wait_queue[SEM_WAIT_QUEUE_CAPACITY];
    uint32_t wait_count;
} semaphore_t;

void semaphore_init(semaphore_t *sem, int32_t initial_value);
void semaphore_init_bounded(semaphore_t *sem, int32_t initial_value, int32_t max_value);
int semaphore_wait(semaphore_t *sem);
int semaphore_try_wait(semaphore_t *sem);
int semaphore_post(semaphore_t *sem);
int32_t semaphore_get_value(const semaphore_t *sem);
int test_semaphore_invariants(void);

#endif
