#include "wm.h"
#include "graphics.h"
#include "drivers/framebuffer.h"

static wm_manager_t g_wm;

static void str_copy_n(char *dest, const char *src, uint32_t max_len)
{
    if (!dest) return;
    uint32_t i = 0;
    if (src) {
        while (src[i] && i + 1 < max_len) {
            dest[i] = src[i];
            i++;
        }
    }
    dest[i] = '\0';
}

void wm_init(uint32_t screen_w, uint32_t screen_h)
{
    g_wm.screen_width = screen_w ? screen_w : 1024;
    g_wm.screen_height = screen_h ? screen_h : 768;
    g_wm.window_count = 0;
    g_wm.focused_window_id = -1;
    g_wm.dragging_window_id = -1;
    g_wm.drag_offset_x = 0;
    g_wm.drag_offset_y = 0;

    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        g_wm.windows[i].id = i + 1;
        g_wm.windows[i].is_active_slot = 0;
        g_wm.windows[i].is_visible = 0;
        g_wm.windows[i].is_focused = 0;
        g_wm.windows[i].z_order = 0;
        g_wm.windows[i].render_content = NULL;
        g_wm.windows[i].handle_key = NULL;
        g_wm.windows[i].handle_click = NULL;
        g_wm.windows[i].user_data = NULL;
    }
}

window_t *wm_get_by_id(int id)
{
    if (id < 1 || id > WM_MAX_WINDOWS) return NULL;
    window_t *w = &g_wm.windows[id - 1];
    return w->is_active_slot ? w : NULL;
}

window_t *wm_get_focused(void)
{
    if (g_wm.focused_window_id == -1) return NULL;
    return wm_get_by_id(g_wm.focused_window_id);
}

int wm_bring_to_front(int id)
{
    window_t *target = wm_get_by_id(id);
    if (!target || !target->is_visible) return -1;

    int old_z = target->z_order;
    int max_z = (int)g_wm.window_count - 1;
    if (max_z < 0) max_z = 0;

    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_wm.windows[i].is_active_slot && g_wm.windows[i].is_visible) {
            if (g_wm.windows[i].z_order > old_z) {
                g_wm.windows[i].z_order--;
            }
        }
    }

    target->z_order = max_z;
    return 0;
}

int wm_focus_window(int id)
{
    window_t *target = wm_get_by_id(id);
    if (!target) return -1;

    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_wm.windows[i].is_active_slot) {
            g_wm.windows[i].is_focused = (g_wm.windows[i].id == id) ? 1 : 0;
        }
    }

    g_wm.focused_window_id = id;
    if (target->state == WINDOW_STATE_MINIMIZED) {
        target->state = WINDOW_STATE_NORMAL;
        target->is_visible = 1;
    }

    wm_bring_to_front(id);
    return 0;
}

window_t *wm_create_window(const char *title, int32_t x, int32_t y, uint32_t w, uint32_t h, window_type_t type)
{
    int slot = -1;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (!g_wm.windows[i].is_active_slot) {
            slot = i;
            break;
        }
    }

    if (slot == -1) return NULL; /* Max windows reached */

    /* Bounds validation */
    if (w < WM_MIN_WIDTH) w = WM_MIN_WIDTH;
    if (h < WM_MIN_HEIGHT) h = WM_MIN_HEIGHT;
    if (w > g_wm.screen_width) w = g_wm.screen_width;
    if (h > g_wm.screen_height - WM_TOP_BAR_HEIGHT - WM_TASKBAR_HEIGHT) {
        h = g_wm.screen_height - WM_TOP_BAR_HEIGHT - WM_TASKBAR_HEIGHT;
    }

    if (y < (int32_t)WM_TOP_BAR_HEIGHT) y = (int32_t)WM_TOP_BAR_HEIGHT;
    if (y + (int32_t)h > (int32_t)(g_wm.screen_height - WM_TASKBAR_HEIGHT)) {
        y = (int32_t)(g_wm.screen_height - WM_TASKBAR_HEIGHT) - (int32_t)h;
    }

    if (x < 0) x = 0;
    if (x + (int32_t)w > (int32_t)g_wm.screen_width) {
        x = (int32_t)g_wm.screen_width - (int32_t)w;
    }

    window_t *win = &g_wm.windows[slot];
    win->id = slot + 1;
    win->is_active_slot = 1;
    win->type = type;
    str_copy_n(win->title, title ? title : "Window", sizeof(win->title));
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    win->orig_x = x;
    win->orig_y = y;
    win->orig_w = w;
    win->orig_h = h;
    win->state = WINDOW_STATE_NORMAL;
    win->is_visible = 1;
    win->is_focused = 1;
    win->z_order = (int)g_wm.window_count;
    win->bg_color = FB_COLOR_PANEL_BG;
    win->border_color = FB_COLOR_BORDER;
    win->render_content = NULL;
    win->handle_key = NULL;
    win->handle_click = NULL;
    win->user_data = NULL;

    g_wm.window_count++;
    wm_focus_window(win->id);
    return win;
}

