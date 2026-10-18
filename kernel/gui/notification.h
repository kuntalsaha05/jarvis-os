#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <stdint.h>
#include <stddef.h>

#define NOTIF_MAX_ENTRIES    8
#define NOTIF_TITLE_MAX     32
#define NOTIF_MSG_MAX       64

#define NOTIF_OK                  0
#define NOTIF_ERR_INVALID_PARAM  -1
#define NOTIF_ERR_NOT_FOUND      -2

typedef enum {
    NOTIF_LEVEL_INFO  = 0,
    NOTIF_LEVEL_WARN  = 1,
    NOTIF_LEVEL_ERROR = 2
} notif_level_t;

typedef struct {
    uint32_t id;
    notif_level_t level;
    char title[NOTIF_TITLE_MAX];
    char message[NOTIF_MSG_MAX];
    uint64_t timestamp_ticks;
    uint32_t duration_ticks; /* 0 = sticky until dismissed */
    uint8_t is_active;
    uint8_t is_read;
} notification_t;

typedef struct {
    notification_t items[NOTIF_MAX_ENTRIES];
    uint32_t next_id;
    uint32_t active_count;
    uint32_t total_posted;
    uint32_t total_expired;
    uint32_t total_dismissed;
    uint32_t total_dropped_overflow;
} notif_queue_t;

void notif_init(void);
int notif_post(notif_level_t level, const char *title, const char *message, uint32_t duration_ticks);
int notif_dismiss(uint32_t id);
void notif_dismiss_all(void);
void notif_update(uint64_t current_ticks);
const notif_queue_t *notif_get_queue(void);
uint32_t notif_get_active_count(void);
const notification_t *notif_get_latest_active(void);

int test_notification_invariants(void);

#endif /* NOTIFICATION_H */

