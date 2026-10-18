#include "app.h"
#include "wm.h"

static app_instance_t g_apps[MAX_REGISTERED_APPS];
static uint32_t g_registered_count = 0;

static void wm_bridge_render(window_t *win, int is_focused)
{
    if (!win || !win->user_data) return;
    app_instance_t *app = (app_instance_t *)win->user_data;
    if (app->desc.render) {
        app->desc.render(app->private_ctx, win, is_focused);
    }
}

static void wm_bridge_key(window_t *win, char c)
{
    if (!win || !win->user_data) return;
    app_instance_t *app = (app_instance_t *)win->user_data;
    if (app->desc.handle_key) {
        app->desc.handle_key(app->private_ctx, win, c);
    }
}

static void wm_bridge_click(window_t *win, int32_t rx, int32_t ry, uint8_t buttons)
{
    if (!win || !win->user_data) return;
    app_instance_t *app = (app_instance_t *)win->user_data;
    if (app->desc.handle_click) {
        app->desc.handle_click(app->private_ctx, win, rx, ry, buttons);
    }
}

void app_framework_init(void)
{
    g_registered_count = 0;
    for (uint32_t i = 0; i < MAX_REGISTERED_APPS; i++) {
        g_apps[i].state = APP_STATE_UNREGISTERED;
        g_apps[i].window_id = -1;
        g_apps[i].private_ctx = NULL;
    }
}

int app_register(const app_desc_t *desc)
{
    if (!desc || desc->app_id == 0) return APP_ERR_INVALID_PARAM;

    /* Check for duplicate ID */
    for (uint32_t i = 0; i < g_registered_count; i++) {
        if (g_apps[i].state != APP_STATE_UNREGISTERED && g_apps[i].desc.app_id == desc->app_id) {
            return APP_ERR_DUPLICATE_ID;
        }
    }

    if (g_registered_count >= MAX_REGISTERED_APPS) {
        return APP_ERR_REGISTRY_FULL;
    }

    uint32_t idx = g_registered_count;
    g_apps[idx].desc = *desc;
    g_apps[idx].state = APP_STATE_REGISTERED;
    g_apps[idx].window_id = -1;
    g_apps[idx].private_ctx = NULL;
    g_registered_count++;

    if (desc->init) {
        desc->init(NULL);
    }

    return APP_OK;
}

app_instance_t *app_get_by_id(uint32_t app_id)
{
    for (uint32_t i = 0; i < g_registered_count; i++) {
        if (g_apps[i].state != APP_STATE_UNREGISTERED && g_apps[i].desc.app_id == app_id) {
            return &g_apps[i];
        }
    }
    return NULL;
}

app_instance_t *app_get_by_window_id(int window_id)
{
    if (window_id <= 0) return NULL;
    for (uint32_t i = 0; i < g_registered_count; i++) {
        if (g_apps[i].state != APP_STATE_UNREGISTERED && g_apps[i].window_id == window_id) {
            return &g_apps[i];
        }
    }
    return NULL;
}

int app_launch(uint32_t app_id)
{
    app_instance_t *app = app_get_by_id(app_id);
    if (!app) return APP_ERR_NOT_FOUND;

    /* If already running, bring window to front */
    if (app->state == APP_STATE_RUNNING || app->state == APP_STATE_MINIMIZED) {
        window_t *w = wm_get_by_id(app->window_id);
        if (w) {
            if (w->state == WINDOW_STATE_MINIMIZED) {
                wm_restore_window(w->id);
            }
            wm_focus_window(w->id);
            app->state = APP_STATE_RUNNING;
            return APP_OK;
        }
    }

    /* Create window via Window Manager */
    window_t *win = wm_create_window(
        app->desc.name,
        app->desc.default_x,
        app->desc.default_y,
        app->desc.default_w,
        app->desc.default_h,
        app->desc.win_type
    );

    if (!win) return APP_ERR_WM_FAILURE;

    win->render_content = wm_bridge_render;
    win->handle_key = wm_bridge_key;
    win->handle_click = wm_bridge_click;
    win->user_data = app;

    app->window_id = win->id;
    app->state = APP_STATE_RUNNING;

    return APP_OK;
}