int wm_destroy_window(int id)
{
    window_t *win = wm_get_by_id(id);
    if (!win) return -1;

    int old_z = win->z_order;
    win->is_active_slot = 0;
    win->is_visible = 0;
    win->is_focused = 0;
    g_wm.window_count--;

    /* Rebalance z-orders */
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_wm.windows[i].is_active_slot && g_wm.windows[i].z_order > old_z) {
            g_wm.windows[i].z_order--;
        }
    }

    /* Focus next visible window with highest z-order */
    int next_focus = -1;
    int highest_z = -1;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_wm.windows[i].is_active_slot && g_wm.windows[i].is_visible) {
            if (g_wm.windows[i].z_order > highest_z) {
                highest_z = g_wm.windows[i].z_order;
                next_focus = g_wm.windows[i].id;
            }
        }
    }

    g_wm.focused_window_id = next_focus;
    if (next_focus != -1) {
        wm_focus_window(next_focus);
    }

    return 0;
}

int wm_minimize_window(int id)
{
    window_t *win = wm_get_by_id(id);
    if (!win) return -1;

    win->state = WINDOW_STATE_MINIMIZED;
    win->is_visible = 0;
    win->is_focused = 0;

    /* Focus next visible window */
    int next_focus = -1;
    int highest_z = -1;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_wm.windows[i].is_active_slot && g_wm.windows[i].is_visible) {
            if (g_wm.windows[i].z_order > highest_z) {
                highest_z = g_wm.windows[i].z_order;
                next_focus = g_wm.windows[i].id;
            }
        }
    }

    g_wm.focused_window_id = next_focus;
    if (next_focus != -1) {
        wm_focus_window(next_focus);
    }

    return 0;
}

int wm_maximize_window(int id)
{
    window_t *win = wm_get_by_id(id);
    if (!win) return -1;

    if (win->state == WINDOW_STATE_MAXIMIZED) {
        /* Toggle back to normal */
        return wm_restore_window(id);
    }

    win->orig_x = win->x;
    win->orig_y = win->y;
    win->orig_w = win->width;
    win->orig_h = win->height;

    win->x = 0;
    win->y = (int32_t)WM_TOP_BAR_HEIGHT;
    win->width = g_wm.screen_width;
    win->height = g_wm.screen_height - WM_TOP_BAR_HEIGHT - WM_TASKBAR_HEIGHT;
    win->state = WINDOW_STATE_MAXIMIZED;
    win->is_visible = 1;

    wm_focus_window(id);
    return 0;
}

int wm_restore_window(int id)
{
    window_t *win = wm_get_by_id(id);
    if (!win) return -1;

    win->x = win->orig_x;
    win->y = win->orig_y;
    win->width = win->orig_w;
    win->height = win->orig_h;
    win->state = WINDOW_STATE_NORMAL;
    win->is_visible = 1;

    wm_focus_window(id);
    return 0;
}

int wm_drag_window(int id, int32_t dx, int32_t dy)
{
    window_t *win = wm_get_by_id(id);
    if (!win || win->state == WINDOW_STATE_MAXIMIZED) return -1;

    win->x += dx;
    win->y += dy;

    /* Screen boundary checks & clipping */
    if (win->y < (int32_t)WM_TOP_BAR_HEIGHT) {
        win->y = (int32_t)WM_TOP_BAR_HEIGHT;
    }
    if (win->y > (int32_t)(g_wm.screen_height - WM_TASKBAR_HEIGHT - 24)) {
        win->y = (int32_t)(g_wm.screen_height - WM_TASKBAR_HEIGHT - 24);
    }

    int32_t min_x = -((int32_t)win->width - 64);
    int32_t max_x = (int32_t)g_wm.screen_width - 64;
    if (win->x < min_x) win->x = min_x;
    if (win->x > max_x) win->x = max_x;

    return 0;
}

