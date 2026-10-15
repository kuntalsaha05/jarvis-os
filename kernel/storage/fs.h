#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>

#define FS_MAGIC           0x4A415256 /* "JARV" */
#define FS_MAX_FILES       16
#define FS_MAX_FILENAME    28
#define FS_DIRECT_BLOCKS   8
#define FS_BLOCK_SIZE      512

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

/* Filesystem APIs */
int fs_init(void);
int fs_format(void);
int fs_create(const char *name);
int fs_open(const char *name);
int fs_write(int fd, const void *buf, uint32_t count);
int fs_read(int fd, void *buf, uint32_t count);
int fs_close(int fd);
int fs_delete(const char *name);
void fs_list(void);

/* Educational demonstration */
void fs_run_demo(void);

#endif /* FS_H */
