#ifndef WM_H
#define WM_H

#include <stdint.h>
#include <stddef.h>

#define WM_MAX_WINDOWS 8
#define WM_TITLE_BAR_HEIGHT 24
#define WM_TOP_BAR_HEIGHT 24
#define WM_TASKBAR_HEIGHT 32
#define WM_MIN_WIDTH 180
#define WM_MIN_HEIGHT 100

typedef enum {
    WINDOW_TYPE_TERMINAL = 0,
    WINDOW_TYPE_SYSMON   = 1,
    WINDOW_TYPE_FILES    = 2,
    WINDOW_TYPE_PROC     = 3,
    WINDOW_TYPE_AI_CHAT  = 4,
    WINDOW_TYPE_SETTINGS = 5,
    WINDOW_TYPE_CUSTOM   = 6
} window_type_t;

typedef enum {
    WINDOW_STATE_NORMAL    = 0,
    WINDOW_STATE_MINIMIZED = 1,
    WINDOW_STATE_MAXIMIZED = 2
} window_state_t;

typedef enum {
    WM_HIT_NONE = 0,
    WM_HIT_CLIENT,
    WM_HIT_TITLEBAR,
    WM_HIT_BTN_MINIMIZE,
    WM_HIT_BTN_MAXIMIZE,
    WM_HIT_BTN_CLOSE
} wm_hit_test_t;

typedef struct window {
    int id;
    int is_active_slot;
    window_type_t type;
    char title[48];
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    int32_t orig_x;
    int32_t orig_y;
    uint32_t orig_w;
    uint32_t orig_h;
    window_state_t state;
    int is_visible;
    int is_focused;
    int z_order;
    uint32_t bg_color;
    uint32_t border_color;
    void (*render_content)(struct window *win, int is_focused);
    void (*handle_key)(struct window *win, char c);
    void (*handle_click)(struct window *win, int32_t rel_x, int32_t rel_y, uint8_t buttons);
    void *user_data;
} window_t;

typedef struct {
    uint32_t screen_width;
    uint32_t screen_height;
    window_t windows[WM_MAX_WINDOWS];
    uint32_t window_count;
    int focused_window_id;
    int dragging_window_id;
    int32_t drag_offset_x;
    int32_t drag_offset_y;
} wm_manager_t;

void wm_init(uint32_t screen_w, uint32_t screen_h);
window_t *wm_create_window(const char *title, int32_t x, int32_t y, uint32_t w, uint32_t h, window_type_t type);
int wm_destroy_window(int id);
int wm_focus_window(int id);
int wm_bring_to_front(int id);
int wm_minimize_window(int id);
int wm_maximize_window(int id);
int wm_restore_window(int id);
int wm_drag_window(int id, int32_t dx, int32_t dy);
int wm_cycle_focus(void);

window_t *wm_get_by_id(int id);
window_t *wm_get_focused(void);
window_t *wm_get_window_at(int32_t x, int32_t y);
wm_hit_test_t wm_hit_test(const window_t *win, int32_t screen_x, int32_t screen_y);
int wm_get_windows_by_z_order(window_t **out_list, int max_count);

void wm_render_window_frame(const window_t *win);

int test_wm_invariants(void);

#endif /* WM_H */

