#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stddef.h>
#include "wm.h"
#include "event.h"

#define MAX_REGISTERED_APPS 16

#define APP_OK                    0
#define APP_ERR_INVALID_PARAM   -1
#define APP_ERR_DUPLICATE_ID    -2
#define APP_ERR_REGISTRY_FULL   -3
#define APP_ERR_NOT_FOUND       -4
#define APP_ERR_ALREADY_RUNNING -5
#define APP_ERR_WM_FAILURE      -6

typedef enum {
    APP_STATE_UNREGISTERED = 0,
    APP_STATE_REGISTERED   = 1,
    APP_STATE_RUNNING      = 2,
    APP_STATE_MINIMIZED    = 3,
    APP_STATE_STOPPED      = 4
} app_lifecycle_state_t;

typedef enum {
    APP_ID_TERMINAL = 1,
    APP_ID_SYSMON   = 2,
    APP_ID_FILES    = 3,
    APP_ID_PROC     = 4,
    APP_ID_AI       = 5,
    APP_ID_SETTINGS = 6,
    APP_ID_CUSTOM   = 7
} app_id_t;

typedef struct {
    uint32_t app_id;
    char name[64];
    char category[24];
    char icon_label[16];
    int32_t default_x;
    int32_t default_y;
    uint32_t default_w;
    uint32_t default_h;
    window_type_t win_type;

    int (*init)(void *app_ctx);
    void (*render)(void *app_ctx, window_t *win, int is_focused);
    void (*handle_key)(void *app_ctx, window_t *win, char c);
    void (*handle_click)(void *app_ctx, window_t *win, int32_t rx, int32_t ry, uint8_t buttons);
    void (*shutdown)(void *app_ctx);
} app_desc_t;

typedef struct {
    app_desc_t desc;
    app_lifecycle_state_t state;
    int window_id;
    void *private_ctx;
} app_instance_t;

void app_framework_init(void);
int app_register(const app_desc_t *desc);
int app_launch(uint32_t app_id);
int app_close(uint32_t app_id);
int app_minimize(uint32_t app_id);
void app_shutdown_all(void);

app_instance_t *app_get_by_id(uint32_t app_id);
app_instance_t *app_get_by_window_id(int window_id);
uint32_t app_get_registered_count(void);
uint32_t app_get_running_count(void);

int test_app_framework_invariants(void);

#endif /* APP_H */
