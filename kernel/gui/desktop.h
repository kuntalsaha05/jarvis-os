#ifndef DESKTOP_H
#define DESKTOP_H

#include <stdint.h>
#include <stddef.h>
#include "wm.h"
#include "event.h"

typedef struct {
    int is_running;
    uint32_t bg_color;
    uint32_t tick_counter;

    /* Terminal Application State */
    char term_history[24][80];
    uint32_t term_line_count;
    char term_input[80];
    uint32_t term_input_len;

    /* AI Application State */
    char ai_history[16][80];
    uint32_t ai_line_count;
    char ai_input[80];
    uint32_t ai_input_len;
    char ai_backend_model[32];

    /* File Browser State */
    char selected_file[32];
    char file_preview[256];
    uint32_t file_preview_len;

    /* Window IDs for the 5 standard apps */
    int win_term_id;
    int win_sysmon_id;
    int win_files_id;
    int win_proc_id;
    int win_ai_id;
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

const desktop_env_t *desktop_get_env(void);

int test_desktop_invariants(void);

#endif /* DESKTOP_H */
