#include "notification.h"

static notif_queue_t g_queue;

static void safe_strcpy(char *dest, const char *src, size_t max_len)
{
    if (!dest || max_len == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    size_t i = 0;
    while (i + 1 < max_len && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void notif_init(void)
{
    for (int i = 0; i < NOTIF_MAX_ENTRIES; i++) {
        g_queue.items[i].id = 0;
        g_queue.items[i].level = NOTIF_LEVEL_INFO;
        g_queue.items[i].title[0] = '\0';
        g_queue.items[i].message[0] = '\0';
        g_queue.items[i].timestamp_ticks = 0;
        g_queue.items[i].duration_ticks = 0;
        g_queue.items[i].is_active = 0;
        g_queue.items[i].is_read = 0;
    }
    g_queue.next_id = 1;
    g_queue.active_count = 0;
    g_queue.total_posted = 0;
    g_queue.total_expired = 0;
    g_queue.total_dismissed = 0;
    g_queue.total_dropped_overflow = 0;
}

int notif_post(notif_level_t level, const char *title, const char *message, uint32_t duration_ticks)
{
    if (level > NOTIF_LEVEL_ERROR) return NOTIF_ERR_INVALID_PARAM;
    if (!title || !message) return NOTIF_ERR_INVALID_PARAM;

    int slot = -1;

    /* 1. Look for an inactive slot */
    for (int i = 0; i < NOTIF_MAX_ENTRIES; i++) {
        if (!g_queue.items[i].is_active) {
            slot = i;
            break;
        }
    }

    /* 2. If queue is full, evict the oldest active notification (lowest ID) */
    if (slot < 0) {
        uint32_t lowest_id = 0xFFFFFFFF;
        int oldest_slot = 0;
        for (int i = 0; i < NOTIF_MAX_ENTRIES; i++) {
            if (g_queue.items[i].is_active && g_queue.items[i].id < lowest_id) {
                lowest_id = g_queue.items[i].id;
                oldest_slot = i;
            }
        }
        g_queue.items[oldest_slot].is_active = 0;
        g_queue.active_count--;
        g_queue.total_dropped_overflow++;
        slot = oldest_slot;
    }

    /* 3. Assign notification */
    uint32_t id = g_queue.next_id++;
    if (g_queue.next_id == 0) g_queue.next_id = 1;

    notification_t *item = &g_queue.items[slot];
    item->id = id;
    item->level = level;
    safe_strcpy(item->title, title, NOTIF_TITLE_MAX);
    safe_strcpy(item->message, message, NOTIF_MSG_MAX);
    item->duration_ticks = duration_ticks;
    item->is_active = 1;
    item->is_read = 0;

    /* In a real kernel, timestamp is set via timer_ticks; caller or update handles it */
    extern volatile uint64_t timer_ticks;
    item->timestamp_ticks = timer_ticks;

    g_queue.active_count++;
    g_queue.total_posted++;

    return (int)id;
}

int notif_dismiss(uint32_t id)
{
    if (id == 0) return NOTIF_ERR_INVALID_PARAM;

    for (int i = 0; i < NOTIF_MAX_ENTRIES; i++) {
        if (g_queue.items[i].is_active && g_queue.items[i].id == id) {
            g_queue.items[i].is_active = 0;
            if (g_queue.active_count > 0) g_queue.active_count--;
            g_queue.total_dismissed++;
            return NOTIF_OK;
        }
    }
    return NOTIF_ERR_NOT_FOUND;
}

void notif_dismiss_all(void)
{
    for (int i = 0; i < NOTIF_MAX_ENTRIES; i++) {
        if (g_queue.items[i].is_active) {
            g_queue.items[i].is_active = 0;
            g_queue.total_dismissed++;
        }
    }
    g_queue.active_count = 0;
}

void notif_update(uint64_t current_ticks)
{
    for (int i = 0; i < NOTIF_MAX_ENTRIES; i++) {
        if (g_queue.items[i].is_active && g_queue.items[i].duration_ticks > 0) {
            if (current_ticks >= g_queue.items[i].timestamp_ticks &&
                (current_ticks - g_queue.items[i].timestamp_ticks) >= g_queue.items[i].duration_ticks) {
                g_queue.items[i].is_active = 0;
                if (g_queue.active_count > 0) g_queue.active_count--;
                g_queue.total_expired++;
            }
        }
    }
}

const notif_queue_t *notif_get_queue(void)
{
    return &g_queue;
}

uint32_t notif_get_active_count(void)
{
    return g_queue.active_count;
}

const notification_t *notif_get_latest_active(void)
{
    int latest_idx = -1;
    uint32_t highest_id = 0;
    for (int i = 0; i < NOTIF_MAX_ENTRIES; i++) {
        if (g_queue.items[i].is_active && g_queue.items[i].id > highest_id) {
            highest_id = g_queue.items[i].id;
            latest_idx = i;
        }
    }
    return (latest_idx >= 0) ? &g_queue.items[latest_idx] : NULL;
}

int test_notification_invariants(void)
{
    notif_init();

    /* 1. Invalid parameter checks */
    if (notif_post((notif_level_t)99, "Title", "Message", 100) != NOTIF_ERR_INVALID_PARAM) return -1;
    if (notif_post(NOTIF_LEVEL_INFO, NULL, "Message", 100) != NOTIF_ERR_INVALID_PARAM) return -2;
    if (notif_post(NOTIF_LEVEL_INFO, "Title", NULL, 100) != NOTIF_ERR_INVALID_PARAM) return -3;
    if (notif_dismiss(0) != NOTIF_ERR_INVALID_PARAM) return -4;
    if (notif_dismiss(999) != NOTIF_ERR_NOT_FOUND) return -5;

    /* 2. Post notifications across levels */
    int id1 = notif_post(NOTIF_LEVEL_INFO, "System Boot", "Kernel initialized successfully", 500);
    int id2 = notif_post(NOTIF_LEVEL_WARN, "Battery Low", "Running on AC simulation", 500);
    int id3 = notif_post(NOTIF_LEVEL_ERROR, "Disk Full", "Cannot write dump", 500);

    if (id1 <= 0 || id2 <= 0 || id3 <= 0) return -6;
    if (notif_get_active_count() != 3) return -7;

    const notification_t *latest = notif_get_latest_active();
    if (!latest || latest->id != (uint32_t)id3 || latest->level != NOTIF_LEVEL_ERROR) return -8;

    /* 3. Dismiss specific notification */
    if (notif_dismiss(id2) != NOTIF_OK) return -9;
    if (notif_get_active_count() != 2) return -10;
    if (notif_dismiss(id2) != NOTIF_ERR_NOT_FOUND) return -11; /* already dismissed */

    /* 4. Queue capacity and overflow protection */
    notif_init();
    int ids[NOTIF_MAX_ENTRIES + 2];
    for (int i = 0; i < NOTIF_MAX_ENTRIES; i++) {
        ids[i] = notif_post(NOTIF_LEVEL_INFO, "Note", "Entry", 0);
        if (ids[i] <= 0) return -12;
    }
    if (notif_get_active_count() != NOTIF_MAX_ENTRIES) return -13;

    /* Post 9th and 10th item: should evict oldest items safely without queue corruption */
    int id_ovf1 = notif_post(NOTIF_LEVEL_WARN, "Overflow 1", "Evicts oldest", 0);
    int id_ovf2 = notif_post(NOTIF_LEVEL_ERROR, "Overflow 2", "Evicts second oldest", 0);
    if (id_ovf1 <= 0 || id_ovf2 <= 0) return -14;

    const notif_queue_t *q = notif_get_queue();
    if (q->active_count != NOTIF_MAX_ENTRIES) return -15;
    if (q->total_dropped_overflow != 2) return -16;
    if (q->total_posted != (NOTIF_MAX_ENTRIES + 2)) return -17;

    /* 5. Expiry simulation */
    notif_init();
    extern volatile uint64_t timer_ticks;
    uint64_t current = timer_ticks;
    int exp_id = notif_post(NOTIF_LEVEL_INFO, "Expiring", "Should expire after 50 ticks", 50);
    if (exp_id <= 0) return -18;

    notif_update(current + 20); /* not expired yet */
    if (notif_get_active_count() != 1) return -19;

    notif_update(current + 60); /* now expired */
    if (notif_get_active_count() != 0) return -20;
    if (notif_get_queue()->total_expired != 1) return -21;

    /* 6. Dismiss all */
    notif_post(NOTIF_LEVEL_INFO, "A", "M1", 0);
    notif_post(NOTIF_LEVEL_WARN, "B", "M2", 0);
    if (notif_get_active_count() != 2) return -22;
    notif_dismiss_all();
    if (notif_get_active_count() != 0) return -23;

    return 0;
}

