#ifndef DESKTOP_H
#define DESKTOP_H

#include <stdint.h>
#include <stddef.h>
#include "wm.h"
#include "event.h"
#include "app.h"
#include "config.h"
#include "notification.h"

typedef struct {
    int is_running;
    uint32_t bg_color;
    uint32_t tick_counter;
    desktop_config_t config;

    /* Terminal Application State */
    char term_history[24][80];
    uint32_t term_line_count;
    char term_input[80];
    uint32_t term_input_len;

    /* AI Application State */
    char ai_history[24][80];
    uint32_t ai_line_count;
    char ai_input[80];
    uint32_t ai_input_len;
    char ai_backend_model[32];
    char ai_backend_status[24];
    char ai_status_msg[64];
    int ai_is_streaming;
    int ai_is_connected;
    uint32_t ai_requests_count;

    /* File Browser State */
    char selected_file[32];
    char file_preview[256];
    uint32_t file_preview_len;

    /* Settings Application State */
    uint32_t settings_active_tab;
    char settings_status_msg[64];

    /* Window IDs for standard apps */
    int win_term_id;
    int win_sysmon_id;
    int win_files_id;
    int win_proc_id;
    int win_ai_id;
    int win_settings_id;
} desktop_env_t;

void desktop_init(void);
void desktop_render(void);
void desktop_run(void);
void desktop_handle_key(char c);

void desktop_launch_terminal(void);
void desktop_launch_sysmon(void);
void desktop_launch_files(void);
void desktop_launch_proc(void);
void desktop_launch_ai(void);
void desktop_launch_settings(void);

void desktop_cycle_theme(void);
void desktop_settings_set_tab(uint32_t tab);
void desktop_settings_cycle_accent(void);
void desktop_settings_cycle_mouse_sensitivity(void);
void desktop_settings_cycle_ai_backend(void);
int desktop_save_settings(void);
void desktop_reset_settings(void);

void desktop_ai_clear_history(void);
void desktop_ai_cancel_generation(void);
void desktop_ai_submit_query(const char *query);
void desktop_ai_set_backend_status(const char *status, const char *model, int is_connected);

const desktop_env_t *desktop_get_env(void);

int test_desktop_invariants(void);

#endif /* DESKTOP_H */
