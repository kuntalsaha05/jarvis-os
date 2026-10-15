#include "sync_demo.h"
#include "semaphore.h"
#include "../drivers/terminal.h"

void mutex_init(mutex_t *mutex)
{
    if (mutex) {
        mutex->locked = 0;
        mutex->owner_pid = 0;
    }
}

int mutex_try_lock(mutex_t *mutex)
{
    if (!mutex) return -1;
    if (__sync_lock_test_and_set(&mutex->locked, 1) == 0) {
        mutex->owner_pid = 1;
        return 0;
    }
    return -1;
}

int mutex_lock(mutex_t *mutex)
{
    if (!mutex) return -1;
    while (__sync_lock_test_and_set(&mutex->locked, 1)) {
        __asm__ volatile ("pause");
    }
    mutex->owner_pid = 1;
    return 0;
}

int mutex_unlock(mutex_t *mutex)
{
    if (!mutex || !mutex->locked) return -1;
    mutex->owner_pid = 0;
    __sync_lock_release(&mutex->locked);
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

    semaphore_init(&sem_empty, BUFFER_SIZE);
    semaphore_init(&sem_full, 0);
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

void sync_run_demo(void)
{
    terminal_write("\n========================================\n");
    terminal_write("     SYNCHRONIZATION & IPC BENCHMARK    \n");
    terminal_write("========================================\n");

    sync_demo_producer_consumer();
    sync_demo_reader_writer();
}
