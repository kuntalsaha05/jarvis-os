#include "mem_fit.h"
#include "../drivers/terminal.h"

static void init_blocks(const uint32_t *blocks, uint32_t block_count, mem_fit_result_t *res)
{
    if (block_count > MEM_FIT_MAX_BLOCKS) block_count = MEM_FIT_MAX_BLOCKS;
    res->block_count = block_count;
    res->total_memory = 0;
    res->total_allocated = 0;
    res->successful_allocs = 0;
    res->failed_allocs = 0;

    for (uint32_t i = 0; i < block_count; i++) {
        res->blocks[i].block_id = i;
        res->blocks[i].size = blocks[i];
        res->blocks[i].original_size = blocks[i];
        res->blocks[i].allocated = 0;
        res->blocks[i].process_id = -1;
        res->total_memory += blocks[i];
    }
}

void mem_fit_first(const uint32_t *blocks, uint32_t block_count, const uint32_t *requests, uint32_t req_count, mem_fit_result_t *res)
{
    init_blocks(blocks, block_count, res);
    if (req_count > MEM_FIT_MAX_REQS) req_count = MEM_FIT_MAX_REQS;
    res->req_count = req_count;

    for (uint32_t i = 0; i < req_count; i++) {
        uint32_t req_size = requests[i];
        res->results[i].req_id = i;
        res->results[i].size = req_size;
        res->results[i].allocated_block_id = -1;

        if (req_size == 0) {
            res->failed_allocs++;
            continue;
        }

        /* First Fit: find first unallocated block with sufficient size */
        for (uint32_t j = 0; j < res->block_count; j++) {
            if (!res->blocks[j].allocated && res->blocks[j].size >= req_size) {
                res->blocks[j].allocated = 1;
                res->blocks[j].process_id = (int)i;
                res->results[i].allocated_block_id = (int)j;
                res->total_allocated += req_size;
                res->successful_allocs++;
                break;
            }
        }

        if (res->results[i].allocated_block_id == -1) {
            res->failed_allocs++;
        }
    }

    res->external_frag = 0;
    for (uint32_t j = 0; j < res->block_count; j++) {
        if (!res->blocks[j].allocated) {
            res->external_frag += res->blocks[j].size;
        }
    }
}

void mem_fit_best(const uint32_t *blocks, uint32_t block_count, const uint32_t *requests, uint32_t req_count, mem_fit_result_t *res)
{
    init_blocks(blocks, block_count, res);
    if (req_count > MEM_FIT_MAX_REQS) req_count = MEM_FIT_MAX_REQS;
    res->req_count = req_count;

    for (uint32_t i = 0; i < req_count; i++) {
        uint32_t req_size = requests[i];
        res->results[i].req_id = i;
        res->results[i].size = req_size;
        res->results[i].allocated_block_id = -1;

        if (req_size == 0) {
            res->failed_allocs++;
            continue;
        }

        /* Best Fit: find unallocated block that is smallest but >= req_size */
        int best_idx = -1;
        uint32_t min_diff = 0xFFFFFFFF;

        for (uint32_t j = 0; j < res->block_count; j++) {
            if (!res->blocks[j].allocated && res->blocks[j].size >= req_size) {
                uint32_t diff = res->blocks[j].size - req_size;
                if (diff < min_diff) {
                    min_diff = diff;
                    best_idx = (int)j;
                }
            }
        }

        if (best_idx != -1) {
            res->blocks[best_idx].allocated = 1;
            res->blocks[best_idx].process_id = (int)i;
            res->results[i].allocated_block_id = best_idx;
            res->total_allocated += req_size;
            res->successful_allocs++;
        } else {
            res->failed_allocs++;
        }
    }

    res->external_frag = 0;
    for (uint32_t j = 0; j < res->block_count; j++) {
        if (!res->blocks[j].allocated) {
            res->external_frag += res->blocks[j].size;
        }
    }
}

