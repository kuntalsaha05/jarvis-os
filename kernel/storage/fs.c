#include "fs.h"
#include "block.h"
#include "../drivers/terminal.h"

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

static int normalize_path(const char *path, char *out, uint32_t max_len)
{
    if (path == 0 || out == 0 || max_len == 0) return FS_ERR_INVALID_PARAM;

    /* Skip leading slashes */
    while (*path == '/') path++;

    if (*path == '\0') return FS_ERR_INVALID_PARAM;

    uint32_t len = 0;
    while (path[len] != '\0') {
        /* Disallow control characters or secondary slashes (flat namespace) */
        if ((unsigned char)path[len] < 32 || path[len] == '/') {
            return FS_ERR_INVALID_PARAM;
        }
        len++;
    }

    if (len >= max_len || len >= FS_MAX_FILENAME) {
        return FS_ERR_INVALID_PARAM;
    }

    str_copy(out, path, max_len);
    return FS_OK;
}

static int alloc_block(void)
{
    for (uint32_t i = 0; i < FS_TOTAL_DATA_BLOCKS; i++) {
        uint32_t byte_idx = i / 8;
        uint8_t bit_mask = 1 << (i % 8);
        if ((block_bitmap[byte_idx] & bit_mask) == 0) {
            block_bitmap[byte_idx] |= bit_mask;
            if (sb.free_blocks > 0) sb.free_blocks--;
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
            uint32_t byte_idx = i / 8;
            uint8_t bit_mask = 1 << (i % 8);
            if ((block_bitmap[byte_idx] & bit_mask) != 0) {
                block_bitmap[byte_idx] &= ~bit_mask;
                sb.free_blocks++;
            }
        }
    }
}

int fs_format(void)
{
    sb.magic = FS_MAGIC;
    sb.total_blocks = FS_TOTAL_BLOCKS;
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
        open_files[i].offset = 0;
        open_files[i].inode_idx = -1;
    }

    fs_mounted = 1;
    return FS_OK;
}

int fs_init(void)
{
    int ret = fs_format();
    if (ret == FS_OK) {
        fs_create("kernel.cfg");
        int fd = fs_open("kernel.cfg");
        if (fd >= 0) {
            const char *cfg = "JARVIS-OS v1.0\narch=x86-64\nscheduler=RoundRobin\nquantum=5\nai_bridge=COM1\n";
            uint32_t len = 0;
            while (cfg[len]) len++;
            fs_write(fd, cfg, len);
            fs_close(fd);
        }
    }
    return ret;
}

int fs_get_info(fs_info_t *info)
{
    if (!fs_mounted || info == 0) return FS_ERR_NOT_MOUNTED;

    info->magic = sb.magic;
    info->total_blocks = sb.total_blocks;
    info->free_blocks = sb.free_blocks;
    info->used_blocks = sb.total_blocks - sb.free_blocks;
    info->total_inodes = sb.inode_count;
    info->block_size = FS_BLOCK_SIZE;
    info->max_file_size = FS_MAX_FILE_SIZE;

    uint32_t used_inodes = 0;
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (inodes[i].used) used_inodes++;
    }
    info->used_inodes = used_inodes;
    info->free_inodes = sb.inode_count - used_inodes;

    return FS_OK;
}

int fs_get_inode(uint32_t idx, fs_inode_t *out_node)
{
    if (!fs_mounted || out_node == 0 || idx >= FS_MAX_FILES) {
        return FS_ERR_INVALID_PARAM;
    }
    *out_node = inodes[idx];
    return FS_OK;
}

int fs_create(const char *name)
{
    if (!fs_mounted) return FS_ERR_NOT_MOUNTED;

    char clean_name[FS_MAX_FILENAME];
    int norm = normalize_path(name, clean_name, sizeof(clean_name));
    if (norm != FS_OK) return norm;

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (inodes[i].used && str_equals(inodes[i].name, clean_name)) {
            return FS_ERR_ALREADY_EXISTS;
        }
    }

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (!inodes[i].used) {
            inodes[i].used = 1;
            inodes[i].size = 0;
            str_copy(inodes[i].name, clean_name, FS_MAX_FILENAME);
            for (int b = 0; b < FS_DIRECT_BLOCKS; b++) inodes[i].blocks[b] = 0;
            return FS_OK;
        }
    }

    return FS_ERR_INODE_FULL;
}

