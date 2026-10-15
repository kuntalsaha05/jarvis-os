#include "file_alloc.h"
#include "../drivers/terminal.h"

static void copy_filename(char *dst, const char *src, uint32_t max_len)
{
    uint32_t i = 0;
    while (src && src[i] != '\0' && i + 1 < max_len) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void file_alloc_disk_init(file_alloc_disk_t *disk)
{
    disk->total_blocks = FILE_ALLOC_DISK_BLOCKS;
    disk->free_blocks = FILE_ALLOC_DISK_BLOCKS;
    for (uint32_t i = 0; i < FILE_ALLOC_DISK_BLOCKS; i++) {
        disk->bitmap[i] = 0;
    }
}

int file_alloc_contiguous(file_alloc_disk_t *disk, const char *name, uint32_t blocks, contiguous_file_t *out)
{
    copy_filename(out->name, name, sizeof(out->name));
    out->size_blocks = blocks;
    out->start_block = 0;
    out->success = 0;

    if (blocks == 0 || blocks > disk->free_blocks) {
        return -1; /* Out of space or invalid request */
    }

    /* Find consecutive run of 'blocks' free blocks */
    for (uint32_t i = 0; i + blocks <= disk->total_blocks; i++) {
        int contiguous = 1;
        for (uint32_t j = 0; j < blocks; j++) {
            if (disk->bitmap[i + j] != 0) {
                contiguous = 0;
                break;
            }
        }

        if (contiguous) {
            for (uint32_t j = 0; j < blocks; j++) {
                disk->bitmap[i + j] = 1;
            }
            disk->free_blocks -= blocks;
            out->start_block = i;
            out->success = 1;
            return 0;
        }
    }

    return -2; /* External fragmentation: space exists, but not contiguous! */
}

int file_alloc_linked(file_alloc_disk_t *disk, const char *name, uint32_t blocks, linked_file_t *out)
{
    copy_filename(out->name, name, sizeof(out->name));
    out->size_blocks = blocks;
    out->start_block = 0;
    out->end_block = 0;
    out->success = 0;

    if (blocks == 0 || blocks > disk->free_blocks || blocks > 16) {
        return -1;
    }

    uint32_t allocated = 0;
    for (uint32_t i = 0; i < disk->total_blocks && allocated < blocks; i++) {
        if (disk->bitmap[i] == 0) {
            disk->bitmap[i] = 1;
            out->blocks[allocated] = i;
            if (allocated == 0) out->start_block = i;
            out->end_block = i;
            allocated++;
        }
    }

    if (allocated == blocks) {
        disk->free_blocks -= blocks;
        out->success = 1;
        return 0;
    }

    return -1;
}

int file_alloc_indexed(file_alloc_disk_t *disk, const char *name, uint32_t blocks, indexed_file_t *out)
{
    copy_filename(out->name, name, sizeof(out->name));
    out->size_blocks = blocks;
    out->index_block = 0;
    out->success = 0;

    /* Needs 1 index block + 'blocks' data blocks */
    if (blocks == 0 || (blocks + 1) > disk->free_blocks || blocks > 16) {
        return -1;
    }

    /* Allocate index block first */
    int index_blk = -1;
    for (uint32_t i = 0; i < disk->total_blocks; i++) {
        if (disk->bitmap[i] == 0) {
            disk->bitmap[i] = 1;
            index_blk = (int)i;
            break;
        }
    }

    if (index_blk == -1) return -1;
    out->index_block = (uint32_t)index_blk;

    /* Allocate data blocks */
    uint32_t allocated = 0;
    for (uint32_t i = 0; i < disk->total_blocks && allocated < blocks; i++) {
        if (disk->bitmap[i] == 0) {
            disk->bitmap[i] = 1;
            out->blocks[allocated++] = i;
        }
    }

    if (allocated == blocks) {
        disk->free_blocks -= (blocks + 1);
        out->success = 1;
        return 0;
    }

    return -1;
}

int file_alloc_contiguous_lookup(const contiguous_file_t *f, uint32_t logical_block)
{
    if (!f->success || logical_block >= f->size_blocks) return -1;
    return (int)(f->start_block + logical_block);
}

int file_alloc_linked_lookup(const linked_file_t *f, uint32_t logical_block)
{
    if (!f->success || logical_block >= f->size_blocks || logical_block >= 16) return -1;
    return (int)f->blocks[logical_block];
}

int file_alloc_indexed_lookup(const indexed_file_t *f, uint32_t logical_block)
{
    if (!f->success || logical_block >= f->size_blocks || logical_block >= 16) return -1;
    return (int)f->blocks[logical_block];
}

void file_alloc_run_demo(void)
{
    terminal_write("\n========================================\n");
    terminal_write("     FILE ALLOCATION METHODS DEMO       \n");
    terminal_write("========================================\n");
    terminal_write("Simulated Disk Capacity: 64 Blocks\n\n");

    file_alloc_disk_t disk;

    /* 1. Contiguous Allocation */
    file_alloc_disk_init(&disk);
    terminal_write("1. Contiguous Allocation Test:\n");
    contiguous_file_t cf1, cf2, cf3;
    file_alloc_contiguous(&disk, "file1.dat", 4, &cf1);
    /* Artificially fragment disk: reserve block 6 */
    disk.bitmap[5] = 1; disk.free_blocks--;
    file_alloc_contiguous(&disk, "file2.dat", 3, &cf2);
    /* Request 6 blocks: 58 free blocks remain, but max contiguous run is 56 */
    int c3_res = file_alloc_contiguous(&disk, "file3.dat", 10, &cf3);

    terminal_write("   file1.dat: Start Block ");
    terminal_write_number(cf1.start_block);
    terminal_write(", Length 4 -> Physical Blocks [");
    terminal_write_number(cf1.start_block);
    terminal_write("..");
    terminal_write_number(cf1.start_block + 3);
    terminal_write("]\n");

    terminal_write("   file2.dat: Start Block ");
    terminal_write_number(cf2.start_block);
    terminal_write(", Length 3 -> Physical Blocks [");
    terminal_write_number(cf2.start_block);
    terminal_write("..");
    terminal_write_number(cf2.start_block + 2);
    terminal_write("]\n");

    terminal_write("   file3.dat: Allocation result: ");
    terminal_write(c3_res == 0 ? "SUCCESS\n" : "FRAGMENTATION REJECTED\n");
    terminal_write("   Direct Lookup: Logical block 2 of file1 -> Physical Block ");
    terminal_write_number((uint32_t)file_alloc_contiguous_lookup(&cf1, 2));
    terminal_write(" (O(1) access)\n");

    /* 2. Linked Allocation */
    file_alloc_disk_init(&disk);
    terminal_write("\n2. Linked Allocation Test (No External Fragmentation):\n");
    linked_file_t lf;
    /* Create scattered allocation */
    disk.bitmap[0] = disk.bitmap[2] = disk.bitmap[4] = 1;
    disk.free_blocks -= 3;
    file_alloc_linked(&disk, "linked.dat", 4, &lf);

    terminal_write("   linked.dat (4 blocks): Start=");
    terminal_write_number(lf.start_block);
    terminal_write(", End=");
    terminal_write_number(lf.end_block);
    terminal_write("\n   Block Chain: ");
    for (uint32_t i = 0; i < lf.size_blocks; i++) {
        terminal_write_number(lf.blocks[i]);
        if (i + 1 < lf.size_blocks) terminal_write(" -> ");
    }
    terminal_write("\n   Direct Lookup: Logical block 3 -> Physical Block ");
    terminal_write_number((uint32_t)file_alloc_linked_lookup(&lf, 3));
    terminal_write(" (O(N) sequential traversal)\n");

    /* 3. Indexed Allocation */
    file_alloc_disk_init(&disk);
    terminal_write("\n3. Indexed Allocation Test (Index Block + Direct Access):\n");
    indexed_file_t idx_f;
    file_alloc_indexed(&disk, "indexed.dat", 4, &idx_f);

    terminal_write("   indexed.dat: Index Block = ");
    terminal_write_number(idx_f.index_block);
    terminal_write(" (Stores pointer table)\n   Data Blocks: [ ");
    for (uint32_t i = 0; i < idx_f.size_blocks; i++) {
        terminal_write_number(idx_f.blocks[i]);
        terminal_putchar(' ');
    }
    terminal_write("]\n   Direct Lookup: Logical block 2 -> Physical Block ");
    terminal_write_number((uint32_t)file_alloc_indexed_lookup(&idx_f, 2));
    terminal_write(" (O(1) via index table)\n");

    terminal_write("\nComparison Summary:\n");
    terminal_write("Method       Sequential Access  Random Access  Fragmentation  Overhead\n");
    terminal_write("Contiguous   Fastest            Fastest O(1)   External       None\n");
    terminal_write("Linked       Fast               Slow O(N)      None           Pointer per block\n");
    terminal_write("Indexed      Fast               Fast O(1)      None           1 Index block\n");
}

