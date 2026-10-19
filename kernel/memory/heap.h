#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

#define KERNEL_HEAP_SIZE (64 * 1024)

typedef struct {
    size_t total_size;
    size_t used_bytes;
    size_t free_bytes;
    size_t total_blocks;
    size_t free_blocks;
    size_t largest_free_chunk;
    size_t alloc_count;
    size_t free_count;
} heap_stats_t;

void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
void heap_get_stats(heap_stats_t *stats);

#endif
