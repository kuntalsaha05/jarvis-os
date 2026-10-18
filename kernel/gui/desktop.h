#ifndef DESKTOP_H
#define DESKTOP_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    WIN_TERMINAL = 0,
    WIN_SYSMON   = 1,
    WIN_AI_CHAT  = 2,
    WIN_COUNT    = 3
} desktop_window_t;

typedef struct {
    desktop_window_t active_window;
    int is_running;
    uint32_t bg_color;
    char term_history[16][80];
    uint32_t term_line_count;
    char term_input[80];
    uint32_t term_input_len;
    char ai_history[12][80];
    uint32_t ai_line_count;
    char ai_input[80];
    uint32_t ai_input_len;
    int ai_waiting_reply;
} desktop_state_t;

void desktop_init(void);
void desktop_render(void);
void desktop_handle_key(char c);
void desktop_run(void);
const desktop_state_t *desktop_get_state(void);

int test_desktop_invariants(void);

#endif /* DESKTOP_H */

