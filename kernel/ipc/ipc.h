#ifndef IPC_H
#define IPC_H

#include <stdint.h>

#define IPC_QUEUE_CAPACITY 16
#define IPC_MESSAGE_SIZE 64
#define IPC_MAX_QUEUES 4
#define IPC_BROADCAST_PID 0
#define IPC_DEFAULT_QUEUE 0

/* IPC Error Codes */
#define IPC_SUCCESS          0
#define IPC_ERR_INVALID     -1   /* NULL pointer, length > 64, or invalid queue */
#define IPC_ERR_FULL        -2   /* Queue at maximum capacity */
#define IPC_ERR_EMPTY       -2   /* Nonblocking receive from empty queue */
#define IPC_ERR_TIMEOUT     -3   /* Blocking receive timed out */
#define IPC_ERR_NOT_OWNER   -4   /* Operation rejected: requester is not queue owner */
#define IPC_ERR_PERMISSION  -5   /* Operation rejected: missing required permissions */

typedef struct
{
    uint32_t sender_pid;
    uint32_t receiver_pid; /* target process PID or IPC_BROADCAST_PID (0) */
    uint32_t type;         /* application / subsystem message type */
    uint32_t length;       /* payload length in bytes */
    char data[IPC_MESSAGE_SIZE];
} ipc_message_t;

typedef struct
{
    uint32_t queue_id;
    uint32_t owner_pid;
    uint32_t capacity;
    uint32_t count;
    uint32_t head;
    uint32_t tail;
    uint64_t total_sent;
    uint64_t total_received;
    uint64_t total_dropped_full;
    int is_allocated;
} ipc_queue_stats_t;

/* System lifecycle */
void ipc_init(void);

/* Queue management */
int ipc_queue_create(uint32_t owner_pid);
int ipc_queue_destroy(uint32_t queue_id, uint32_t requester_pid);
int ipc_get_stats(uint32_t queue_id, ipc_queue_stats_t *stats);

/* Multi-queue send/receive */
int ipc_send_to_queue(uint32_t queue_id, const ipc_message_t *message);
int ipc_receive_from_queue(uint32_t queue_id, ipc_message_t *message, uint32_t receiver_pid);
int ipc_receive_blocking_from_queue(uint32_t queue_id, ipc_message_t *message, uint32_t receiver_pid, uint64_t timeout_ticks);

/* Default system queue operations (queue 0) */
int ipc_send(const ipc_message_t *message);
int ipc_receive(ipc_message_t *message);
int ipc_receive_filtered(ipc_message_t *message, uint32_t receiver_pid);
int ipc_receive_blocking(ipc_message_t *message, uint32_t receiver_pid, uint64_t timeout_ticks);

/* Automated invariants verification */
int test_ipc_invariants(void);

#endif
