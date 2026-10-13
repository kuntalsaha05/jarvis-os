#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

#define KERNEL_HEAP_SIZE (64 * 1024)

void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);

#endif
