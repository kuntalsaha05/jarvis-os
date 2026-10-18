#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

typedef struct {
    int32_t x;
    int32_t y;
    uint8_t buttons;
    int is_present;
    uint32_t packet_count;
} mouse_state_t;

void mouse_init(void);
void mouse_handler(void);
const mouse_state_t *mouse_get_state(void);
int mouse_is_available(void);
void mouse_draw_cursor(uint32_t color);

int test_mouse_invariants(void);

#endif /* MOUSE_H */

