#include <stddef.h>
#include <stdint.h>
#include "heap.h"

#define HEAP_ALIGNMENT 16

typedef struct heap_block
{
    size_t size;
    int free;
    int padding;
    struct heap_block *next;
} __attribute__((aligned(16))) heap_block_t;

static uint8_t kernel_heap[KERNEL_HEAP_SIZE]
    __attribute__((aligned(16)));

static heap_block_t *first_block;
static int heap_initialized;

static size_t align_size(size_t size)
{
    return (size + HEAP_ALIGNMENT - 1) &
           ~(size_t)(HEAP_ALIGNMENT - 1);
}

static size_t heap_alloc_count = 0;
static size_t heap_free_count = 0;

void heap_init(void)
{
    first_block = (heap_block_t *)kernel_heap;
    first_block->size =
        KERNEL_HEAP_SIZE - sizeof(heap_block_t);
    first_block->free = 1;
    first_block->next = NULL;
    heap_initialized = 1;
    heap_alloc_count = 0;
    heap_free_count = 0;
}

void *kmalloc(size_t size)
{
    if (!heap_initialized || size == 0)
    {
        return NULL;
    }

    size = align_size(size);

    heap_block_t *block = first_block;

    while (block != NULL)
    {
        if (block->free && block->size >= size)
        {
            if (block->size >= size + sizeof(heap_block_t) +
                               HEAP_ALIGNMENT)
            {
                heap_block_t *split =
                    (heap_block_t *)((uint8_t *)(block + 1) + size);

                split->size =
                    block->size - size - sizeof(heap_block_t);
                split->free = 1;
                split->next = block->next;

                block->next = split;
                block->size = size;
            }

            block->free = 0;
            heap_alloc_count++;
            return (void *)(block + 1);
        }

        block = block->next;
    }

    return NULL;
}

void kfree(void *ptr)
{
    if (!heap_initialized || ptr == NULL)
    {
        return;
    }

    uint8_t *address = (uint8_t *)ptr;

    if (address < kernel_heap + sizeof(heap_block_t) ||
        address >= kernel_heap + KERNEL_HEAP_SIZE)
    {
        return;
    }

    heap_block_t *block = ((heap_block_t *)ptr) - 1;

    if ((uint8_t *)block < kernel_heap ||
        (uint8_t *)block >= kernel_heap + KERNEL_HEAP_SIZE ||
        block->free)
    {
        return;
    }

    block->free = 1;
    heap_free_count++;

    heap_block_t *current = first_block;

    while (current != NULL && current->next != NULL)
    {
        if (current->free && current->next->free)
        {
            current->size += sizeof(heap_block_t) +
                             current->next->size;
            current->next = current->next->next;
        }
        else
        {
            current = current->next;
        }
    }
}

void heap_get_stats(heap_stats_t *stats)
{
    if (!stats) return;
    stats->total_size = KERNEL_HEAP_SIZE;
    stats->used_bytes = 0;
    stats->free_bytes = 0;
    stats->total_blocks = 0;
    stats->free_blocks = 0;
    stats->largest_free_chunk = 0;
    stats->alloc_count = heap_alloc_count;
    stats->free_count = heap_free_count;

    if (!heap_initialized || !first_block) return;

    heap_block_t *current = first_block;
    while (current != NULL) {
        stats->total_blocks++;
        if (current->free) {
            stats->free_blocks++;
            stats->free_bytes += current->size;
            if (current->size > stats->largest_free_chunk) {
                stats->largest_free_chunk = current->size;
            }
        } else {
            stats->used_bytes += current->size;
        }
        current = current->next;
    }
}
