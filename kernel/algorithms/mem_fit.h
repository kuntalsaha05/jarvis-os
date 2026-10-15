#ifndef MEM_FIT_H
#define MEM_FIT_H

#include <stdint.h>

#define MEM_FIT_MAX_BLOCKS 16
#define MEM_FIT_MAX_REQS   16

typedef struct {
    uint32_t block_id;
    uint32_t size;
    uint32_t original_size;
    int allocated;
    int process_id;
} mem_block_t;

typedef struct {
    uint32_t req_id;
    uint32_t size;
    int allocated_block_id; /* -1 if allocation failed */
} mem_req_result_t;

typedef struct {
    uint32_t total_memory;
    uint32_t total_allocated;
    uint32_t external_frag;
    uint32_t successful_allocs;
    uint32_t failed_allocs;
    mem_block_t blocks[MEM_FIT_MAX_BLOCKS];
    uint32_t block_count;
    mem_req_result_t results[MEM_FIT_MAX_REQS];
    uint32_t req_count;
} mem_fit_result_t;

/* Memory fit allocation algorithms */
void mem_fit_first(const uint32_t *blocks, uint32_t block_count, const uint32_t *requests, uint32_t req_count, mem_fit_result_t *res);
void mem_fit_best(const uint32_t *blocks, uint32_t block_count, const uint32_t *requests, uint32_t req_count, mem_fit_result_t *res);
void mem_fit_worst(const uint32_t *blocks, uint32_t block_count, const uint32_t *requests, uint32_t req_count, mem_fit_result_t *res);

/* Educational demonstration */
void mem_fit_run_demo(void);

#endif /* MEM_FIT_H */
