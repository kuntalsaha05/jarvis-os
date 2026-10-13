#include "page.h"

static uint8_t page_pool[PAGE_POOL_COUNT][PAGE_SIZE]
    __attribute__((aligned(PAGE_SIZE)));

static uint64_t page_bitmap;

void page_init(void)
{
    page_bitmap = 0;
}

void *page_alloc(void)
{
    for (uint32_t i = 0; i < PAGE_POOL_COUNT; i++) {
        uint64_t mask = 1ULL << i;

        if ((page_bitmap & mask) == 0) {
            page_bitmap |= mask;
            return page_pool[i];
        }
    }

    return NULL;
}

int page_free(void *address)
{
    if (address == NULL)
        return -1;

    uintptr_t base = (uintptr_t)&page_pool[0][0];
    uintptr_t ptr = (uintptr_t)address;
    uintptr_t end = base + sizeof(page_pool);

    if (ptr < base || ptr >= end)
        return -1;

    uintptr_t offset = ptr - base;

    if ((offset % PAGE_SIZE) != 0)
        return -1;

    uint32_t index = offset / PAGE_SIZE;
    uint64_t mask = 1ULL << index;

    if ((page_bitmap & mask) == 0)
        return -1;

    page_bitmap &= ~mask;
    return 0;
}

uint32_t page_free_count(void)
{
    uint32_t count = 0;

    for (uint32_t i = 0; i < PAGE_POOL_COUNT; i++) {
        if ((page_bitmap & (1ULL << i)) == 0)
            count++;
    }

    return count;
}
