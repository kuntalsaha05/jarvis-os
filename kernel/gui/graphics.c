#include "graphics.h"

void gfx_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg)
{
    if ((unsigned char)c < 32 || (unsigned char)c > 127) {
        c = '?';
    }

    uint32_t glyph_idx = (uint32_t)((unsigned char)c - 32);

    for (uint32_t row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = font_8x16[glyph_idx][row];
        for (uint32_t col = 0; col < FONT_WIDTH; col++) {
            if (bits & (0x80 >> col)) {
                fb_put_pixel(x + col, y + row, fg);
            } else if (bg != 0xFF123456) {
                fb_put_pixel(x + col, y + row, bg);
            }
        }
    }
}

void gfx_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg)
{
    if (!str) return;
    uint32_t cx = x;
    uint32_t cy = y;

    while (*str) {
        if (*str == '\n') {
            cx = x;
            cy += FONT_HEIGHT;
        } else if (*str == '\r') {
            cx = x;
        } else if (*str == '\t') {
            cx += FONT_WIDTH * 4;
        } else {
            gfx_draw_char(cx, cy, *str, fg, bg);
            cx += FONT_WIDTH;
        }
        str++;
    }
}

void gfx_draw_string_len(uint32_t x, uint32_t y, const char *str, uint32_t len, uint32_t fg, uint32_t bg)
{
    if (!str) return;
    uint32_t cx = x;
    uint32_t cy = y;

    for (uint32_t i = 0; i < len && str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            cx = x;
            cy += FONT_HEIGHT;
        } else {
            gfx_draw_char(cx, cy, str[i], fg, bg);
            cx += FONT_WIDTH;
        }
    }
}

void gfx_draw_number(uint32_t x, uint32_t y, uint64_t val, uint32_t fg, uint32_t bg)
{
    char buf[32];
    int idx = 0;

    if (val == 0) {
        buf[idx++] = '0';
    } else {
        char temp[32];
        int t = 0;
        while (val > 0) {
            temp[t++] = (char)('0' + (val % 10));
            val /= 10;
        }
        while (t > 0) {
            buf[idx++] = temp[--t];
        }
    }
    buf[idx] = '\0';
    gfx_draw_string(x, y, buf, fg, bg);
}

void gfx_draw_hex(uint32_t x, uint32_t y, uint64_t val, uint32_t fg, uint32_t bg)
{
    char buf[20];
    const char hex_chars[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        buf[2 + i] = hex_chars[(val >> (28 - i * 4)) & 0xF];
    }
    buf[10] = '\0';
    gfx_draw_string(x, y, buf, fg, bg);
}

void gfx_draw_button(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const char *label, int is_active)
{
    uint32_t bg = is_active ? FB_COLOR_CYAN_DARK : FB_COLOR_DARK_GRAY;
    uint32_t border = is_active ? FB_COLOR_CYAN_ACCENT : FB_COLOR_BORDER;
    uint32_t text_fg = is_active ? FB_COLOR_WHITE : FB_COLOR_GRAY;

    fb_fill_rect(x, y, w, h, bg);
    fb_draw_rect_outline(x, y, w, h, border, 1);

    if (label) {
        uint32_t len = 0;
        while (label[len]) len++;
        uint32_t text_w = len * FONT_WIDTH;
        uint32_t text_x = (w > text_w) ? x + (w - text_w) / 2 : x + 2;
        uint32_t text_y = (h > FONT_HEIGHT) ? y + (h - FONT_HEIGHT) / 2 : y + 2;
        gfx_draw_string(text_x, text_y, label, text_fg, bg);
    }
}

void gfx_draw_window_frame(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const char *title, int is_focused)
{
    uint32_t title_bg = is_focused ? FB_COLOR_CYAN_DARK : FB_COLOR_DARK_GRAY;
    uint32_t border = is_focused ? FB_COLOR_CYAN_ACCENT : FB_COLOR_BORDER;
    uint32_t title_fg = is_focused ? FB_COLOR_WHITE : FB_COLOR_GRAY;

    /* Window Background */
    fb_fill_rect(x, y, w, h, FB_COLOR_PANEL_BG);

    /* Title Bar (height: 24px) */
    fb_fill_rect(x, y, w, 24, title_bg);

    /* Title text */
    if (title) {
        gfx_draw_string(x + 10, y + 4, title, title_fg, title_bg);
    }

    /* Window control hint on right */
    gfx_draw_string(x + w - 24, y + 4, "[_]", FB_COLOR_GRAY, title_bg);

    /* Outer Border */
    fb_draw_rect_outline(x, y, w, h, border, 1);
}

void gfx_draw_progress_bar(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t current, uint32_t max, uint32_t color)
{
    fb_fill_rect(x, y, w, h, FB_COLOR_DARK_GRAY);
    fb_draw_rect_outline(x, y, w, h, FB_COLOR_BORDER, 1);

    if (max > 0 && current > 0) {
        uint32_t fill_w = (current * (w - 2)) / max;
        if (fill_w > w - 2) fill_w = w - 2;
        if (fill_w > 0) {
            fb_fill_rect(x + 1, y + 1, fill_w, h - 2, color);
        }
    }
}