int fs_open(const char *name)
{
    if (!fs_mounted) return FS_ERR_NOT_MOUNTED;

    char clean_name[FS_MAX_FILENAME];
    int norm = normalize_path(name, clean_name, sizeof(clean_name));
    if (norm != FS_OK) return norm;

    int inode_idx = -1;
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (inodes[i].used && str_equals(inodes[i].name, clean_name)) {
            inode_idx = (int)i;
            break;
        }
    }
    if (inode_idx == -1) return FS_ERR_NOT_FOUND;

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (!open_files[i].used) {
            open_files[i].used = 1;
            open_files[i].inode_idx = inode_idx;
            open_files[i].offset = 0;
            return (int)i;
        }
    }

    return FS_ERR_TOO_MANY_OPEN;
}

int fs_write(int fd, const void *buf, uint32_t count)
{
    if (!fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (fd < 0 || fd >= FS_MAX_FILES || !open_files[fd].used) return FS_ERR_BAD_FD;
    if (buf == 0) return FS_ERR_INVALID_PARAM;
    if (open_files[fd].inode_idx < 0 || open_files[fd].inode_idx >= FS_MAX_FILES) return FS_ERR_BAD_FD;

    fs_inode_t *node = &inodes[open_files[fd].inode_idx];
    if (!node->used) return FS_ERR_BAD_FD;

    if (count == 0) return 0;

    if (open_files[fd].offset >= FS_MAX_FILE_SIZE) {
        return FS_ERR_FILE_TOO_LARGE;
    }

    /* Bound count to max file capacity */
    if (open_files[fd].offset + count > FS_MAX_FILE_SIZE) {
        count = FS_MAX_FILE_SIZE - open_files[fd].offset;
    }

    /* Save original metadata for rollback on allocation failure */
    uint32_t orig_size = node->size;
    uint32_t orig_offset = open_files[fd].offset;
    uint32_t orig_blocks[FS_DIRECT_BLOCKS];
    for (int b = 0; b < FS_DIRECT_BLOCKS; b++) orig_blocks[b] = node->blocks[b];

    uint32_t newly_allocated[FS_DIRECT_BLOCKS];
    uint32_t newly_allocated_count = 0;

    const uint8_t *src = (const uint8_t *)buf;
    uint32_t written = 0;

    while (written < count) {
        uint32_t block_index = open_files[fd].offset / FS_BLOCK_SIZE;
        uint32_t block_offset = open_files[fd].offset % FS_BLOCK_SIZE;

        if (block_index >= FS_DIRECT_BLOCKS) {
            break;
        }

        if (node->blocks[block_index] == 0) {
            int b = alloc_block();
            if (b == -1) {
                /* Storage exhaustion: Rollback newly allocated blocks */
                for (uint32_t r = 0; r < newly_allocated_count; r++) {
                    free_block(newly_allocated[r]);
                }
                for (int r = 0; r < FS_DIRECT_BLOCKS; r++) {
                    node->blocks[r] = orig_blocks[r];
                }
                node->size = orig_size;
                open_files[fd].offset = orig_offset;
                return FS_ERR_DISK_FULL;
            }
            node->blocks[block_index] = (uint32_t)b;
            newly_allocated[newly_allocated_count++] = (uint32_t)b;

            /* Zero the newly allocated block */
            uint8_t zero_buf[FS_BLOCK_SIZE];
            for (int z = 0; z < FS_BLOCK_SIZE; z++) zero_buf[z] = 0;
            block_write((uint64_t)b, 1, zero_buf);
        }

        uint8_t sector_buf[FS_BLOCK_SIZE];
        int r_res = block_read(node->blocks[block_index], 1, sector_buf);
        if (r_res < 0) {
            /* Rollback */
            for (uint32_t r = 0; r < newly_allocated_count; r++) free_block(newly_allocated[r]);
            for (int r = 0; r < FS_DIRECT_BLOCKS; r++) node->blocks[r] = orig_blocks[r];
            node->size = orig_size;
            open_files[fd].offset = orig_offset;
            return FS_ERR_IO;
        }

        uint32_t to_write = FS_BLOCK_SIZE - block_offset;
        if (to_write > count - written) to_write = count - written;

        for (uint32_t i = 0; i < to_write; i++) {
            sector_buf[block_offset + i] = src[written + i];
        }

        int w_res = block_write(node->blocks[block_index], 1, sector_buf);
        if (w_res < 0) {
            /* Rollback */
            for (uint32_t r = 0; r < newly_allocated_count; r++) free_block(newly_allocated[r]);
            for (int r = 0; r < FS_DIRECT_BLOCKS; r++) node->blocks[r] = orig_blocks[r];
            node->size = orig_size;
            open_files[fd].offset = orig_offset;
            return FS_ERR_IO;
        }

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
    if (!fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (fd < 0 || fd >= FS_MAX_FILES || !open_files[fd].used) return FS_ERR_BAD_FD;
    if (buf == 0) return FS_ERR_INVALID_PARAM;
    if (open_files[fd].inode_idx < 0 || open_files[fd].inode_idx >= FS_MAX_FILES) return FS_ERR_BAD_FD;

    fs_inode_t *node = &inodes[open_files[fd].inode_idx];
    if (!node->used) return FS_ERR_BAD_FD;

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
        int r_res = block_read(node->blocks[block_index], 1, sector_buf);
        if (r_res < 0) return FS_ERR_IO;

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
        if (!open_files[fd].used) return FS_ERR_BAD_FD;
        open_files[fd].used = 0;
        open_files[fd].offset = 0;
        open_files[fd].inode_idx = -1;
        return FS_OK;
    }
    return FS_ERR_BAD_FD;
}

int fs_truncate(int fd, uint32_t new_size)
{
    if (!fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (fd < 0 || fd >= FS_MAX_FILES || !open_files[fd].used) return FS_ERR_BAD_FD;
    if (open_files[fd].inode_idx < 0 || open_files[fd].inode_idx >= FS_MAX_FILES) return FS_ERR_BAD_FD;

    fs_inode_t *node = &inodes[open_files[fd].inode_idx];
    if (!node->used) return FS_ERR_BAD_FD;

    if (new_size > FS_MAX_FILE_SIZE) return FS_ERR_FILE_TOO_LARGE;

    if (new_size < node->size) {
        uint32_t needed_blocks = (new_size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
        if (new_size == 0) needed_blocks = 0;

        for (uint32_t b = needed_blocks; b < FS_DIRECT_BLOCKS; b++) {
            if (node->blocks[b] != 0) {
                free_block(node->blocks[b]);
                node->blocks[b] = 0;
            }
        }

        /* If partial last block remains, zero the truncated tail */
        if (needed_blocks > 0 && (new_size % FS_BLOCK_SIZE != 0)) {
            uint32_t last_idx = needed_blocks - 1;
            uint32_t tail_offset = new_size % FS_BLOCK_SIZE;
            uint8_t sector_buf[FS_BLOCK_SIZE];
            if (block_read(node->blocks[last_idx], 1, sector_buf) == 0) {
                for (uint32_t z = tail_offset; z < FS_BLOCK_SIZE; z++) {
                    sector_buf[z] = 0;
                }
                block_write(node->blocks[last_idx], 1, sector_buf);
            }
        }

        node->size = new_size;
        if (open_files[fd].offset > new_size) open_files[fd].offset = new_size;
        return FS_OK;
    }
    else if (new_size > node->size) {
        uint32_t needed_blocks = (new_size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
        for (uint32_t b = 0; b < needed_blocks; b++) {
            if (node->blocks[b] == 0) {
                int blk = alloc_block();
                if (blk == -1) return FS_ERR_DISK_FULL;
                node->blocks[b] = (uint32_t)blk;
                uint8_t zero_buf[FS_BLOCK_SIZE];
                for (int z = 0; z < FS_BLOCK_SIZE; z++) zero_buf[z] = 0;
                block_write((uint64_t)blk, 1, zero_buf);
            }
        }
        node->size = new_size;
        return FS_OK;
    }

    return FS_OK;
}

int fs_truncate_by_name(const char *name, uint32_t new_size)
{
    int fd = fs_open(name);
    if (fd < 0) return fd;
    int res = fs_truncate(fd, new_size);
    fs_close(fd);
    return res;
}

int fs_delete(const char *name)
{
    if (!fs_mounted) return FS_ERR_NOT_MOUNTED;

    char clean_name[FS_MAX_FILENAME];
    int norm = normalize_path(name, clean_name, sizeof(clean_name));
    if (norm != FS_OK) return norm;

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (inodes[i].used && str_equals(inodes[i].name, clean_name)) {
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
            return FS_OK;
        }
    }
    return FS_ERR_NOT_FOUND;
}

int fs_stat(const char *name, fs_stat_t *st)
{
    if (!fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (st == 0) return FS_ERR_INVALID_PARAM;

    char clean_name[FS_MAX_FILENAME];
    int norm = normalize_path(name, clean_name, sizeof(clean_name));
    if (norm != FS_OK) return norm;

    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (inodes[i].used && str_equals(inodes[i].name, clean_name)) {
            str_copy(st->name, inodes[i].name, FS_MAX_FILENAME);
            st->size = inodes[i].size;
            st->in_use = 1;
            uint32_t blk_count = 0;
            for (int b = 0; b < FS_DIRECT_BLOCKS; b++) {
                if (inodes[i].blocks[b] != 0) blk_count++;
            }
            st->blocks_allocated = blk_count;
            return FS_OK;
        }
    }
    return FS_ERR_NOT_FOUND;
}

void fs_list(void)
{
    terminal_write("\nJARVIS-FS Directory Listing:\n");
    terminal_write("----------------------------------------------------\n");
    terminal_write("Filename                     Size (bytes)   Blocks\n");
    terminal_write("----------------------------------------------------\n");

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
            
            uint32_t blk_count = 0;
            for (int b = 0; b < FS_DIRECT_BLOCKS; b++) {
                if (inodes[i].blocks[b] != 0) blk_count++;
            }
            terminal_write("          ");
            terminal_write_number(blk_count);
            terminal_putchar('\n');
        }
    }

    if (count == 0) {
        terminal_write("(No files found)\n");
    }

    terminal_write("----------------------------------------------------\n");
    terminal_write("Total files: ");
    terminal_write_number(count);
    terminal_write(" / 16, Free blocks: ");
    terminal_write_number(sb.free_blocks);
    terminal_write(" / 120 (512B/blk)\n");
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
    for (int i = 0; i < r && read_buf[i] != '\n'; i++) {
        terminal_putchar(read_buf[i]);
    }
    terminal_write("...\"\n");

    /* Directory listing */
    fs_list();
}

int test_fs_invariants(void)
{
    /* 1. Format clean filesystem for testing */
    if (fs_format() != FS_OK) return -1;

    /* 2. Verify Superblock */
    fs_info_t info;
    if (fs_get_info(&info) != FS_OK) return -2;
    if (info.magic != FS_MAGIC) return -3;
    if (info.total_blocks != FS_TOTAL_BLOCKS) return -4;
    if (info.free_blocks != FS_TOTAL_DATA_BLOCKS) return -5;
    if (info.total_inodes != FS_MAX_FILES) return -6;

    /* 3. Path Validation Checks */
    if (fs_create(0) != FS_ERR_INVALID_PARAM) return -7;
    if (fs_create("") != FS_ERR_INVALID_PARAM) return -8;
    if (fs_create("a_very_long_file_name_that_exceeds_limits") != FS_ERR_INVALID_PARAM) return -9;

    /* Path with leading slash normalized */
    if (fs_create("/test_file.txt") != FS_OK) return -10;
    if (fs_create("test_file.txt") != FS_ERR_ALREADY_EXISTS) return -11;

    /* 4. Write/Read Roundtrip */
    int fd = fs_open("/test_file.txt");
    if (fd < 0) return -12;

    const char *payload = "JARVIS-OS File System Invariant Test Payload";
    uint32_t plen = 0;
    while (payload[plen]) plen++;

    int w = fs_write(fd, payload, plen);
    if (w != (int)plen) return -13;
    fs_close(fd);

    fd = fs_open("test_file.txt");
    if (fd < 0) return -14;

    char read_buf[64];
    for (int i = 0; i < 64; i++) read_buf[i] = 0;
    int r = fs_read(fd, read_buf, plen);
    if (r != (int)plen) return -15;

    for (uint32_t i = 0; i < plen; i++) {
        if (read_buf[i] != payload[i]) return -16;
    }
    fs_close(fd);

    /* 5. Empty File Check */
    if (fs_create("empty.dat") != FS_OK) return -17;
    fd = fs_open("empty.dat");
    if (fd < 0) return -18;
    r = fs_read(fd, read_buf, 10);
    if (r != 0) return -19; /* EOF on empty file */
    fs_close(fd);

    /* 6. Truncation and Block Release */
    fd = fs_open("test_file.txt");
    if (fd < 0) return -20;
    /* Current size is plen (~44 bytes, 1 block). Write more to use 2 blocks (> 512 bytes) */
    uint8_t big_buf[600];
    for (int i = 0; i < 600; i++) big_buf[i] = (uint8_t)(i & 0xFF);
    w = fs_write(fd, big_buf, 600);
    if (w != 600) return -21;

    fs_stat_t st;
    if (fs_stat("test_file.txt", &st) != FS_OK) return -22;
    if (st.blocks_allocated != 2) return -23;

    /* Truncate down to 20 bytes: should free block 2 and leave 1 block */
    if (fs_truncate(fd, 20) != FS_OK) return -24;
    if (fs_stat("test_file.txt", &st) != FS_OK) return -25;
    if (st.size != 20) return -26;
    if (st.blocks_allocated != 1) return -27;
    fs_close(fd);

    /* 7. Deletion, Invalidation, and Block Reuse */
    uint32_t free_before = sb.free_blocks;
    if (fs_create("reuse_test.bin") != FS_OK) return -28;
    fd = fs_open("reuse_test.bin");
    if (fd < 0) return -29;
    w = fs_write(fd, big_buf, 600); /* allocates 2 blocks */
    if (w != 600) return -30;
    if (sb.free_blocks != free_before - 2) return -31;

    /* Delete while fd is open: should invalidate fd and reclaim blocks */
    if (fs_delete("reuse_test.bin") != FS_OK) return -32;
    if (sb.free_blocks != free_before) return -33; /* Blocks reclaimed */

    /* Subsequence write/read on invalidated fd must fail */
    if (fs_write(fd, big_buf, 10) != FS_ERR_BAD_FD) return -34;
    if (fs_read(fd, big_buf, 10) != FS_ERR_BAD_FD) return -35;

    /* 8. Boundary Size Limit (4096 bytes = 8 direct blocks) */
    if (fs_create("boundary.bin") != FS_OK) return -36;
    fd = fs_open("boundary.bin");
    if (fd < 0) return -37;

    uint8_t chunk[512];
    for (int i = 0; i < 512; i++) chunk[i] = 0x55;
    for (int b = 0; b < FS_DIRECT_BLOCKS; b++) {
        w = fs_write(fd, chunk, 512);
        if (w != 512) return -38;
    }
    if (fs_stat("boundary.bin", &st) != FS_OK) return -39;
    if (st.size != FS_MAX_FILE_SIZE) return -40;
    if (st.blocks_allocated != FS_DIRECT_BLOCKS) return -41;

    /* Exceeding max file size returns FS_ERR_FILE_TOO_LARGE */
    if (fs_write(fd, chunk, 1) != FS_ERR_FILE_TOO_LARGE) return -42;
    fs_close(fd);

    /* 9. Inode Exhaustion Test */
    /* Currently 3 files exist: test_file.txt, empty.dat, boundary.bin */
    for (int i = 3; i < FS_MAX_FILES; i++) {
        char fname[16];
        fname[0] = 'f'; fname[1] = 'i'; fname[2] = 'l'; fname[3] = 'e';
        fname[4] = (char)('0' + (i / 10));
        fname[5] = (char)('0' + (i % 10));
        fname[6] = '\0';
        if (fs_create(fname) != FS_OK) return -43;
    }
    /* 17th file creation must fail with FS_ERR_INODE_FULL */
    if (fs_create("overflow.bin") != FS_ERR_INODE_FULL) return -44;

    /* Delete one file, verify slot freed */
    if (fs_delete("empty.dat") != FS_OK) return -45;
    if (fs_create("new_slot.bin") != FS_OK) return -46;

    /* Clean format and reinitialize default kernel.cfg */
    fs_init();
    return 0;
}
