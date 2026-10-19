#include "ipc.h"
#include <stddef.h>
#include <stdint.h>
#include "../process/process.h"
#include "../process/scheduler.h"
#include "../security/security.h"

extern volatile uint64_t timer_ticks;

typedef struct {
    ipc_message_t messages[IPC_QUEUE_CAPACITY];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t owner_pid;
    uint32_t waiting_pid;
    uint64_t total_sent;
    uint64_t total_received;
    uint64_t total_dropped_full;
    int is_allocated;
} queue_entry_t;

static queue_entry_t queues[IPC_MAX_QUEUES];

void ipc_init(void)
{
    for (uint32_t q = 0; q < IPC_MAX_QUEUES; q++) {
        queues[q].head = 0;
        queues[q].tail = 0;
        queues[q].count = 0;
        queues[q].owner_pid = 0;
        queues[q].waiting_pid = 0;
        queues[q].total_sent = 0;
        queues[q].total_received = 0;
        queues[q].total_dropped_full = 0;
        queues[q].is_allocated = 0;
    }

    /* Queue 0 is the permanent system-wide message queue */
    queues[0].is_allocated = 1;
    queues[0].owner_pid = 0; /* Kernel owned */
}

int ipc_queue_create(uint32_t owner_pid)
{
    for (uint32_t q = 1; q < IPC_MAX_QUEUES; q++) {
        if (!queues[q].is_allocated) {
            queues[q].head = 0;
            queues[q].tail = 0;
            queues[q].count = 0;
            queues[q].owner_pid = owner_pid;
            queues[q].waiting_pid = 0;
            queues[q].total_sent = 0;
            queues[q].total_received = 0;
            queues[q].total_dropped_full = 0;
            queues[q].is_allocated = 1;
            return (int)q;
        }
    }
    return IPC_ERR_FULL;
}

int ipc_queue_destroy(uint32_t queue_id, uint32_t requester_pid)
{
    if (queue_id == 0 || queue_id >= IPC_MAX_QUEUES || !queues[queue_id].is_allocated)
        return IPC_ERR_INVALID;

    /* Only queue owner or kernel (PID 0) may destroy the queue */
    if (requester_pid != queues[queue_id].owner_pid && requester_pid != 0)
        return IPC_ERR_NOT_OWNER;

    /* Wake up any task currently waiting on this queue before destruction */
    if (queues[queue_id].waiting_pid > 0) {
        process_unblock(queues[queue_id].waiting_pid);
        queues[queue_id].waiting_pid = 0;
    }

    queues[queue_id].is_allocated = 0;
    queues[queue_id].count = 0;
    queues[queue_id].head = 0;
    queues[queue_id].tail = 0;
    return IPC_SUCCESS;
}

int ipc_get_stats(uint32_t queue_id, ipc_queue_stats_t *stats)
{
    if (queue_id >= IPC_MAX_QUEUES || !stats)
        return IPC_ERR_INVALID;

    stats->queue_id = queue_id;
    stats->owner_pid = queues[queue_id].owner_pid;
    stats->capacity = IPC_QUEUE_CAPACITY;
    stats->count = queues[queue_id].count;
    stats->head = queues[queue_id].head;
    stats->tail = queues[queue_id].tail;
    stats->total_sent = queues[queue_id].total_sent;
    stats->total_received = queues[queue_id].total_received;
    stats->total_dropped_full = queues[queue_id].total_dropped_full;
    stats->is_allocated = queues[queue_id].is_allocated;
    return IPC_SUCCESS;
}