int wm_cycle_focus(void)
{
    if (g_wm.window_count == 0) return -1;

    int current_id = g_wm.focused_window_id;
    int next_id = -1;

    /* Cycle in slot index order */
    int start = (current_id == -1) ? 0 : current_id;
    for (int step = 1; step <= WM_MAX_WINDOWS; step++) {
        int idx = (start - 1 + step) % WM_MAX_WINDOWS;
        if (g_wm.windows[idx].is_active_slot && g_wm.windows[idx].is_visible) {
            next_id = g_wm.windows[idx].id;
            break;
        }
    }

    if (next_id != -1) {
        wm_focus_window(next_id);
        return next_id;
    }
    return -1;
}

wm_hit_test_t wm_hit_test(const window_t *win, int32_t screen_x, int32_t screen_y)
{
    if (!win || !win->is_visible) return WM_HIT_NONE;

    if (screen_x < win->x || screen_x >= win->x + (int32_t)win->width ||
        screen_y < win->y || screen_y >= win->y + (int32_t)win->height) {
        return WM_HIT_NONE;
    }

    /* Check title bar (top 24px) */
    if (screen_y < win->y + (int32_t)WM_TITLE_BAR_HEIGHT) {
        int32_t btn_close_x = win->x + (int32_t)win->width - 22;
        int32_t btn_max_x   = win->x + (int32_t)win->width - 42;
        int32_t btn_min_x   = win->x + (int32_t)win->width - 62;

        if (screen_x >= btn_close_x && screen_x <= btn_close_x + 16) {
            return WM_HIT_BTN_CLOSE;
        }
        if (screen_x >= btn_max_x && screen_x <= btn_max_x + 16) {
            return WM_HIT_BTN_MAXIMIZE;
        }
        if (screen_x >= btn_min_x && screen_x <= btn_min_x + 16) {
            return WM_HIT_BTN_MINIMIZE;
        }

        return WM_HIT_TITLEBAR;
    }

    return WM_HIT_CLIENT;
}

window_t *wm_get_window_at(int32_t x, int32_t y)
{
    window_t *top_win = NULL;
    int highest_z = -1;

    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_wm.windows[i].is_active_slot && g_wm.windows[i].is_visible) {
            if (wm_hit_test(&g_wm.windows[i], x, y) != WM_HIT_NONE) {
                if (g_wm.windows[i].z_order > highest_z) {
                    highest_z = g_wm.windows[i].z_order;
                    top_win = &g_wm.windows[i];
                }
            }
        }
    }

    return top_win;
}

int wm_get_windows_by_z_order(window_t **out_list, int max_count)
{
    if (!out_list || max_count <= 0) return 0;

    int count = 0;
    for (int z = 0; z < (int)g_wm.window_count && count < max_count; z++) {
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (g_wm.windows[i].is_active_slot && g_wm.windows[i].is_visible && g_wm.windows[i].z_order == z) {
                out_list[count++] = &g_wm.windows[i];
                break;
            }
        }
    }

    return count;
}

