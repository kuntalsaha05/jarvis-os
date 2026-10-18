#ifndef FILE_ALLOC_H
#define FILE_ALLOC_H

#include <stdint.h>

#define FILE_ALLOC_DISK_BLOCKS 64
#define FILE_ALLOC_MAX_FILES   8

typedef struct {
    char name[16];
    uint32_t size_blocks;
    uint32_t start_block;
    int success;
} contiguous_file_t;

typedef struct {
    char name[16];
    uint32_t size_blocks;
    uint32_t start_block;
    uint32_t end_block;
    uint32_t blocks[16];
    int success;
} linked_file_t;

typedef struct {
    char name[16];
    uint32_t size_blocks;
    uint32_t index_block;
    uint32_t blocks[16];
    int success;
} indexed_file_t;

typedef struct {
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint8_t bitmap[FILE_ALLOC_DISK_BLOCKS];
} file_alloc_disk_t;

/* Simulators */
void file_alloc_disk_init(file_alloc_disk_t *disk);
int file_alloc_contiguous(file_alloc_disk_t *disk, const char *name, uint32_t blocks, contiguous_file_t *out);
int file_alloc_linked(file_alloc_disk_t *disk, const char *name, uint32_t blocks, linked_file_t *out);
int file_alloc_indexed(file_alloc_disk_t *disk, const char *name, uint32_t blocks, indexed_file_t *out);

/* Logical to physical resolution */
int file_alloc_contiguous_lookup(const contiguous_file_t *f, uint32_t logical_block);
int file_alloc_linked_lookup(const linked_file_t *f, uint32_t logical_block);
int file_alloc_indexed_lookup(const indexed_file_t *f, uint32_t logical_block);

/* Block deallocation and reuse */
int file_alloc_free_contiguous(file_alloc_disk_t *disk, contiguous_file_t *f);
int file_alloc_free_linked(file_alloc_disk_t *disk, linked_file_t *f);
int file_alloc_free_indexed(file_alloc_disk_t *disk, indexed_file_t *f);

/* Invariant verification */
int test_file_alloc_invariants(void);

/* Educational demonstration */
void file_alloc_run_demo(void);

#endif /* FILE_ALLOC_H */