int ipc_send_to_queue(uint32_t queue_id, const ipc_message_t *message)
{
    if (security_check_permission(PERM_PROC_OP, SEC_ACT_IPC_SEND, "ipc_queue", (int32_t)queue_id) != SEC_OK)
        return IPC_ERR_PERMISSION;

    if (message == NULL || message->length > IPC_MESSAGE_SIZE)
        return IPC_ERR_INVALID;

    if (queue_id >= IPC_MAX_QUEUES || !queues[queue_id].is_allocated)
        return IPC_ERR_INVALID;

    queue_entry_t *q = &queues[queue_id];

    if (q->count == IPC_QUEUE_CAPACITY) {
        q->total_dropped_full++;
        return IPC_ERR_FULL;
    }

    q->messages[q->tail] = *message;
    q->tail = (q->tail + 1) % IPC_QUEUE_CAPACITY;
    q->count++;
    q->total_sent++;

    /* If a process is blocked waiting on this queue, awaken it */
    if (q->waiting_pid > 0) {
        process_unblock(q->waiting_pid);
        q->waiting_pid = 0;
    }

    return IPC_SUCCESS;
}

int ipc_receive_from_queue(uint32_t queue_id, ipc_message_t *message, uint32_t receiver_pid)
{
    if (message == NULL)
        return IPC_ERR_INVALID;

    if (queue_id >= IPC_MAX_QUEUES || !queues[queue_id].is_allocated)
        return IPC_ERR_INVALID;

    queue_entry_t *q = &queues[queue_id];

    if (q->count == 0)
        return IPC_ERR_EMPTY;

    /* If no recipient filtering (receiver_pid == 0), dequeue FIFO from head */
    if (receiver_pid == IPC_BROADCAST_PID) {
        *message = q->messages[q->head];
        q->head = (q->head + 1) % IPC_QUEUE_CAPACITY;
        q->count--;
        q->total_received++;
        return IPC_SUCCESS;
    }

    /* Filter by receiver_pid: search circular buffer for match */
    for (uint32_t offset = 0; offset < q->count; offset++) {
        uint32_t idx = (q->head + offset) % IPC_QUEUE_CAPACITY;
        if (q->messages[idx].receiver_pid == receiver_pid ||
            q->messages[idx].receiver_pid == IPC_BROADCAST_PID) {
            *message = q->messages[idx];

            /* Shift subsequent items left by 1 to preserve contiguous FIFO */
            for (uint32_t s = offset; s < q->count - 1; s++) {
                uint32_t from = (q->head + s + 1) % IPC_QUEUE_CAPACITY;
                uint32_t to = (q->head + s) % IPC_QUEUE_CAPACITY;
                q->messages[to] = q->messages[from];
            }

            if (q->tail == 0)
                q->tail = IPC_QUEUE_CAPACITY - 1;
            else
                q->tail--;

            q->count--;
            q->total_received++;
            return IPC_SUCCESS;
        }
    }

    return IPC_ERR_EMPTY;
}

int ipc_receive_blocking_from_queue(uint32_t queue_id, ipc_message_t *message, uint32_t receiver_pid, uint64_t timeout_ticks)
{
    if (message == NULL || queue_id >= IPC_MAX_QUEUES || !queues[queue_id].is_allocated)
        return IPC_ERR_INVALID;

    /* Immediate receive check */
    if (ipc_receive_from_queue(queue_id, message, receiver_pid) == IPC_SUCCESS)
        return IPC_SUCCESS;

    process_t *cur = scheduler_current_process();
    if (!cur || cur->pid == 0)
        return IPC_ERR_EMPTY;

    queue_entry_t *q = &queues[queue_id];
    q->waiting_pid = cur->pid;

    if (timeout_ticks > 0)
        cur->wake_tick = timer_ticks + timeout_ticks;
    else
        cur->wake_tick = 0;

    process_block(cur->pid);
    scheduler_yield();

    q->waiting_pid = 0;

    if (ipc_receive_from_queue(queue_id, message, receiver_pid) == IPC_SUCCESS)
        return IPC_SUCCESS;

    return IPC_ERR_TIMEOUT;
}

int ipc_send(const ipc_message_t *message)
{
    return ipc_send_to_queue(IPC_DEFAULT_QUEUE, message);
}

int ipc_receive(ipc_message_t *message)
{
    return ipc_receive_from_queue(IPC_DEFAULT_QUEUE, message, IPC_BROADCAST_PID);
}

int ipc_receive_filtered(ipc_message_t *message, uint32_t receiver_pid)
{
    return ipc_receive_from_queue(IPC_DEFAULT_QUEUE, message, receiver_pid);
}

