#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stddef.h>

#define BLOCK_OK                 0
#define BLOCK_ERR_NOT_READY     -1
#define BLOCK_ERR_INVALID_PARAM -2
#define BLOCK_ERR_OUT_OF_BOUNDS -3
#define BLOCK_ERR_IO_FAILED     -4

typedef struct
{
    int (*read)(uint64_t lba, uint32_t count, void *buffer);
    int (*write)(uint64_t lba, uint32_t count, const void *buffer);
    uint32_t sector_size;
    uint64_t sector_count;
    const char *name;
} block_device_t;

int block_device_register(const block_device_t *device);
const block_device_t *block_device_get(void);
int block_device_is_ready(void);
uint32_t block_device_get_sector_size(void);
uint64_t block_device_get_sector_count(void);
uint64_t block_device_get_capacity_bytes(void);

int block_read(uint64_t lba, uint32_t count, void *buffer);
int block_write(uint64_t lba, uint32_t count, const void *buffer);

/* Self-test of block-device boundary checks */
int test_block_device_invariants(void);

#endif /* BLOCK_H */
