#include "page.h"

static uint8_t page_pool[PAGE_POOL_COUNT][PAGE_SIZE]
    __attribute__((aligned(PAGE_SIZE)));

static uint64_t page_bitmap;
static page_stats_t page_statistics;

void page_init(void)
{
    page_bitmap = 0;
    page_statistics.total_pages = PAGE_POOL_COUNT;
    page_statistics.free_pages = PAGE_POOL_COUNT;
    page_statistics.allocated_pages = 0;
    page_statistics.total_allocations = 0;
    page_statistics.total_frees = 0;
    page_statistics.failed_allocations = 0;
    page_statistics.pool_base = (uintptr_t)&page_pool[0][0];
    page_statistics.pool_end = page_statistics.pool_base + sizeof(page_pool);
}

void *page_alloc(void)
{
    for (uint32_t i = 0; i < PAGE_POOL_COUNT; i++) {
        uint64_t mask = 1ULL << i;

        if ((page_bitmap & mask) == 0) {
            page_bitmap |= mask;
            page_statistics.allocated_pages++;
            page_statistics.free_pages--;
            page_statistics.total_allocations++;
            return page_pool[i];
        }
    }

    page_statistics.failed_allocations++;
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
        return -1; /* Double free rejection */

    page_bitmap &= ~mask;
    page_statistics.allocated_pages--;
    page_statistics.free_pages++;
    page_statistics.total_frees++;
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

int page_is_allocated(void *address)
{
    if (address == NULL)
        return 0;

    uintptr_t base = (uintptr_t)&page_pool[0][0];
    uintptr_t ptr = (uintptr_t)address;
    uintptr_t end = base + sizeof(page_pool);

    if (ptr < base || ptr >= end)
        return 0;

    uintptr_t offset = ptr - base;
    if ((offset % PAGE_SIZE) != 0)
        return 0;

    uint32_t index = offset / PAGE_SIZE;
    return (page_bitmap & (1ULL << index)) != 0;
}

void page_get_stats(page_stats_t *stats)
{
    if (!stats) return;
    *stats = page_statistics;
    stats->free_pages = page_free_count();
    stats->allocated_pages = PAGE_POOL_COUNT - stats->free_pages;
}

int test_page_allocator_invariants(void)
{
    void *pages[PAGE_POOL_COUNT];

    /* 1. Allocate all 64 pages */
    for (uint32_t i = 0; i < PAGE_POOL_COUNT; i++) {
        pages[i] = page_alloc();
        if (pages[i] == NULL)
            return -1;
        if (!page_is_allocated(pages[i]))
            return -2;
    }

    /* 2. Verify exhaustion */
    void *over_alloc = page_alloc();
    if (over_alloc != NULL)
        return -3;
    if (page_free_count() != 0)
        return -4;

    /* 3. Verify bad free rejections */
    if (page_free(NULL) != -1)
        return -5;
    if (page_free((void *)(page_statistics.pool_base - PAGE_SIZE)) != -1)
        return -6;
    if (page_free((void *)(page_statistics.pool_end + PAGE_SIZE)) != -1)
        return -7;
    if (page_free((void *)(page_statistics.pool_base + 128)) != -1)
        return -8;

    /* 4. Free one page and verify double-free rejection */
    if (page_free(pages[10]) != 0)
        return -9;
    if (page_is_allocated(pages[10]))
        return -10;
    if (page_free(pages[10]) != -1)
        return -11;

    /* 5. Re-allocate the single freed page */
    void *reallocated = page_alloc();
    if (reallocated != pages[10])
        return -12;

    /* 6. Release all pages and verify clean pool */
    for (uint32_t i = 0; i < PAGE_POOL_COUNT; i++) {
        if (page_free(pages[i]) != 0)
            return -13;
    }

    if (page_free_count() != PAGE_POOL_COUNT)
        return -14;

    return 0;
}
