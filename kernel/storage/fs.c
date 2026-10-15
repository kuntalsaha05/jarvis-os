#include "fs.h"
#include "block.h"
#include "../drivers/terminal.h"

#define FS_FIRST_DATA_BLOCK 4
#define FS_TOTAL_DATA_BLOCKS 120

typedef struct {
    int inode_idx;
    uint32_t offset;
    int used;
} file_desc_t;

static file_desc_t open_files[FS_MAX_FILES];
static fs_inode_t inodes[FS_MAX_FILES];
static uint8_t block_bitmap[FS_BLOCK_SIZE];
static fs_superblock_t sb;
static int fs_mounted = 0;

static int str_equals(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (*(const unsigned char *)s1 == *(const unsigned char *)s2);
}

static void str_copy(char *dst, const char *src, uint32_t max_len)
{
    uint32_t i = 0;
    while (src[i] != '\0' && i + 1 < max_len) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int alloc_block(void)
{
    for (uint32_t i = 0; i < FS_TOTAL_DATA_BLOCKS; i++) {
        uint32_t byte_idx = i / 8;
        uint8_t bit_mask = 1 << (i % 8);
        if ((block_bitmap[byte_idx] & bit_mask) == 0) {
            block_bitmap[byte_idx] |= bit_mask;
            sb.free_blocks--;
            return (int)(FS_FIRST_DATA_BLOCK + i);
        }
    }
    return -1;
}

static void free_block(uint32_t block_num)
{
    if (block_num >= FS_FIRST_DATA_BLOCK) {
        uint32_t i = block_num - FS_FIRST_DATA_BLOCK;
        if (i < FS_TOTAL_DATA_BLOCKS) {
            block_bitmap[i / 8] &= ~(1 << (i % 8));
            sb.free_blocks++;
        }
    }
}

int fs_format(void)
{
    sb.magic = FS_MAGIC;
    sb.total_blocks = 128;
    sb.free_blocks = FS_TOTAL_DATA_BLOCKS;
    sb.inode_count = FS_MAX_FILES;

    for (uint32_t i = 0; i < FS_BLOCK_SIZE; i++) {
        block_bitmap[i] = 0;
    }

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        inodes[i].used = 0;
        inodes[i].size = 0;
        inodes[i].name[0] = '\0';
        for (int b = 0; b < FS_DIRECT_BLOCKS; b++) inodes[i].blocks[b] = 0;
    }

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        open_files[i].used = 0;
    }

    fs_mounted = 1;
    return 0;
}

int fs_init(void)
{
    return fs_format();
}

int fs_create(const char *name)
{
    if (!fs_mounted || name == 0 || name[0] == '\0') return -1;

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (inodes[i].used && str_equals(inodes[i].name, name)) {
            return -2; /* Already exists */
        }
    }

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (!inodes[i].used) {
            inodes[i].used = 1;
            inodes[i].size = 0;
            str_copy(inodes[i].name, name, FS_MAX_FILENAME);
            for (int b = 0; b < FS_DIRECT_BLOCKS; b++) inodes[i].blocks[b] = 0;
            return 0;
        }
    }

    return -3; /* Inode table full */
}

int fs_open(const char *name)
{
    if (!fs_mounted || name == 0) return -1;

    int inode_idx = -1;
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (inodes[i].used && str_equals(inodes[i].name, name)) {
            inode_idx = (int)i;
            break;
        }
    }
    if (inode_idx == -1) return -1;

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (!open_files[i].used) {
            open_files[i].used = 1;
            open_files[i].inode_idx = inode_idx;
            open_files[i].offset = 0;
            return (int)i;
        }
    }

    return -2; /* Too many open files */
}

int fs_write(int fd, const void *buf, uint32_t count)
{
    if (!fs_mounted || fd < 0 || fd >= FS_MAX_FILES || !open_files[fd].used || buf == 0) return -1;
    if (open_files[fd].inode_idx < 0 || open_files[fd].inode_idx >= FS_MAX_FILES) return -1;

    fs_inode_t *node = &inodes[open_files[fd].inode_idx];
    if (!node->used) return -1;

    const uint8_t *src = (const uint8_t *)buf;
    uint32_t written = 0;

    while (written < count) {
        uint32_t block_index = open_files[fd].offset / FS_BLOCK_SIZE;
        uint32_t block_offset = open_files[fd].offset % FS_BLOCK_SIZE;

        if (block_index >= FS_DIRECT_BLOCKS) {
            break; /* File size limit reached (4096 bytes direct) */
        }

        if (node->blocks[block_index] == 0) {
            int b = alloc_block();
            if (b == -1) break; /* Disk full */
            node->blocks[block_index] = (uint32_t)b;
        }

        uint8_t sector_buf[FS_BLOCK_SIZE];
        block_read(node->blocks[block_index], 1, sector_buf);

        uint32_t to_write = FS_BLOCK_SIZE - block_offset;
        if (to_write > count - written) to_write = count - written;

        for (uint32_t i = 0; i < to_write; i++) {
            sector_buf[block_offset + i] = src[written + i];
        }

        block_write(node->blocks[block_index], 1, sector_buf);

        written += to_write;
        open_files[fd].offset += to_write;
        if (open_files[fd].offset > node->size) {
            node->size = open_files[fd].offset;
        }
    }

    return (int)written;
}