int app_close(uint32_t app_id)
{
    app_instance_t *app = app_get_by_id(app_id);
    if (!app) return APP_ERR_NOT_FOUND;

    if (app->state != APP_STATE_RUNNING && app->state != APP_STATE_MINIMIZED) {
        return APP_OK;
    }

    if (app->desc.shutdown) {
        app->desc.shutdown(app->private_ctx);
    }

    if (app->window_id > 0) {
        wm_destroy_window(app->window_id);
        app->window_id = -1;
    }

    app->state = APP_STATE_STOPPED;
    return APP_OK;
}

int app_minimize(uint32_t app_id)
{
    app_instance_t *app = app_get_by_id(app_id);
    if (!app || app->window_id <= 0) return APP_ERR_NOT_FOUND;

    wm_minimize_window(app->window_id);
    app->state = APP_STATE_MINIMIZED;
    return APP_OK;
}

void app_shutdown_all(void)
{
    for (uint32_t i = 0; i < g_registered_count; i++) {
        if (g_apps[i].state == APP_STATE_RUNNING || g_apps[i].state == APP_STATE_MINIMIZED) {
            app_close(g_apps[i].desc.app_id);
        }
    }
}

uint32_t app_get_registered_count(void)
{
    return g_registered_count;
}

uint32_t app_get_running_count(void)
{
    uint32_t running = 0;
    for (uint32_t i = 0; i < g_registered_count; i++) {
        if (g_apps[i].state == APP_STATE_RUNNING || g_apps[i].state == APP_STATE_MINIMIZED) {
            running++;
        }
    }
    return running;
}

static int dummy_init_called = 0;
static int dummy_shutdown_called = 0;
static char dummy_last_key = 0;

static int dummy_init(void *ctx) { (void)ctx; dummy_init_called = 1; return 0; }
static void dummy_render(void *ctx, window_t *win, int is_focused) { (void)ctx; (void)win; (void)is_focused; }
static void dummy_key(void *ctx, window_t *win, char c) { (void)ctx; (void)win; dummy_last_key = c; }
static void dummy_shutdown(void *ctx) { (void)ctx; dummy_shutdown_called = 1; }

int test_app_framework_invariants(void)
{
    app_framework_init();
    wm_init(1024, 768);

    if (app_get_registered_count() != 0) return -1;
    if (app_get_running_count() != 0) return -2;

    /* 1. Register App 1 */
    app_desc_t desc1 = {
        .app_id = 991,
        .name = "Test App 1",
        .category = "Test",
        .icon_label = "[T1]",
        .default_x = 50,
        .default_y = 50,
        .default_w = 300,
        .default_h = 200,
        .win_type = WINDOW_TYPE_CUSTOM,
        .init = dummy_init,
        .render = dummy_render,
        .handle_key = dummy_key,
        .shutdown = dummy_shutdown
    };

    if (app_register(&desc1) != APP_OK) return -3;
    if (app_get_registered_count() != 1) return -4;
    if (!dummy_init_called) return -5;

    /* 2. Reject duplicate ID */
    if (app_register(&desc1) != APP_ERR_DUPLICATE_ID) return -6;

    /* 3. Launch App */
    if (app_launch(991) != APP_OK) return -7;
    if (app_get_running_count() != 1) return -8;

    app_instance_t *inst = app_get_by_id(991);
    if (!inst || inst->state != APP_STATE_RUNNING || inst->window_id <= 0) return -9;

    /* 4. Event bridge invocation */
    window_t *w = wm_get_by_id(inst->window_id);
    if (!w || !w->handle_key) return -10;
    w->handle_key(w, 'K');
    if (dummy_last_key != 'K') return -11;

    /* 5. Minimize and Restore */
    if (app_minimize(991) != APP_OK) return -12;
    if (inst->state != APP_STATE_MINIMIZED) return -13;

    if (app_launch(991) != APP_OK) return -14;
    if (inst->state != APP_STATE_RUNNING) return -15;

    /* 6. Close App */
    if (app_close(991) != APP_OK) return -16;
    if (app_get_running_count() != 0) return -17;
    if (!dummy_shutdown_called) return -18;
    if (inst->state != APP_STATE_STOPPED || inst->window_id != -1) return -19;

    return 0;
}

