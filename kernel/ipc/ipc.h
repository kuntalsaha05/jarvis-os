#ifndef IPC_H
#define IPC_H

#include <stdint.h>

#define IPC_QUEUE_CAPACITY 16
#define IPC_MESSAGE_SIZE 64

typedef struct
{
    uint32_t sender_pid;
    uint32_t length;
    char data[IPC_MESSAGE_SIZE];
} ipc_message_t;

void ipc_init(void);
int ipc_send(const ipc_message_t *message);
int ipc_receive(ipc_message_t *message);

#endif
