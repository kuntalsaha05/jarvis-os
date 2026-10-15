#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdint.h>

typedef struct
{
    volatile int32_t value;
} semaphore_t;

void semaphore_init(semaphore_t *sem, int32_t initial_value);
int semaphore_wait(semaphore_t *sem);
int semaphore_try_wait(semaphore_t *sem);
int semaphore_post(semaphore_t *sem);
int32_t semaphore_get_value(const semaphore_t *sem);

#endif
