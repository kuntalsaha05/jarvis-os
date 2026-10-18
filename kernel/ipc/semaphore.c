#include "semaphore.h"
#include <stddef.h>
#include "../process/process.h"
#include "../process/scheduler.h"

void semaphore_init(semaphore_t *sem, int32_t initial_value)
{
    semaphore_init_bounded(sem, initial_value, 0x7FFFFFFF);
}

void semaphore_init_bounded(semaphore_t *sem, int32_t initial_value, int32_t max_value)
{
    if (sem != NULL)
    {
        sem->value = initial_value < 0 ? 0 : initial_value;
        sem->max_value = max_value <= 0 ? 0x7FFFFFFF : max_value;
        sem->wait_count = 0;
        for (uint32_t i = 0; i < SEM_WAIT_QUEUE_CAPACITY; i++) {
            sem->wait_queue[i] = 0;
        }
    }
}

int semaphore_wait(semaphore_t *sem)
{
    if (sem == NULL) return -1;

    while (1) {
        if (sem->value > 0) {
            if (__sync_sub_and_fetch(&sem->value, 1) >= 0) {
                return 0;
            }
            __sync_add_and_fetch(&sem->value, 1);
        }

        process_t *cur = scheduler_current_process();
        if (cur != NULL && cur->pid > 0) {
            if (sem->wait_count < SEM_WAIT_QUEUE_CAPACITY) {
                sem->wait_queue[sem->wait_count++] = cur->pid;
                process_block(cur->pid);
                scheduler_yield();
                continue;
            }
        }

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
    if (sem == NULL || sem->value >= sem->max_value)
    {
        return -1;
    }

    __sync_add_and_fetch(&sem->value, 1);

    if (sem->wait_count > 0) {
        uint32_t wake_pid = sem->wait_queue[0];
        for (uint32_t i = 0; i < sem->wait_count - 1; i++) {
            sem->wait_queue[i] = sem->wait_queue[i + 1];
        }
        sem->wait_queue[sem->wait_count - 1] = 0;
        sem->wait_count--;

        process_unblock(wake_pid);
    }

    return 0;
}

int32_t semaphore_get_value(const semaphore_t *sem)
{
    return sem != NULL ? sem->value : 0;
}

int test_semaphore_invariants(void)
{
    semaphore_t sem;

    /* 1. NULL parameter check */
    if (semaphore_wait(NULL) != -1) return -1;
    if (semaphore_try_wait(NULL) != -1) return -2;
    if (semaphore_post(NULL) != -1) return -3;

    /* 2. Bounded initialization: initial=2, max=4 */
    semaphore_init_bounded(&sem, 2, 4);
    if (semaphore_get_value(&sem) != 2) return -4;

    /* 3. Acquire 1st unit */
    if (semaphore_try_wait(&sem) != 0) return -5;
    if (semaphore_get_value(&sem) != 1) return -6;

    /* 4. Acquire 2nd unit -> reaches 0 */
    if (semaphore_try_wait(&sem) != 0) return -7;
    if (semaphore_get_value(&sem) != 0) return -8;

    /* 5. 3rd acquire must be rejected on empty */
    if (semaphore_try_wait(&sem) != -1) return -9;
    if (semaphore_get_value(&sem) != 0) return -10;

    /* 6. Release units up to max */
    if (semaphore_post(&sem) != 0) return -11; /* val=1 */
    if (semaphore_post(&sem) != 0) return -12; /* val=2 */
    if (semaphore_post(&sem) != 0) return -13; /* val=3 */
    if (semaphore_post(&sem) != 0) return -14; /* val=4 (max) */
    if (semaphore_get_value(&sem) != 4) return -15;

    /* 7. Post beyond max must be rejected */
    if (semaphore_post(&sem) != -1) return -16;
    if (semaphore_get_value(&sem) != 4) return -17;

    return 0;
}