void wm_render_window_frame(const window_t *win)
{
    if (!win || !win->is_visible) return;

    uint32_t title_bg = win->is_focused ? FB_COLOR_CYAN_DARK : FB_COLOR_DARK_GRAY;
    uint32_t border_col = win->is_focused ? FB_COLOR_CYAN_ACCENT : FB_COLOR_BORDER;
    uint32_t title_fg = win->is_focused ? FB_COLOR_WHITE : FB_COLOR_GRAY;

    /* Window Background */
    fb_fill_rect((uint32_t)win->x, (uint32_t)win->y, win->width, win->height, win->bg_color);

    /* Title Bar */
    fb_fill_rect((uint32_t)win->x, (uint32_t)win->y, win->width, WM_TITLE_BAR_HEIGHT, title_bg);

    /* Title text with clipping length */
    uint32_t max_title_chars = (win->width > 120) ? (win->width - 100) / FONT_WIDTH : 4;
    gfx_draw_string_len((uint32_t)win->x + 10, (uint32_t)win->y + 4, win->title, max_title_chars, title_fg, title_bg);

    /* Window control buttons on title bar */
    int32_t btn_close_x = win->x + (int32_t)win->width - 22;
    int32_t btn_max_x   = win->x + (int32_t)win->width - 42;
    int32_t btn_min_x   = win->x + (int32_t)win->width - 62;
    int32_t btn_y       = win->y + 4;

    /* Minimize button '_' */
    fb_fill_rect((uint32_t)btn_min_x, (uint32_t)btn_y, 16, 16, FB_COLOR_DARK_GRAY);
    fb_draw_rect_outline((uint32_t)btn_min_x, (uint32_t)btn_y, 16, 16, FB_COLOR_GRAY, 1);
    gfx_draw_string((uint32_t)btn_min_x + 4, (uint32_t)btn_y, "_", FB_COLOR_WHITE, FB_COLOR_DARK_GRAY);

    /* Maximize button '[]' */
    fb_fill_rect((uint32_t)btn_max_x, (uint32_t)btn_y, 16, 16, FB_COLOR_DARK_GRAY);
    fb_draw_rect_outline((uint32_t)btn_max_x, (uint32_t)btn_y, 16, 16, FB_COLOR_GRAY, 1);
    gfx_draw_string((uint32_t)btn_max_x + 4, (uint32_t)btn_y, (win->state == WINDOW_STATE_MAXIMIZED) ? "=" : "^", FB_COLOR_WHITE, FB_COLOR_DARK_GRAY);

    /* Close button 'X' */
    fb_fill_rect((uint32_t)btn_close_x, (uint32_t)btn_y, 16, 16, 0xFF8A1515);
    fb_draw_rect_outline((uint32_t)btn_close_x, (uint32_t)btn_y, 16, 16, 0xFFE53935, 1);
    gfx_draw_string((uint32_t)btn_close_x + 4, (uint32_t)btn_y, "x", FB_COLOR_WHITE, 0xFF8A1515);

    /* Window Outer Border */
    fb_draw_rect_outline((uint32_t)win->x, (uint32_t)win->y, win->width, win->height, border_col, 1);

    /* Client Content */
    if (win->render_content) {
        win->render_content((window_t *)win, win->is_focused);
    }
}

int test_wm_invariants(void)
{
    wm_init(1024, 768);
    if (g_wm.window_count != 0) return -1;

    /* 1. Create window 1 */
    window_t *w1 = wm_create_window("Window 1", 50, 50, 300, 200, WINDOW_TYPE_TERMINAL);
    if (!w1 || w1->id != 1 || g_wm.window_count != 1) return -2;
    if (w1->z_order != 0 || !w1->is_focused) return -3;

    /* 2. Create window 2 */
    window_t *w2 = wm_create_window("Window 2", 100, 100, 400, 300, WINDOW_TYPE_SYSMON);
    if (!w2 || w2->id != 2 || g_wm.window_count != 2) return -4;
    if (w2->z_order != 1 || !w2->is_focused || w1->is_focused) return -5;

    /* 3. Bring window 1 to front */
    wm_focus_window(w1->id);
    if (!w1->is_focused || w2->is_focused) return -6;
    if (w1->z_order <= w2->z_order) return -7;

    /* 4. Hit test */
    if (wm_hit_test(w1, 60, 60) != WM_HIT_TITLEBAR) return -8;
    if (wm_hit_test(w1, 60, 80) != WM_HIT_CLIENT) return -9;
    if (wm_hit_test(w1, 10, 10) != WM_HIT_NONE) return -10;

    /* 5. Drag window and boundary clamp */
    wm_drag_window(w1->id, 50, 20);
    if (w1->x != 100 || w1->y != 70) return -11;

    /* Attempt to drag above top bar (y < 24) must clamp to 24 */
    wm_drag_window(w1->id, 0, -200);
    if (w1->y != (int32_t)WM_TOP_BAR_HEIGHT) return -12;

    /* 6. Maximize and Restore */
    wm_maximize_window(w1->id);
    if (w1->state != WINDOW_STATE_MAXIMIZED || w1->width != 1024) return -13;
    wm_restore_window(w1->id);
    if (w1->state != WINDOW_STATE_NORMAL || w1->width != 300) return -14;

    /* 7. Minimize */
    wm_minimize_window(w1->id);
    if (w1->is_visible != 0 || w1->state != WINDOW_STATE_MINIMIZED) return -15;

    /* 8. Destroy window 2 */
    wm_destroy_window(w2->id);
    if (g_wm.window_count != 1) return -16;

    /* Destroy window 1 */
    wm_destroy_window(w1->id);
    if (g_wm.window_count != 0) return -17;

    return 0;
}
