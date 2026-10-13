#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    int (*read)(uint64_t lba, uint32_t count, void *buffer);
    int (*write)(uint64_t lba, uint32_t count, const void *buffer);
    uint32_t sector_size;
    uint64_t sector_count;
} block_device_t;

int block_device_register(const block_device_t *device);
const block_device_t *block_device_get(void);
int block_read(uint64_t lba, uint32_t count, void *buffer);
int block_write(uint64_t lba, uint32_t count, const void *buffer);

#endif

