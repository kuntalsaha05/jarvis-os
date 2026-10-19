#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>

#define FS_MAGIC           0x4A415256 /* "JARV" */
#define FS_MAX_FILES       16
#define FS_MAX_FILENAME    28
#define FS_DIRECT_BLOCKS   8
#define FS_BLOCK_SIZE      512
#define FS_MAX_FILE_SIZE   (FS_DIRECT_BLOCKS * FS_BLOCK_SIZE) /* 4096 bytes */
#define FS_TOTAL_BLOCKS    128
#define FS_FIRST_DATA_BLOCK 4
#define FS_TOTAL_DATA_BLOCKS 120

/* Filesystem Return Codes */
#define FS_OK                    0
#define FS_ERR_NOT_MOUNTED      -1
#define FS_ERR_NOT_FOUND        -2
#define FS_ERR_ALREADY_EXISTS   -3
#define FS_ERR_INODE_FULL       -4
#define FS_ERR_DISK_FULL        -5
#define FS_ERR_INVALID_PARAM    -6
#define FS_ERR_FILE_TOO_LARGE   -7
#define FS_ERR_TOO_MANY_OPEN    -8
#define FS_ERR_BAD_FD           -9
#define FS_ERR_IO               -10
#define FS_ERR_PERMISSION_DENIED -11

typedef struct {
    char name[FS_MAX_FILENAME];
    uint32_t size;
    uint32_t blocks[FS_DIRECT_BLOCKS];
    uint8_t used;
} fs_inode_t;

typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t inode_count;
} fs_superblock_t;

typedef struct {
    char name[FS_MAX_FILENAME];
    uint32_t size;
    uint32_t blocks_allocated;
    uint8_t in_use;
} fs_stat_t;

typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t used_blocks;
    uint32_t total_inodes;
    uint32_t used_inodes;
    uint32_t free_inodes;
    uint32_t block_size;
    uint32_t max_file_size;
} fs_info_t;

/* Filesystem Lifecycle & Management */
int fs_init(void);
int fs_format(void);
int fs_get_info(fs_info_t *info);
int fs_get_inode(uint32_t idx, fs_inode_t *out_node);
void fs_get_stats(uint32_t *ops, uint32_t *errors, uint32_t *rollbacks);

/* File Operations */
int fs_create(const char *name);
int fs_open(const char *name);
int fs_write(int fd, const void *buf, uint32_t count);
int fs_read(int fd, void *buf, uint32_t count);
int fs_close(int fd);
int fs_delete(const char *name);
int fs_truncate(int fd, uint32_t new_size);
int fs_truncate_by_name(const char *name, uint32_t new_size);
int fs_stat(const char *name, fs_stat_t *st);
void fs_list(void);

/* Educational demonstration */
void fs_run_demo(void);

/* Invariant Verification */
int test_fs_invariants(void);

#endif /* FS_H */
