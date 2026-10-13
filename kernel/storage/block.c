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
        return -1;
    }

    registered_device = *device;
    device_registered = 1;
    return 0;
}

const block_device_t *block_device_get(void)
{
    return device_registered ? &registered_device : 0;
}

int block_read(uint64_t lba, uint32_t count, void *buffer)
{
    if (!device_registered || buffer == 0 || count == 0)
        return -1;

    if (lba >= registered_device.sector_count ||
        count > registered_device.sector_count - lba)
        return -1;

    return registered_device.read(lba, count, buffer);
}

int block_write(uint64_t lba, uint32_t count, const void *buffer)
{
    if (!device_registered || buffer == 0 || count == 0)
        return -1;

    if (lba >= registered_device.sector_count ||
        count > registered_device.sector_count - lba)
        return -1;

    return registered_device.write(lba, count, buffer);
}

