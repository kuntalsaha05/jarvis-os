#include "event.h"

static gui_event_queue_t g_event_queue;

void event_queue_init(void)
{
    g_event_queue.head = 0;
    g_event_queue.tail = 0;
    g_event_queue.count = 0;
    g_event_queue.dropped_count = 0;
}

int event_queue_push(const gui_event_t *ev)
{
    if (!ev) return -1;

    if (g_event_queue.count >= GUI_EVENT_QUEUE_SIZE) {
        g_event_queue.dropped_count++;
        return -2; /* Overflow: rejected safely without memory corruption */
    }

    g_event_queue.events[g_event_queue.head] = *ev;
    g_event_queue.head = (g_event_queue.head + 1) % GUI_EVENT_QUEUE_SIZE;
    __sync_fetch_and_add(&g_event_queue.count, 1);
    return 0;
}

int event_queue_pop(gui_event_t *ev)
{
    if (!ev) return -1;
    if (g_event_queue.count == 0) return -2; /* Empty */

    *ev = g_event_queue.events[g_event_queue.tail];
    g_event_queue.tail = (g_event_queue.tail + 1) % GUI_EVENT_QUEUE_SIZE;
    __sync_fetch_and_sub(&g_event_queue.count, 1);
    return 0;
}

uint32_t event_queue_count(void)
{
    return g_event_queue.count;
}

uint32_t event_queue_dropped(void)
{
    return g_event_queue.dropped_count;
}

int test_event_queue_invariants(void)
{
    event_queue_init();
    if (event_queue_count() != 0) return -1;

    /* Underflow check */
    gui_event_t ev;
    if (event_queue_pop(&ev) != -2) return -2;

    /* Push and pop FIFO */
    gui_event_t e1 = { .type = EVENT_KEY_PRESS, .key = 'A' };
    gui_event_t e2 = { .type = EVENT_MOUSE_DOWN, .x = 100, .y = 200, .buttons = 1 };

    if (event_queue_push(&e1) != 0) return -3;
    if (event_queue_push(&e2) != 0) return -4;
    if (event_queue_count() != 2) return -5;

    gui_event_t out;
    if (event_queue_pop(&out) != 0 || out.type != EVENT_KEY_PRESS || out.key != 'A') return -6;
    if (event_queue_pop(&out) != 0 || out.type != EVENT_MOUSE_DOWN || out.x != 100 || out.y != 200) return -7;
    if (event_queue_count() != 0) return -8;

    /* Overflow check */
    for (int i = 0; i < GUI_EVENT_QUEUE_SIZE; i++) {
        gui_event_t item = { .type = EVENT_MOUSE_MOVE, .x = i };
        if (event_queue_push(&item) != 0) return -9;
    }

    if (event_queue_count() != GUI_EVENT_QUEUE_SIZE) return -10;

    /* Next push must fail gracefully with -2 and increment dropped_count */
    gui_event_t overflow_item = { .type = EVENT_KEY_PRESS, .key = 'Z' };
    if (event_queue_push(&overflow_item) != -2) return -11;
    if (event_queue_dropped() != 1) return -12;

    /* Pop all and verify no corruption */
    for (int i = 0; i < GUI_EVENT_QUEUE_SIZE; i++) {
        if (event_queue_pop(&out) != 0 || out.x != i) return -13;
    }

    if (event_queue_count() != 0) return -14;

    return 0;
}

