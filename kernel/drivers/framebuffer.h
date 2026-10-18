#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>

#define FB_COLOR_BLACK       0x00000000
#define FB_COLOR_DARK_SLATE  0x000D1117
#define FB_COLOR_PANEL_BG    0x00161B22
#define FB_COLOR_BORDER      0x0030363D
#define FB_COLOR_CYAN_ACCENT 0x0000E5FF
#define FB_COLOR_CYAN_DARK   0x00007A8A
#define FB_COLOR_WHITE       0x00FFFFFF
#define FB_COLOR_GRAY        0x008B949E
#define FB_COLOR_DARK_GRAY   0x0021262D
#define FB_COLOR_GREEN       0x002EA043
#define FB_COLOR_RED         0x00DA3633
#define FB_COLOR_YELLOW      0x00D29922
#define FB_COLOR_BLUE        0x001F6FEB

typedef struct {
    uint64_t phys_addr;
    uint32_t *buffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;     /* Bytes per scanline */
    uint32_t bpp;       /* Bits per pixel */
    uint8_t type;       /* 1 = RGB */
    int is_valid;
    char method[48];    /* Discovery mechanism */
} fb_info_t;

void fb_init(uint64_t magic, uint64_t mb_info_addr);
void fb_init_mock(uint32_t *buffer, uint32_t width, uint32_t height);
const fb_info_t *fb_get_info(void);
int fb_is_available(void);

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t fb_get_pixel(uint32_t x, uint32_t y);
void fb_clear(uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_rect_outline(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color, uint32_t thickness);
void fb_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);

int test_framebuffer_invariants(void);

#endif /* FRAMEBUFFER_H */

