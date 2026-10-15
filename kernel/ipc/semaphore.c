#include "semaphore.h"
#include <stddef.h>

extern void scheduler_yield(void);

void semaphore_init(semaphore_t *sem, int32_t initial_value)
{
    if (sem != NULL)
    {
        sem->value = initial_value < 0 ? 0 : initial_value;
    }
}

int semaphore_wait(semaphore_t *sem)
{
    if (sem == NULL) return -1;

    /*
     * Scheduler-integrated counting semaphore wait:
     * Atomically spins/yields until a resource unit is acquired.
     */
    while (1) {
        while (sem->value <= 0) {
            scheduler_yield();
        }

        if (__sync_sub_and_fetch(&sem->value, 1) >= 0) {
            return 0;
        }

        /* If sub brought it below 0 due to contention, undo and retry */
        __sync_add_and_fetch(&sem->value, 1);
        scheduler_yield();
    }
}

int semaphore_try_wait(semaphore_t *sem)
{
    if (sem == NULL || sem->value <= 0)
    {
        return -1;
    }

    if (__sync_sub_and_fetch(&sem->value, 1) >= 0)
    {
        return 0;
    }

    __sync_add_and_fetch(&sem->value, 1);
    return -1;
}

int semaphore_post(semaphore_t *sem)
{
    if (sem == NULL || sem->value >= 0x7FFFFFFF)
    {
        return -1;
    }

    __sync_add_and_fetch(&sem->value, 1);
    return 0;
}

int32_t semaphore_get_value(const semaphore_t *sem)
{
    return sem != NULL ? sem->value : 0;
}
