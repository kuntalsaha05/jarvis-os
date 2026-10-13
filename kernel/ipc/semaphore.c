#include "semaphore.h"

void semaphore_init(semaphore_t *sem, int32_t initial_value)
{
    if (sem != 0)
    {
        sem->value = initial_value < 0 ? 0 : initial_value;
    }
}

int semaphore_try_wait(semaphore_t *sem)
{
    if (sem == 0 || sem->value <= 0)
    {
        return -1;
    }

    sem->value--;
    return 0;
}

int semaphore_post(semaphore_t *sem)
{
    if (sem == 0 || sem->value == INT32_MAX)
    {
        return -1;
    }

    sem->value++;
    return 0;
}
