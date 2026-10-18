#include "sync_demo.h"
#include "semaphore.h"
#include <stddef.h>
#include "../process/process.h"

#include "../process/scheduler.h"
#include "../drivers/terminal.h"

static uint32_t get_current_pid(void)
{
    process_t *cur = scheduler_current_process();
    return (cur != 0 && cur->pid > 0) ? cur->pid : 1;
}

void mutex_init(mutex_t *mutex)
{
    if (mutex) {
        mutex->locked = 0;
        mutex->owner_pid = 0;
        mutex->lock_count = 0;
    }
}

int mutex_try_lock(mutex_t *mutex)
{
    if (!mutex) return -1;
    uint32_t pid = get_current_pid();

    if (mutex->locked && mutex->owner_pid == pid) {
        return MUTEX_ERR_DEADLOCK;
    }

    if (__sync_lock_test_and_set(&mutex->locked, 1) == 0) {
        mutex->owner_pid = pid;
        mutex->lock_count++;
        return MUTEX_SUCCESS;
    }
    return MUTEX_ERR_BUSY;
}

int mutex_lock(mutex_t *mutex)
{
    if (!mutex) return -1;
    uint32_t pid = get_current_pid();

    if (mutex->locked && mutex->owner_pid == pid) {
        return MUTEX_ERR_DEADLOCK;
    }

    while (__sync_lock_test_and_set(&mutex->locked, 1)) {
        scheduler_yield();
    }

    mutex->owner_pid = pid;
    mutex->lock_count++;
    return MUTEX_SUCCESS;
}

int mutex_unlock(mutex_t *mutex)
{
    if (!mutex || !mutex->locked) return MUTEX_ERR_NOT_LOCKED;
    uint32_t pid = get_current_pid();

    /* Strict ownership check: only the holding task can unlock */
    if (mutex->owner_pid != pid) {
        return MUTEX_ERR_NOT_OWNER;
    }

    mutex->owner_pid = 0;
    __sync_lock_release(&mutex->locked);
    return MUTEX_SUCCESS;
}

int mutex_is_locked(const mutex_t *mutex)
{
    return (mutex != 0) ? mutex->locked : 0;
}

uint32_t mutex_get_owner(const mutex_t *mutex)
{
    return (mutex != 0) ? mutex->owner_pid : 0;
}

int test_mutex_invariants(void)
{
    mutex_t m;

    /* 1. NULL check */
    if (mutex_lock(NULL) != -1) return -1;
    if (mutex_try_lock(NULL) != -1) return -2;
    if (mutex_unlock(NULL) != MUTEX_ERR_NOT_LOCKED) return -3;

    /* 2. Initialization invariant */
    mutex_init(&m);
    if (mutex_is_locked(&m) != 0 || mutex_get_owner(&m) != 0) return -4;

    /* 3. Unlock on unlocked mutex rejected */
    if (mutex_unlock(&m) != MUTEX_ERR_NOT_LOCKED) return -5;

    /* 4. Normal lock acquisition */
    if (mutex_lock(&m) != MUTEX_SUCCESS) return -6;
    if (mutex_is_locked(&m) != 1) return -7;
    uint32_t current_pid = get_current_pid();
    if (mutex_get_owner(&m) != current_pid) return -8;

    /* 5. Recursive re-lock attempt must be rejected to prevent self-deadlock */
    if (mutex_try_lock(&m) != MUTEX_ERR_DEADLOCK) return -9;
    if (mutex_lock(&m) != MUTEX_ERR_DEADLOCK) return -10;

    /* 6. Unauthorized unlock: non-owner task attempted unlock must be rejected */
    m.owner_pid = current_pid + 100; /* Simulate different owner task */
    if (mutex_unlock(&m) != MUTEX_ERR_NOT_OWNER) return -11;
    if (mutex_is_locked(&m) != 1) return -12; /* Must remain locked */

    /* 7. Authorized unlock */
    m.owner_pid = current_pid;
    if (mutex_unlock(&m) != MUTEX_SUCCESS) return -13;
    if (mutex_is_locked(&m) != 0 || mutex_get_owner(&m) != 0) return -14;

    return 0;
}

