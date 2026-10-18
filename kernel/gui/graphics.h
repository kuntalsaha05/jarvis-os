#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stddef.h>
#include "../drivers/framebuffer.h"
#include "font.h"

void gfx_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void gfx_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg);
void gfx_draw_string_len(uint32_t x, uint32_t y, const char *str, uint32_t len, uint32_t fg, uint32_t bg);
void gfx_draw_number(uint32_t x, uint32_t y, uint64_t val, uint32_t fg, uint32_t bg);
void gfx_draw_hex(uint32_t x, uint32_t y, uint64_t val, uint32_t fg, uint32_t bg);

void gfx_draw_button(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const char *label, int is_active);
void gfx_draw_window_frame(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const char *title, int is_focused);
void gfx_draw_progress_bar(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t current, uint32_t max, uint32_t color);

#endif /* GRAPHICS_H */

