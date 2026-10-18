#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include "memory/heap.h"
#include "storage/ramdisk.h"
#include "storage/fs.h"
#include "drivers/framebuffer.h"
#include "gui/font.h"
#include "gui/graphics.h"
#include "gui/wm.h"
#include "gui/event.h"

/* Terminal and debug stubs for host test */
void terminal_write(const char *str) { (void)str; }
void terminal_putchar(char c) { (void)c; }
void terminal_write_number(uint32_t n) { (void)n; }
void debug_puts(const char *str) { (void)str; }

static double get_time_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(void)
{
    printf("==================================================\n");
    printf("     JARVIS-OS PHASE 13 PERFORMANCE BENCHMARK     \n");
    printf("==================================================\n");

    /* 1. Dynamic Heap Memory Allocation & Coalescing Benchmark */
    {
        heap_init();
        const int ITERS = 10000;
        double t0 = get_time_sec();
        int success_count = 0;
        for (int i = 0; i < ITERS; i++) {
            size_t sz = 16 * ((i % 8) + 1);
            void *p = kmalloc(sz);
            if (p) {
                success_count++;
                assert(((uintptr_t)p % 16) == 0);
                kfree(p);
            }
        }
        double elapsed = get_time_sec() - t0;
        double ops_per_sec = (ITERS * 2.0) / elapsed;
        double avg_lat_us = (elapsed / (ITERS * 2.0)) * 1e6;
        printf("[PERF] Heap kmalloc/kfree:\n");
        printf("       Iterations: %d alloc+free cycles\n", ITERS);
        printf("       Success Rate: %.2f%%\n", (double)success_count / ITERS * 100.0);
        printf("       Throughput:   %.2f ops/sec\n", ops_per_sec);
        printf("       Avg Latency:  %.4f us/op\n\n", avg_lat_us);
    }

    /* 2. Filesystem Read/Write Latency Benchmark */
    {
        ramdisk_init();
        fs_format();
        const int FS_ITERS = 500;
        uint8_t buffer[512];
        for (int i = 0; i < 512; i++) buffer[i] = (uint8_t)(i & 0xFF);

        double t0 = get_time_sec();
        for (int i = 0; i < FS_ITERS; i++) {
            char name[16];
            snprintf(name, sizeof(name), "file_%d.dat", i % 10);
            if (i < 10) {
                fs_create(name);
            }
            int fd = fs_open(name);
            if (fd >= 0) {
                fs_write(fd, buffer, 512);
                fs_close(fd);
            }
        }
        double elapsed_write = get_time_sec() - t0;

        double t1 = get_time_sec();
        for (int i = 0; i < FS_ITERS; i++) {
            char name[16];
            snprintf(name, sizeof(name), "file_%d.dat", i % 10);
            int fd = fs_open(name);
            if (fd >= 0) {
                uint8_t read_buf[512];
                fs_read(fd, read_buf, 512);
                fs_close(fd);
            }
        }
        double elapsed_read = get_time_sec() - t1;

        printf("[PERF] JARVIS-FS Operations (RAMDisk backed):\n");
        printf("       Write Latency (512B): %.3f us/op (%.1f MB/s)\n",
               (elapsed_write / FS_ITERS) * 1e6, (FS_ITERS * 512.0 / (1024.0 * 1024.0)) / elapsed_write);
        printf("       Read Latency  (512B): %.3f us/op (%.1f MB/s)\n\n",
               (elapsed_read / FS_ITERS) * 1e6, (FS_ITERS * 512.0 / (1024.0 * 1024.0)) / elapsed_read);
    }

    /* 3. Framebuffer Redraw & Window Composition Benchmark */
    {
        /* Setup simulated linear framebuffer */
        static uint32_t fake_fb[1024 * 768];
        fb_init_mock(fake_fb, 1024, 768);

        wm_init(1024, 768);
        for (int w = 0; w < 5; w++) {
            wm_create_window("BenchWindow", 50 + w * 40, 50 + w * 40, 300, 200, WINDOW_TYPE_CUSTOM);
        }

        const int FRAMES = 100;
        double t0 = get_time_sec();
        for (int f = 0; f < FRAMES; f++) {
            /* Simulated full desktop redraw */
            fb_fill_rect(0, 0, 1024, 768, 0x000F172A);
            window_t *z_list[WM_MAX_WINDOWS];
            int count = wm_get_windows_by_z_order(z_list, WM_MAX_WINDOWS);
            for (int i = 0; i < count; i++) {
                wm_render_window_frame(z_list[i]);
            }
        }
        double elapsed = get_time_sec() - t0;
        double ms_per_frame = (elapsed / FRAMES) * 1000.0;
        double fps_equivalent = FRAMES / elapsed;

        printf("[PERF] Framebuffer Composition & Window Rendering (1024x768x32bpp):\n");
        printf("       Frames Rendered: %d frames (5 layered windows)\n", FRAMES);
        printf("       Redraw Cost:     %.3f ms/frame\n", ms_per_frame);
        printf("       Throughput:      %.1f FPS (Software Blit)\n\n", fps_equivalent);
    }

    /* 4. GUI Event Queue Dispatch Throughput */
    {
        event_queue_init();
        const int EV_COUNT = 50000;
        gui_event_t ev = { .type = EVENT_MOUSE_MOVE, .x = 100, .y = 200 };
        gui_event_t out;

        double t0 = get_time_sec();
        for (int i = 0; i < EV_COUNT; i++) {
            event_queue_push(&ev);
            event_queue_pop(&out);
        }
        double elapsed = get_time_sec() - t0;
        printf("[PERF] GUI Event Queue (Lockless Ring Buffer):\n");
        printf("       Processed:   %d push+pop pairs\n", EV_COUNT);
        printf("       Throughput:  %.2f events/sec\n", (EV_COUNT * 2.0) / elapsed);
        printf("       Avg Latency: %.4f us/event\n\n", (elapsed / (EV_COUNT * 2.0)) * 1e6);
    }

    printf("==================================================\n");
    printf("   HOST PERFORMANCE BENCHMARKS COMPLETE!          \n");
    printf("==================================================\n");
    return 0;
}