/* --- Producer-Consumer Implementation --- */
#define BUFFER_SIZE 4

static uint32_t buffer[BUFFER_SIZE];
static uint32_t in_idx = 0;
static uint32_t out_idx = 0;
static uint32_t item_count = 0;

static semaphore_t sem_empty;
static semaphore_t sem_full;
static mutex_t buffer_mutex;

static int pc_produce(uint32_t item)
{
    if (semaphore_try_wait(&sem_empty) != 0) {
        return -1; /* Buffer full */
    }
    mutex_lock(&buffer_mutex);

    buffer[in_idx] = item;
    in_idx = (in_idx + 1) % BUFFER_SIZE;
    item_count++;

    mutex_unlock(&buffer_mutex);
    semaphore_post(&sem_full);
    return 0;
}

static int pc_consume(uint32_t *item)
{
    if (semaphore_try_wait(&sem_full) != 0) {
        return -1; /* Buffer empty */
    }
    mutex_lock(&buffer_mutex);

    *item = buffer[out_idx];
    out_idx = (out_idx + 1) % BUFFER_SIZE;
    item_count--;

    mutex_unlock(&buffer_mutex);
    semaphore_post(&sem_empty);
    return 0;
}

void sync_demo_producer_consumer(void)
{
    terminal_write("\n1. Producer-Consumer Bounded Buffer Test:\n");
    terminal_write("   Capacity: 4 items, Semaphores: empty=4, full=0, mutex=1\n");

    semaphore_init_bounded(&sem_empty, BUFFER_SIZE, BUFFER_SIZE);
    semaphore_init_bounded(&sem_full, 0, BUFFER_SIZE);
    mutex_init(&buffer_mutex);
    in_idx = out_idx = item_count = 0;

    int passed = 1;

    /* Produce 4 items (fills buffer) */
    for (uint32_t i = 101; i <= 104; i++) {
        if (pc_produce(i) != 0) passed = 0;
    }

    /* 5th produce should be rejected (bounded buffer full) */
    if (pc_produce(999) == 0) {
        passed = 0; /* Should have failed */
    }

    /* Consume items and verify FIFO order */
    uint32_t consumed[4];
    for (int i = 0; i < 4; i++) {
        if (pc_consume(&consumed[i]) != 0) passed = 0;
        if (consumed[i] != (uint32_t)(101 + i)) passed = 0;
    }

    /* Next consume should fail (buffer empty) */
    uint32_t dummy;
    if (pc_consume(&dummy) == 0) {
        passed = 0; /* Should have failed */
    }

    if (passed && item_count == 0) {
        terminal_write("   Result: PASS (Bounded buffer, FIFO order, & full/empty invariants verified)\n");
    } else {
        terminal_write("   Result: FAIL\n");
    }
}

/* --- Reader-Writer Implementation --- */
static uint32_t shared_data = 0;
static uint32_t read_count = 0;
static mutex_t rw_mutex;
static mutex_t count_mutex;
static int active_writers = 0;
static int max_concurrent_readers = 0;

static void reader_enter(void)
{
    mutex_lock(&count_mutex);
    read_count++;
    if (read_count > (uint32_t)max_concurrent_readers) {
        max_concurrent_readers = read_count;
    }
    if (read_count == 1) {
        mutex_lock(&rw_mutex); /* First reader locks shared resource against writers */
    }
    mutex_unlock(&count_mutex);
}

static void reader_exit(void)
{
    mutex_lock(&count_mutex);
    read_count--;
    if (read_count == 0) {
        mutex_unlock(&rw_mutex); /* Last reader releases shared resource for writers */
    }
    mutex_unlock(&count_mutex);
}

static int writer_enter(void)
{
    if (mutex_try_lock(&rw_mutex) != 0) {
        return -1; /* Resource locked by readers or another writer */
    }
    active_writers++;
    return 0;
}

static void writer_exit(void)
{
    active_writers--;
    mutex_unlock(&rw_mutex);
}