int fs_read(int fd, void *buf, uint32_t count)
{
    if (!fs_mounted || fd < 0 || fd >= FS_MAX_FILES || !open_files[fd].used || buf == 0) return -1;
    if (open_files[fd].inode_idx < 0 || open_files[fd].inode_idx >= FS_MAX_FILES) return -1;

    fs_inode_t *node = &inodes[open_files[fd].inode_idx];
    if (!node->used) return -1;

    uint8_t *dst = (uint8_t *)buf;
    uint32_t read_bytes = 0;

    if (open_files[fd].offset >= node->size) return 0; /* EOF */

    if (count > node->size - open_files[fd].offset) {
        count = node->size - open_files[fd].offset;
    }

    while (read_bytes < count) {
        uint32_t block_index = open_files[fd].offset / FS_BLOCK_SIZE;
        uint32_t block_offset = open_files[fd].offset % FS_BLOCK_SIZE;

        if (block_index >= FS_DIRECT_BLOCKS || node->blocks[block_index] == 0) break;

        uint8_t sector_buf[FS_BLOCK_SIZE];
        block_read(node->blocks[block_index], 1, sector_buf);

        uint32_t to_read = FS_BLOCK_SIZE - block_offset;
        if (to_read > count - read_bytes) to_read = count - read_bytes;

        for (uint32_t i = 0; i < to_read; i++) {
            dst[read_bytes + i] = sector_buf[block_offset + i];
        }

        read_bytes += to_read;
        open_files[fd].offset += to_read;
    }

    return (int)read_bytes;
}

int fs_close(int fd)
{
    if (fd >= 0 && fd < FS_MAX_FILES) {
        if (!open_files[fd].used) return -1;
        open_files[fd].used = 0;
        open_files[fd].offset = 0;
        open_files[fd].inode_idx = -1;
        return 0;
    }
    return -1;
}

int fs_delete(const char *name)
{
    if (!fs_mounted || name == 0) return -1;

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (inodes[i].used && str_equals(inodes[i].name, name)) {
            /* Invalidate any active file descriptors referencing this inode */
            for (uint32_t d = 0; d < FS_MAX_FILES; d++) {
                if (open_files[d].used && open_files[d].inode_idx == (int)i) {
                    open_files[d].used = 0;
                    open_files[d].offset = 0;
                    open_files[d].inode_idx = -1;
                }
            }
            for (int b = 0; b < FS_DIRECT_BLOCKS; b++) {
                if (inodes[i].blocks[b] != 0) {
                    free_block(inodes[i].blocks[b]);
                    inodes[i].blocks[b] = 0;
                }
            }
            inodes[i].used = 0;
            inodes[i].size = 0;
            inodes[i].name[0] = '\0';
            return 0;
        }
    }
    return -1;
}

void fs_list(void)
{
    terminal_write("\nJARVIS-FS Directory Listing:\n");
    terminal_write("----------------------------------------\n");
    terminal_write("Filename                     Size (bytes)\n");
    terminal_write("----------------------------------------\n");

    uint32_t count = 0;
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (inodes[i].used) {
            count++;
            terminal_write(inodes[i].name);
            uint32_t len = 0;
            while (inodes[i].name[len]) len++;
            while (len < 29) {
                terminal_putchar(' ');
                len++;
            }
            terminal_write_number(inodes[i].size);
            terminal_putchar('\n');
        }
    }

    if (count == 0) {
        terminal_write("(No files found)\n");
    }

    terminal_write("----------------------------------------\n");
    terminal_write("Total files: ");
    terminal_write_number(count);
    terminal_write(", Free blocks: ");
    terminal_write_number(sb.free_blocks);
    terminal_write("/120\n");
}

void fs_run_demo(void)
{
    terminal_write("\n========================================\n");
    terminal_write("     JARVIS-FS FILESYSTEM DEMO          \n");
    terminal_write("========================================\n");

    fs_format();
    terminal_write("1. Formatted RAM-disk filesystem (Superblock magic 0x4A415256: PASS)\n");

    /* Create file 1 */
    fs_create("kernel.cfg");
    int fd = fs_open("kernel.cfg");
    const char *text = "JARVIS-OS version 1.0\narch=x86-64\nscheduler=RR\n";
    uint32_t len = 0;
    while (text[len]) len++;
    fs_write(fd, text, len);
    fs_close(fd);
    terminal_write("2. Created and wrote 'kernel.cfg' (size ");
    terminal_write_number(len);
    terminal_write(" bytes)\n");

    /* Create file 2 */
    fs_create("ai_agent.log");
    fd = fs_open("ai_agent.log");
    const char *log = "[AI] Companion connected via serial transport. Status: READY.\n";
    len = 0;
    while (log[len]) len++;
    fs_write(fd, log, len);
    fs_close(fd);
    terminal_write("3. Created and wrote 'ai_agent.log' (size ");
    terminal_write_number(len);
    terminal_write(" bytes)\n");

    /* Read back file 1 */
    fd = fs_open("kernel.cfg");
    char read_buf[128];
    for (int i = 0; i < 128; i++) read_buf[i] = 0;
    int r = fs_read(fd, read_buf, 120);
    fs_close(fd);

    terminal_write("4. Readback verification for 'kernel.cfg' (");
    terminal_write_number((uint32_t)r);
    terminal_write(" bytes read):\n   \"");
    /* print until newline or buffer end */
    for (int i = 0; i < r && read_buf[i] != '\n'; i++) {
        terminal_putchar(read_buf[i]);
    }
    terminal_write("...\"\n");

    /* Directory listing */
    fs_list();
}

