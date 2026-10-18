#include "config.h"
#include "drivers/framebuffer.h"
#include "storage/fs.h"

static desktop_config_t g_active_config;

static uint32_t calculate_checksum(const desktop_config_t *cfg)
{
    if (!cfg) return 0;
    const uint32_t *p = (const uint32_t *)cfg;
    uint32_t words = (sizeof(desktop_config_t) - sizeof(uint32_t)) / sizeof(uint32_t);
    uint32_t sum = 0;
    for (uint32_t i = 0; i < words; i++) {
        sum = (sum * 31) + p[i];
    }
    return sum;
}

void config_init_defaults(desktop_config_t *cfg)
{
    if (!cfg) return;
    cfg->magic = CONFIG_MAGIC;
    cfg->version = CONFIG_VERSION;
    cfg->screen_width = 1024;
    cfg->screen_height = 768;
    cfg->bpp = 32;
    cfg->color_desktop_bg = FB_COLOR_DARK_SLATE;
    cfg->color_panel_bg = FB_COLOR_PANEL_BG;
    cfg->color_accent = FB_COLOR_CYAN_ACCENT;
    cfg->theme_id = THEME_JARVIS_DARK;
    cfg->taskbar_visible = 1;
    cfg->taskbar_position = 0; /* bottom */
    cfg->startup_apps_mask = 0x1F; /* All 5 standard apps enabled */
    cfg->mouse_sensitivity = 1;
    cfg->notif_timeout_ticks = 400; /* 4 seconds at 100Hz PIT */
    cfg->ai_backend_pref = 0; /* Ollama */
    cfg->audio_enabled = 0; /* Audio hardware unsupported in kernel */
    cfg->checksum = calculate_checksum(cfg);
}

void config_reset_defaults(desktop_config_t *cfg)
{
    config_init_defaults(cfg);
}

void config_update_checksum(desktop_config_t *cfg)
{
    if (cfg) {
        cfg->checksum = calculate_checksum(cfg);
    }
}

int config_validate(const desktop_config_t *cfg)
{
    if (!cfg) return -1;
    if (cfg->magic != CONFIG_MAGIC) return -2;
    if (cfg->version != CONFIG_VERSION) return -3;
    /* Supported graphics modes: only 1024x768x32bpp is supported in kernel VBE/BGA driver */
    if (cfg->screen_width != 1024 || cfg->screen_height != 768) return -4;
    if (cfg->bpp != 32) return -6;
    if (cfg->theme_id >= THEME_COUNT) return -7;
    if (cfg->taskbar_position > 1) return -8;
    if (cfg->mouse_sensitivity < 1 || cfg->mouse_sensitivity > 3) return -10;
    if (cfg->ai_backend_pref > 3) return -11;
    if (cfg->audio_enabled != 0) return -12; /* Reject unsupported audio config */

    uint32_t expected_cs = calculate_checksum(cfg);
    if (cfg->checksum != expected_cs) return -9; /* Corrupted config */

    return 0;
}

int config_save_to_fs(const desktop_config_t *cfg, const char *path)
{
    if (!cfg || !path) return -1;
    if (config_validate(cfg) != 0) return -2;

    int fd = fs_open(path);
    if (fd < 0) {
        int cr = fs_create(path);
        if (cr != FS_OK && cr != FS_ERR_ALREADY_EXISTS) return -3;
        fd = fs_open(path);
    }
    if (fd < 0) return -4;

    int written = fs_write(fd, cfg, sizeof(desktop_config_t));
    fs_close(fd);

    return (written == (int)sizeof(desktop_config_t)) ? 0 : -5;
}

int config_load_from_fs(desktop_config_t *cfg, const char *path)
{
    if (!cfg || !path) return -1;

    int fd = fs_open(path);
    if (fd < 0) {
        /* File does not exist: fall back to safe defaults */
        config_init_defaults(cfg);
        return -2;
    }

    desktop_config_t temp;
    int bytes = fs_read(fd, &temp, sizeof(desktop_config_t));
    fs_close(fd);

    if (bytes != (int)sizeof(desktop_config_t) || config_validate(&temp) != 0) {
        /* File corrupted or incomplete: fall back to safe defaults */
        config_init_defaults(cfg);
        return -3;
    }

    *cfg = temp;
    return 0;
}

const desktop_config_t *config_get_active(void)
{
    if (g_active_config.magic != CONFIG_MAGIC) {
        config_init_defaults(&g_active_config);
    }
    return &g_active_config;
}

int config_apply(const desktop_config_t *cfg)
{
    if (!cfg) return -1;
    if (config_validate(cfg) != 0) return -2;
    g_active_config = *cfg;
    return 0;
}

int test_config_invariants(void)
{
    desktop_config_t cfg;
    config_init_defaults(&cfg);

    /* 1. Default validation */
    if (config_validate(&cfg) != 0) return -1;
    if (cfg.magic != CONFIG_MAGIC) return -2;
    if (cfg.version != CONFIG_VERSION) return -3;

    /* 2. Corrupted checksum detection */
    desktop_config_t corrupt = cfg;
    corrupt.checksum ^= 0xDEADBEEF;
    if (config_validate(&corrupt) != -9) return -4;

    /* 3. Out-of-bounds / unsupported resolution */
    desktop_config_t bad_res = cfg;
    bad_res.screen_width = 800; /* Only 1024x768 is supported */
    bad_res.checksum = calculate_checksum(&bad_res);
    if (config_validate(&bad_res) != -4) return -5;

    /* 4. Unsupported audio subsystem configuration rejection */
    desktop_config_t bad_audio = cfg;
    bad_audio.audio_enabled = 1; /* Audio hardware does not exist */
    bad_audio.checksum = calculate_checksum(&bad_audio);
    if (config_validate(&bad_audio) != -12) return -13;

    /* 5. Unsupported mouse sensitivity */
    desktop_config_t bad_mouse = cfg;
    bad_mouse.mouse_sensitivity = 9;
    bad_mouse.checksum = calculate_checksum(&bad_mouse);
    if (config_validate(&bad_mouse) != -10) return -14;

    /* 6. Reset to defaults */
    config_reset_defaults(&bad_res);
    if (config_validate(&bad_res) != 0) return -6;

    /* 7. In-session filesystem persistence check */
    int save_res = config_save_to_fs(&cfg, "test_desk.cfg");
    if (save_res != 0) return -7;

    desktop_config_t loaded;
    int load_res = config_load_from_fs(&loaded, "test_desk.cfg");
    if (load_res != 0) return -8;
    if (loaded.screen_width != cfg.screen_width || loaded.color_accent != cfg.color_accent) return -9;
    if (loaded.mouse_sensitivity != cfg.mouse_sensitivity) return -15;

    /* Clean up test file */
    fs_delete("test_desk.cfg");

    return 0;
}

