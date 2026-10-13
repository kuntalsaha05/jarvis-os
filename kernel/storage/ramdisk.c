#include "ramdisk.h"
#include "block.h"
#include <stddef.h>
#include <stdint.h>

#define RAMDISK_SECTOR_SIZE 512
#define RAMDISK_SECTOR_COUNT 128

static uint8_t ramdisk_data[RAMDISK_SECTOR_SIZE * RAMDISK_SECTOR_COUNT];

static int ramdisk_read(uint64_t lba, uint32_t count, void *buffer)
{
    if (buffer == NULL)
        return -1;

    for (uint32_t i = 0; i < count; i++)
    {
        for (uint32_t j = 0; j < RAMDISK_SECTOR_SIZE; j++)
        {
            ((uint8_t *)buffer)[i * RAMDISK_SECTOR_SIZE + j] =
                ramdisk_data[(lba + i) * RAMDISK_SECTOR_SIZE + j];
        }
    }

    return 0;
}

static int ramdisk_write(uint64_t lba, uint32_t count, const void *buffer)
{
    if (buffer == NULL)
        return -1;

    for (uint32_t i = 0; i < count; i++)
    {
        for (uint32_t j = 0; j < RAMDISK_SECTOR_SIZE; j++)
        {
            ramdisk_data[(lba + i) * RAMDISK_SECTOR_SIZE + j] =
                ((const uint8_t *)buffer)[i * RAMDISK_SECTOR_SIZE + j];
        }
    }

    return 0;
}

int ramdisk_init(void)
{
    block_device_t device = {
        .read = ramdisk_read,
        .write = ramdisk_write,
        .sector_size = RAMDISK_SECTOR_SIZE,
        .sector_count = RAMDISK_SECTOR_COUNT
    };

    return block_device_register(&device);
}

