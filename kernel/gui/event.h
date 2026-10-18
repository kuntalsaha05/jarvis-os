#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>
#include <stddef.h>

#define GUI_EVENT_QUEUE_SIZE 64

typedef enum {
    EVENT_NONE = 0,
    EVENT_KEY_PRESS,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP,
    EVENT_WINDOW_CLOSE,
    EVENT_WINDOW_FOCUS,
    EVENT_APP_LAUNCH
} event_type_t;

typedef struct {
    event_type_t type;
    int32_t x;          /* Mouse X or screen X */
    int32_t y;          /* Mouse Y or screen Y */
    int32_t dx;         /* Mouse delta X */
    int32_t dy;         /* Mouse delta Y */
    uint8_t buttons;    /* Mouse button mask: bit 0 = Left, bit 1 = Right, bit 2 = Middle */
    char key;           /* Key ASCII character */
    int window_id;      /* Target window ID if applicable */
} gui_event_t;

typedef struct {
    gui_event_t events[GUI_EVENT_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
    volatile uint32_t dropped_count;
} gui_event_queue_t;

void event_queue_init(void);
int event_queue_push(const gui_event_t *ev);
int event_queue_pop(gui_event_t *ev);
uint32_t event_queue_count(void);
uint32_t event_queue_dropped(void);

int test_event_queue_invariants(void);

#endif /* EVENT_H */

