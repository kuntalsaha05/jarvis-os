#include "block.h"

static block_device_t registered_device;
static int device_registered = 0;

int block_device_register(const block_device_t *device)
{
    if (device == 0 ||
        device->read == 0 ||
        device->write == 0 ||
        device->sector_size == 0 ||
        device->sector_count == 0)
    {
        return BLOCK_ERR_INVALID_PARAM;
    }

    registered_device = *device;
    device_registered = 1;
    return BLOCK_OK;
}

const block_device_t *block_device_get(void)
{
    return device_registered ? &registered_device : 0;
}

int block_device_is_ready(void)
{
    return device_registered;
}

uint32_t block_device_get_sector_size(void)
{
    if (!device_registered) return 0;
    return registered_device.sector_size;
}

uint64_t block_device_get_sector_count(void)
{
    if (!device_registered) return 0;
    return registered_device.sector_count;
}

uint64_t block_device_get_capacity_bytes(void)
{
    if (!device_registered) return 0;
    return registered_device.sector_count * (uint64_t)registered_device.sector_size;
}

int block_read(uint64_t lba, uint32_t count, void *buffer)
{
    if (!device_registered)
        return BLOCK_ERR_NOT_READY;
    if (buffer == 0)
        return BLOCK_ERR_INVALID_PARAM;
    if (count == 0)
        return BLOCK_OK;

    if (lba >= registered_device.sector_count)
        return BLOCK_ERR_OUT_OF_BOUNDS;

    if (lba + count < lba || count > registered_device.sector_count - lba)
        return BLOCK_ERR_OUT_OF_BOUNDS;

    int res = registered_device.read(lba, count, buffer);
    if (res < 0) return BLOCK_ERR_IO_FAILED;
    return BLOCK_OK;
}

int block_write(uint64_t lba, uint32_t count, const void *buffer)
{
    if (!device_registered)
        return BLOCK_ERR_NOT_READY;
    if (buffer == 0)
        return BLOCK_ERR_INVALID_PARAM;
    if (count == 0)
        return BLOCK_OK;

    if (lba >= registered_device.sector_count)
        return BLOCK_ERR_OUT_OF_BOUNDS;

    if (lba + count < lba || count > registered_device.sector_count - lba)
        return BLOCK_ERR_OUT_OF_BOUNDS;

    int res = registered_device.write(lba, count, buffer);
    if (res < 0) return BLOCK_ERR_IO_FAILED;
    return BLOCK_OK;
}

int test_block_device_invariants(void)
{
    if (!device_registered) return -1;

    uint8_t buf[512];
    for (int i = 0; i < 512; i++) buf[i] = 0;

    /* 1. NULL buffer check */
    if (block_read(0, 1, 0) != BLOCK_ERR_INVALID_PARAM) return -2;
    if (block_write(0, 1, 0) != BLOCK_ERR_INVALID_PARAM) return -3;

    /* 2. Count 0 check */
    if (block_read(0, 0, buf) != BLOCK_OK) return -4;
    if (block_write(0, 0, buf) != BLOCK_OK) return -5;

    /* 3. Out-of-bounds LBA */
    uint64_t max_lba = registered_device.sector_count;
    if (block_read(max_lba, 1, buf) != BLOCK_ERR_OUT_OF_BOUNDS) return -6;
    if (block_write(max_lba, 1, buf) != BLOCK_ERR_OUT_OF_BOUNDS) return -7;

    /* 4. Count overflow */
    if (block_read(max_lba - 1, 2, buf) != BLOCK_ERR_OUT_OF_BOUNDS) return -8;
    if (block_write(max_lba - 1, 2, buf) != BLOCK_ERR_OUT_OF_BOUNDS) return -9;

    /* 5. Valid read/write roundtrip at sector 0 */
    uint8_t pattern[512];
    for (int i = 0; i < 512; i++) pattern[i] = (uint8_t)(i ^ 0xA5);

    uint8_t orig[512];
    if (block_read(0, 1, orig) != BLOCK_OK) return -10;

    if (block_write(0, 1, pattern) != BLOCK_OK) return -11;
    if (block_read(0, 1, buf) != BLOCK_OK) return -12;

    for (int i = 0; i < 512; i++) {
        if (buf[i] != pattern[i]) return -13;
    }

    /* Restore sector 0 */
    block_write(0, 1, orig);

    return 0;
}
