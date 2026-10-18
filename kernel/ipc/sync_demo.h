#ifndef SYNC_DEMO_H
#define SYNC_DEMO_H

#include <stdint.h>
#include "semaphore.h"

#define MUTEX_SUCCESS          0
#define MUTEX_ERR_BUSY        -1
#define MUTEX_ERR_DEADLOCK    -2   /* Recursive lock attempt by current owner */
#define MUTEX_ERR_NOT_LOCKED  -1   /* Attempted to unlock an unlocked mutex */
#define MUTEX_ERR_NOT_OWNER   -2   /* Attempted to unlock mutex owned by a different PID */

/* Mutex primitive with strict ownership tracking */
typedef struct {
    int locked;
    uint32_t owner_pid;
    uint32_t lock_count;
} mutex_t;

void mutex_init(mutex_t *mutex);
int mutex_lock(mutex_t *mutex);
int mutex_unlock(mutex_t *mutex);
int mutex_try_lock(mutex_t *mutex);
int mutex_is_locked(const mutex_t *mutex);
uint32_t mutex_get_owner(const mutex_t *mutex);

/* Invariant verification functions */
int test_mutex_invariants(void);
int test_sync_invariants(void);

/* Educational demonstrations */
void sync_demo_producer_consumer(void);
void sync_demo_reader_writer(void);
void sync_run_demo(void);

#endif /* SYNC_DEMO_H */
