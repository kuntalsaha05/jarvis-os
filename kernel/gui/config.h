#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stddef.h>

#define CONFIG_MAGIC       0x4A434647  /* "JCFG" */
#define CONFIG_VERSION     1
#define CONFIG_FILE_PATH   "desktop.cfg"

typedef enum {
    THEME_JARVIS_DARK   = 0,
    THEME_CYBERPUNK     = 1,
    THEME_CLASSIC_SLATE = 2,
    THEME_COUNT         = 3
} desktop_theme_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t screen_width;
    uint32_t screen_height;
    uint32_t bpp;
    uint32_t color_desktop_bg;
    uint32_t color_panel_bg;
    uint32_t color_accent;
    uint32_t theme_id;
    uint32_t taskbar_visible;
    uint32_t taskbar_position;     /* 0 = bottom, 1 = top */
    uint32_t startup_apps_mask;    /* Bitmask: bit 0=Terminal, 1=SysMon, 2=Files, 3=Proc, 4=AI */
    uint32_t checksum;
} desktop_config_t;

void config_init_defaults(desktop_config_t *cfg);
int config_validate(const desktop_config_t *cfg);
void config_reset_defaults(desktop_config_t *cfg);

int config_save_to_fs(const desktop_config_t *cfg, const char *path);
int config_load_from_fs(desktop_config_t *cfg, const char *path);

const desktop_config_t *config_get_active(void);
int config_apply(const desktop_config_t *cfg);

int test_config_invariants(void);

#endif /* CONFIG_H */

