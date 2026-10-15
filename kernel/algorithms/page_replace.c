#include "page_replace.h"
#include "../drivers/terminal.h"

uint32_t page_replace_fifo(const uint32_t *refs, uint32_t ref_count, uint32_t frame_count)
{
    if (frame_count == 0 || frame_count > PAGE_REPLACE_MAX_FRAMES) return 0;

    int32_t frames[PAGE_REPLACE_MAX_FRAMES];
    for (uint32_t i = 0; i < frame_count; i++) frames[i] = -1;

    uint32_t fifo_head = 0;
    uint32_t faults = 0;

    for (uint32_t i = 0; i < ref_count; i++) {
        uint32_t page = refs[i];
        int hit = 0;

        for (uint32_t j = 0; j < frame_count; j++) {
            if (frames[j] == (int32_t)page) {
                hit = 1;
                break;
            }
        }

        if (!hit) {
            faults++;
            frames[fifo_head] = (int32_t)page;
            fifo_head = (fifo_head + 1) % frame_count;
        }
    }

    return faults;
}

uint32_t page_replace_lru(const uint32_t *refs, uint32_t ref_count, uint32_t frame_count)
{
    if (frame_count == 0 || frame_count > PAGE_REPLACE_MAX_FRAMES) return 0;

    int32_t frames[PAGE_REPLACE_MAX_FRAMES];
    uint32_t last_used[PAGE_REPLACE_MAX_FRAMES];
    for (uint32_t i = 0; i < frame_count; i++) {
        frames[i] = -1;
        last_used[i] = 0;
    }

    uint32_t faults = 0;

    for (uint32_t time = 0; time < ref_count; time++) {
        uint32_t page = refs[time];
        int hit_idx = -1;

        for (uint32_t j = 0; j < frame_count; j++) {
            if (frames[j] == (int32_t)page) {
                hit_idx = (int)j;
                break;
            }
        }

        if (hit_idx != -1) {
            last_used[hit_idx] = time + 1;
        } else {
            faults++;
            /* Find empty frame or LRU frame */
            int replace_idx = -1;
            uint32_t oldest_time = 0xFFFFFFFF;

            for (uint32_t j = 0; j < frame_count; j++) {
                if (frames[j] == -1) {
                    replace_idx = (int)j;
                    break;
                }
                if (last_used[j] < oldest_time) {
                    oldest_time = last_used[j];
                    replace_idx = (int)j;
                }
            }

            frames[replace_idx] = (int32_t)page;
            last_used[replace_idx] = time + 1;
        }
    }

    return faults;
}

uint32_t page_replace_optimal(const uint32_t *refs, uint32_t ref_count, uint32_t frame_count)
{
    if (frame_count == 0 || frame_count > PAGE_REPLACE_MAX_FRAMES) return 0;

    int32_t frames[PAGE_REPLACE_MAX_FRAMES];
    for (uint32_t i = 0; i < frame_count; i++) frames[i] = -1;

    uint32_t faults = 0;

    for (uint32_t i = 0; i < ref_count; i++) {
        uint32_t page = refs[i];
        int hit = 0;

        for (uint32_t j = 0; j < frame_count; j++) {
            if (frames[j] == (int32_t)page) {
                hit = 1;
                break;
            }
        }

        if (!hit) {
            faults++;
            int empty_idx = -1;
            for (uint32_t j = 0; j < frame_count; j++) {
                if (frames[j] == -1) {
                    empty_idx = (int)j;
                    break;
                }
            }

            if (empty_idx != -1) {
                frames[empty_idx] = (int32_t)page;
            } else {
                /* Find victim: page used farthest in future */
                int victim_idx = -1;
                uint32_t farthest = i + 1;

                for (uint32_t j = 0; j < frame_count; j++) {
                    uint32_t next_use = 0xFFFFFFFF;
                    for (uint32_t k = i + 1; k < ref_count; k++) {
                        if ((int32_t)refs[k] == frames[j]) {
                            next_use = k;
                            break;
                        }
                    }

                    if (next_use == 0xFFFFFFFF) {
                        victim_idx = (int)j;
                        break;
                    }

                    if (next_use > farthest) {
                        farthest = next_use;
                        victim_idx = (int)j;
                    }
                }

                if (victim_idx == -1) victim_idx = 0;
                frames[victim_idx] = (int32_t)page;
            }
        }
    }

    return faults;
}

void page_replace_belady_demo(void)
{
    /* Belady's Anomaly: 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5 */
    static const uint32_t belady_refs[12] = {
        1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5
    };

    terminal_write("\n========================================\n");
    terminal_write("       BELADY'S ANOMALY DEMONSTRATION   \n");
    terminal_write("========================================\n");
    terminal_write("Reference string: 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5 (12 refs)\n\n");

    uint32_t faults_3 = page_replace_fifo(belady_refs, 12, 3);
    uint32_t faults_4 = page_replace_fifo(belady_refs, 12, 4);

    terminal_write("FIFO with 3 page frames: ");
    terminal_write_number(faults_3);
    terminal_write(" page faults\n");

    terminal_write("FIFO with 4 page frames: ");
    terminal_write_number(faults_4);
    terminal_write(" page faults\n\n");

    if (faults_4 > faults_3) {
        terminal_write("Result: ANOMALY CONFIRMED!\n");
        terminal_write("Increasing physical frame allocation from 3 to 4\n");
        terminal_write("increased total page faults from 9 to 10 under FIFO.\n");
    } else {
        terminal_write("Result: Anomaly not observed.\n");
    }
}

void page_replace_run_demo(void)
{
    static const uint32_t refs[20] = {
        7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1
    };
    uint32_t frame_count = 3;

    terminal_write("\n========================================\n");
    terminal_write("    PAGE REPLACEMENT ALGORITHM BENCHMARK\n");
    terminal_write("========================================\n");
    terminal_write("Ref string: 7,0,1,2,0,3,0,4,2,3,0,3,2,1,2,0,1,7,0,1 (20 refs)\n");
    terminal_write("Physical frame capacity: 3 frames\n\n");

    uint32_t fifo_faults = page_replace_fifo(refs, 20, frame_count);
    uint32_t lru_faults = page_replace_lru(refs, 20, frame_count);
    uint32_t opt_faults = page_replace_optimal(refs, 20, frame_count);

    terminal_write("1. FIFO Replacement:    ");
    terminal_write_number(fifo_faults);
    terminal_write(" page faults (Hits: ");
    terminal_write_number(20 - fifo_faults);
    terminal_write(")\n");

    terminal_write("2. LRU Replacement:     ");
    terminal_write_number(lru_faults);
    terminal_write(" page faults (Hits: ");
    terminal_write_number(20 - lru_faults);
    terminal_write(")\n");

    terminal_write("3. Optimal Replacement: ");
    terminal_write_number(opt_faults);
    terminal_write(" page faults (Hits: ");
    terminal_write_number(20 - opt_faults);
    terminal_write(")\n\n");

    page_replace_belady_demo();
}

