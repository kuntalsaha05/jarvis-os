#ifndef SYNC_DEMO_H
#define SYNC_DEMO_H

#include <stdint.h>
#include "semaphore.h"

/* Mutex primitive */
typedef struct {
    int locked;
    uint32_t owner_pid;
} mutex_t;

void mutex_init(mutex_t *mutex);
int mutex_lock(mutex_t *mutex);
int mutex_unlock(mutex_t *mutex);
int mutex_try_lock(mutex_t *mutex);

/* Synchronization invariant tests & demonstrations */
void sync_demo_producer_consumer(void);
void sync_demo_reader_writer(void);
void sync_run_demo(void);

#endif /* SYNC_DEMO_H */