int ipc_receive_blocking(ipc_message_t *message, uint32_t receiver_pid, uint64_t timeout_ticks)
{
    return ipc_receive_blocking_from_queue(IPC_DEFAULT_QUEUE, message, receiver_pid, timeout_ticks);
}

int test_ipc_invariants(void)
{
    /* 1. Round-trip send & receive on default queue */
    ipc_message_t m1 = {
        .sender_pid = 10,
        .receiver_pid = 20,
        .type = 1,
        .length = 4,
        .data = "TEST"
    };

    if (ipc_send(&m1) != IPC_SUCCESS)
        return -1;

    ipc_message_t r1;
    if (ipc_receive(&r1) != IPC_SUCCESS)
        return -2;

    if (r1.sender_pid != 10 || r1.receiver_pid != 20 || r1.type != 1 || r1.length != 4)
        return -3;

    if (r1.data[0] != 'T' || r1.data[1] != 'E' || r1.data[2] != 'S' || r1.data[3] != 'T')
        return -4;

    /* 2. Message boundary rejection: length > IPC_MESSAGE_SIZE (64) */
    ipc_message_t m_overflow = {
        .sender_pid = 1,
        .receiver_pid = 2,
        .type = 0,
        .length = IPC_MESSAGE_SIZE + 8,
        .data = {0}
    };
    if (ipc_send(&m_overflow) != IPC_ERR_INVALID)
        return -5;

    /* 3. NULL argument rejection */
    if (ipc_send(NULL) != IPC_ERR_INVALID)
        return -6;
    if (ipc_receive(NULL) != IPC_ERR_INVALID)
        return -7;

    /* 4. Queue capacity exhaustion (fill 16 messages) */
    for (uint32_t i = 0; i < IPC_QUEUE_CAPACITY; i++) {
        ipc_message_t m = {
            .sender_pid = 1,
            .receiver_pid = 2,
            .type = i,
            .length = 1,
            .data = "X"
        };
        if (ipc_send(&m) != IPC_SUCCESS)
            return -8;
    }

    /* 17th message must be rejected as queue is full */
    ipc_message_t m_full = { .sender_pid = 1, .receiver_pid = 2, .type = 99, .length = 1, .data = "F" };
    if (ipc_send(&m_full) != IPC_ERR_FULL)
        return -9;

    ipc_queue_stats_t stats;
    ipc_get_stats(0, &stats);
    if (stats.count != IPC_QUEUE_CAPACITY || stats.total_dropped_full < 1)
        return -10;

    /* Drain queue */
    for (uint32_t i = 0; i < IPC_QUEUE_CAPACITY; i++) {
        ipc_message_t drained;
        if (ipc_receive(&drained) != IPC_SUCCESS)
            return -11;
    }

    /* 5. Empty queue receive must return IPC_ERR_EMPTY */
    ipc_message_t empty_recv;
    if (ipc_receive(&empty_recv) != IPC_ERR_EMPTY)
        return -12;

    /* 6. Dynamic queue creation and ownership checks */
    int q1 = ipc_queue_create(42); /* Owned by PID 42 */
    if (q1 <= 0)
        return -13;

    /* Attempt destroy by unauthorized PID 99 -> must return IPC_ERR_NOT_OWNER */
    if (ipc_queue_destroy(q1, 99) != IPC_ERR_NOT_OWNER)
        return -14;

    /* Queue isolation: send to q1 must not appear in queue 0 */
    ipc_message_t q1_msg = { .sender_pid = 42, .receiver_pid = 42, .type = 5, .length = 2, .data = "OK" };
    if (ipc_send_to_queue(q1, &q1_msg) != IPC_SUCCESS)
        return -15;

    if (ipc_receive(&empty_recv) != IPC_ERR_EMPTY)
        return -16; /* Leaked to default queue! */

    if (ipc_receive_from_queue(q1, &empty_recv, 0) != IPC_SUCCESS)
        return -17;

    /* Authorized destroy by owner PID 42 */
    if (ipc_queue_destroy(q1, 42) != IPC_SUCCESS)
        return -18;

    return 0;
}
