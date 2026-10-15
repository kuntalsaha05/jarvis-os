#ifndef PAGE_REPLACE_H
#define PAGE_REPLACE_H

#include <stdint.h>

#define PAGE_REPLACE_MAX_FRAMES 16
#define PAGE_REPLACE_MAX_REFS   32

typedef struct {
    uint32_t page_faults;
    uint32_t hits;
    uint32_t frame_count;
    uint32_t ref_count;
} page_replace_result_t;

/* Simulators */
uint32_t page_replace_fifo(const uint32_t *refs, uint32_t ref_count, uint32_t frame_count);
uint32_t page_replace_lru(const uint32_t *refs, uint32_t ref_count, uint32_t frame_count);
uint32_t page_replace_optimal(const uint32_t *refs, uint32_t ref_count, uint32_t frame_count);

/* Formatted demonstrations */
void page_replace_run_demo(void);
void page_replace_belady_demo(void);

#endif /* PAGE_REPLACE_H */