void mem_fit_worst(const uint32_t *blocks, uint32_t block_count, const uint32_t *requests, uint32_t req_count, mem_fit_result_t *res)
{
    init_blocks(blocks, block_count, res);
    if (req_count > MEM_FIT_MAX_REQS) req_count = MEM_FIT_MAX_REQS;
    res->req_count = req_count;

    for (uint32_t i = 0; i < req_count; i++) {
        uint32_t req_size = requests[i];
        res->results[i].req_id = i;
        res->results[i].size = req_size;
        res->results[i].allocated_block_id = -1;

        if (req_size == 0) {
            res->failed_allocs++;
            continue;
        }

        /* Worst Fit: find unallocated block that is largest */
        int worst_idx = -1;
        uint32_t max_size = 0;

        for (uint32_t j = 0; j < res->block_count; j++) {
            if (!res->blocks[j].allocated && res->blocks[j].size >= req_size) {
                if (res->blocks[j].size > max_size) {
                    max_size = res->blocks[j].size;
                    worst_idx = (int)j;
                }
            }
        }

        if (worst_idx != -1) {
            res->blocks[worst_idx].allocated = 1;
            res->blocks[worst_idx].process_id = (int)i;
            res->results[i].allocated_block_id = worst_idx;
            res->total_allocated += req_size;
            res->successful_allocs++;
        } else {
            res->failed_allocs++;
        }
    }

    res->external_frag = 0;
    for (uint32_t j = 0; j < res->block_count; j++) {
        if (!res->blocks[j].allocated) {
            res->external_frag += res->blocks[j].size;
        }
    }
}

static void print_fit_table(const char *name, const mem_fit_result_t *res)
{
    terminal_write("\nStrategy: ");
    terminal_write(name);
    terminal_write("\nRequest  Size (KB)  Assigned Block  Block Size (KB)\n");

    for (uint32_t i = 0; i < res->req_count; i++) {
        terminal_write("P");
        terminal_write_number(i);
        terminal_write("       ");
        terminal_write_number(res->results[i].size);
        terminal_write("        ");

        if (res->results[i].allocated_block_id != -1) {
            terminal_write("Block ");
            terminal_write_number((uint32_t)res->results[i].allocated_block_id);
            terminal_write("         ");
            uint32_t bsize = res->blocks[res->results[i].allocated_block_id].original_size;
            terminal_write_number(bsize);
            terminal_write(" (Hole: ");
            terminal_write_number(bsize - res->results[i].size);
            terminal_write(")\n");
        } else {
            terminal_write("Not Allocated   -\n");
        }
    }

    terminal_write("Summary: ");
    terminal_write_number(res->successful_allocs);
    terminal_write(" allocated, ");
    terminal_write_number(res->failed_allocs);
    terminal_write(" failed. Unused external memory: ");
    terminal_write_number(res->external_frag);
    terminal_write(" KB\n");
}

void mem_fit_run_demo(void)
{
    /* Benchmark input: 5 partitions, 4 process requests */
    static const uint32_t partitions[5] = { 100, 500, 200, 300, 600 };
    static const uint32_t requests[4]   = { 212, 417, 112, 426 };

    terminal_write("\n========================================\n");
    terminal_write("   MEMORY ALLOCATION STRATEGIES DEMO    \n");
    terminal_write("========================================\n");
    terminal_write("Initial Memory Partitions: [100, 500, 200, 300, 600] KB\n");
    terminal_write("Process Requests:          [P0: 212, P1: 417, P2: 112, P3: 426] KB\n");

    mem_fit_result_t res;

    mem_fit_first(partitions, 5, requests, 4, &res);
    print_fit_table("First Fit", &res);

    mem_fit_best(partitions, 5, requests, 4, &res);
    print_fit_table("Best Fit", &res);

    mem_fit_worst(partitions, 5, requests, 4, &res);
    print_fit_table("Worst Fit", &res);
}
