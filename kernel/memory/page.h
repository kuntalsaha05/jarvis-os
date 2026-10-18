#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096
#define PAGE_POOL_COUNT 64

typedef struct {
    uint32_t total_pages;
    uint32_t free_pages;
    uint32_t allocated_pages;
    uint64_t total_allocations;
    uint64_t total_frees;
    uint64_t failed_allocations;
    uintptr_t pool_base;
    uintptr_t pool_end;
} page_stats_t;

void page_init(void);
void *page_alloc(void);
int page_free(void *address);
uint32_t page_free_count(void);
void page_get_stats(page_stats_t *stats);
int page_is_allocated(void *address);
int test_page_allocator_invariants(void);

#endif
