#include "ipc.h"
#include <stddef.h>

static ipc_message_t queue[IPC_QUEUE_CAPACITY];
static uint32_t head;
static uint32_t tail;
static uint32_t count;

void ipc_init(void)
{
    head = 0;
    tail = 0;
    count = 0;
}

int ipc_send(const ipc_message_t *message)
{
    if (message == NULL ||
        message->length > IPC_MESSAGE_SIZE)
    {
        return -1;
    }

    if (count == IPC_QUEUE_CAPACITY)
    {
        return -2;
    }

    queue[tail] = *message;
    tail = (tail + 1) % IPC_QUEUE_CAPACITY;
    count++;

    return 0;
}

int ipc_receive(ipc_message_t *message)
{
    if (message == NULL)
    {
        return -1;
    }

    if (count == 0)
    {
        return -2;
    }

    *message = queue[head];
    head = (head + 1) % IPC_QUEUE_CAPACITY;
    count--;

    return 0;
}