void sync_demo_reader_writer(void)
{
    terminal_write("\n2. Reader-Writer Concurrency Test:\n");

    mutex_init(&rw_mutex);
    mutex_init(&count_mutex);
    read_count = 0;
    active_writers = 0;
    max_concurrent_readers = 0;
    shared_data = 42;

    int passed = 1;

    /* Simulate multiple concurrent readers */
    reader_enter();
    reader_enter();
    reader_enter();

    /* Invariant 1: Readers share access concurrently */
    if (max_concurrent_readers != 3 || read_count != 3) passed = 0;

    /* Invariant 2: Writer MUST be locked out while readers are active */
    if (writer_enter() == 0) {
        passed = 0; /* Writer should not enter */
        writer_exit();
    }

    /* Readers exit */
    reader_exit();
    reader_exit();
    reader_exit();

    /* Invariant 3: Writer enters exclusively after readers leave */
    if (writer_enter() != 0) {
        passed = 0;
    } else {
        shared_data = 100;
        /* Another reader should be locked out while writer active */
        if (mutex_try_lock(&rw_mutex) == 0) {
            passed = 0;
            mutex_unlock(&rw_mutex);
        }
        writer_exit();
    }

    if (passed && shared_data == 100 && read_count == 0) {
        terminal_write("   Result: PASS (Concurrent read allowed; exclusive write enforced)\n");
    } else {
        terminal_write("   Result: FAIL\n");
    }
}

int test_sync_invariants(void)
{
    /* 1. Mutex ownership invariants */
    if (test_mutex_invariants() != 0) return -1;

    /* 2. Semaphore counting invariants */
    if (test_semaphore_invariants() != 0) return -2;

    /* 3. Producer-Consumer buffer invariants */
    semaphore_init_bounded(&sem_empty, BUFFER_SIZE, BUFFER_SIZE);
    semaphore_init_bounded(&sem_full, 0, BUFFER_SIZE);
    mutex_init(&buffer_mutex);
    in_idx = out_idx = item_count = 0;

    for (uint32_t i = 1; i <= BUFFER_SIZE; i++) {
        if (pc_produce(i * 10) != 0) return -3;
    }
    if (pc_produce(99) == 0) return -4; /* Overflow must fail */

    for (uint32_t i = 1; i <= BUFFER_SIZE; i++) {
        uint32_t val = 0;
        if (pc_consume(&val) != 0 || val != i * 10) return -5;
    }
    uint32_t d = 0;
    if (pc_consume(&d) == 0) return -6; /* Underflow must fail */

    /* 4. Reader-Writer invariants */
    mutex_init(&rw_mutex);
    mutex_init(&count_mutex);
    read_count = 0;
    active_writers = 0;
    max_concurrent_readers = 0;
    shared_data = 50;

    reader_enter();
    reader_enter();
    if (read_count != 2) return -7;
    if (writer_enter() == 0) return -8; /* Writer locked out */
    reader_exit();
    reader_exit();
    if (writer_enter() != 0) return -9; /* Writer enters */
    writer_exit();

    return 0;
}

void sync_run_demo(void)
{
    terminal_write("\n========================================\n");
    terminal_write("     SYNCHRONIZATION & IPC BENCHMARK    \n");
    terminal_write("========================================\n");

    sync_demo_producer_consumer();
    sync_demo_reader_writer();

    terminal_write("\n3. Mutex Ownership & Access Protection:\n");
    int m_res = test_mutex_invariants();
    if (m_res == 0) {
        terminal_write("   Result: PASS (Owner PID tracked; non-owner unlock rejected; recursive lock rejected)\n");
    } else {
        terminal_write("   Result: FAIL\n");
    }

    terminal_write("\n4. Counting Semaphore Invariants:\n");
    int s_res = test_semaphore_invariants();
    if (s_res == 0) {
        terminal_write("   Result: PASS (Atomic counting, bounded limits, underflow/overflow protection)\n");
    } else {
        terminal_write("   Result: FAIL\n");
    }
}

