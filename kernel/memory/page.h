#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096
#define PAGE_POOL_COUNT 64

void page_init(void);
void *page_alloc(void);
int page_free(void *address);
uint32_t page_free_count(void);

#endif
