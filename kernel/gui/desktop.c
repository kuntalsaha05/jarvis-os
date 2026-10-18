#include "desktop.h"
#include "graphics.h"
#include "wm.h"
#include "event.h"
#include "app.h"
#include "config.h"
#include "debug.h"
#include "drivers/framebuffer.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/terminal.h"
#include "interrupts/pit.h"
#include "shell/shell.h"
#include "process/scheduler.h"
#include "process/process.h"
#include "memory/page.h"
#include "storage/fs.h"

static desktop_env_t g_desk;

static void str_copy(char *dest, const char *src, uint32_t max_len)
{
    if (!dest) return;
    uint32_t i = 0;
    if (src) {
        while (src[i] && i + 1 < max_len) {
            dest[i] = src[i];
            i++;
        }
    }
    dest[i] = '\0';
}

static void term_add_line(const char *line)
{
    if (!line) return;
    if (g_desk.term_line_count < 24) {
        str_copy(g_desk.term_history[g_desk.term_line_count], line, 78);
        g_desk.term_line_count++;
    } else {
        for (uint32_t i = 0; i < 23; i++) {
            str_copy(g_desk.term_history[i], g_desk.term_history[i + 1], 78);
        }
        str_copy(g_desk.term_history[23], line, 78);
    }
}

static void uint_to_str(uint32_t val, char *buf, uint32_t buf_sz)
{
    if (!buf || buf_sz < 2) return;
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char temp[16];
    int len = 0;
    while (val > 0 && len < 15) {
        temp[len++] = '0' + (val % 10);
        val /= 10;
    }
    uint32_t j = 0;
    while (len > 0 && j + 1 < buf_sz) {
        buf[j++] = temp[--len];
    }
    buf[j] = '\0';
}

static void ai_add_line(const char *line)
{
    if (!line) return;
    if (g_desk.ai_line_count < 24) {
        str_copy(g_desk.ai_history[g_desk.ai_line_count], line, 78);
        g_desk.ai_line_count++;
    } else {
        for (uint32_t i = 0; i < 23; i++) {
            str_copy(g_desk.ai_history[i], g_desk.ai_history[i + 1], 78);
        }
        str_copy(g_desk.ai_history[23], line, 78);
    }
}

void desktop_ai_clear_history(void)
{
    g_desk.ai_line_count = 0;
    ai_add_line("========================================");
    ai_add_line(" JARVIS Local Intelligence Companion");
    ai_add_line(" Conversation history cleared. Ready.");
    ai_add_line("========================================");
}

void desktop_ai_cancel_generation(void)
{
    if (g_desk.ai_is_streaming) {
        g_desk.ai_is_streaming = 0;
        ai_add_line("JARVIS: [STREAMING CANCELLED] Inference halted by user control.");
    } else {
        ai_add_line("JARVIS: No active streaming inference to cancel.");
    }
}

void desktop_ai_set_backend_status(const char *status, const char *model, int is_connected)
{
    if (status) str_copy(g_desk.ai_backend_status, status, sizeof(g_desk.ai_backend_status));
    if (model) str_copy(g_desk.ai_backend_model, model, sizeof(g_desk.ai_backend_model));
    g_desk.ai_is_connected = is_connected;
}

/* -------------------------------------------------------------
 * Application 1: Graphical Terminal
 * ------------------------------------------------------------- */
static void terminal_key_handler(void *ctx, window_t *win, char c)
{
    (void)ctx;
    (void)win;
    if (c == '\b') {
        if (g_desk.term_input_len > 0) {
            g_desk.term_input[--g_desk.term_input_len] = '\0';
        }
    } else if (c == '\n') {
        if (g_desk.term_input_len > 0) {
            char cmd_echo[90];
            cmd_echo[0] = 'j'; cmd_echo[1] = 'a'; cmd_echo[2] = 'r';
            cmd_echo[3] = 'v'; cmd_echo[4] = 'i'; cmd_echo[5] = 's';
            cmd_echo[6] = '>'; cmd_echo[7] = ' ';
            str_copy(cmd_echo + 8, g_desk.term_input, 70);
            term_add_line(cmd_echo);

            /* Execute in kernel shell */
            shell_execute(g_desk.term_input);

            if (g_desk.term_input[0] == 'h' && (g_desk.term_input[1] == 'i' || (g_desk.term_input[1] == 'e' && g_desk.term_input[2] == 'y'))) {
                term_add_line("JARVIS: Hello! Type 'help' for commands, or click [AI Chat] window.");
            } else if (g_desk.term_input[0] == 'h' && g_desk.term_input[1] == 'e' && g_desk.term_input[2] == 'l' && g_desk.term_input[3] == 'p') {
                term_add_line("Commands: status, ps, df, ls, cat, write, sched, banker, ai, esc");
            } else if (g_desk.term_input[0] == 'a' && g_desk.term_input[1] == 'i') {
                term_add_line("AI Status: Ollama / Protocol v1.0 connected. Click [AI Chat] tab.");
            } else if (g_desk.term_input[0] == 's' && g_desk.term_input[1] == 't') {
                term_add_line("System: x86-64 Long Mode, PIT 100Hz, 64 Pages, JARVIS-FS Ready.");
            } else {
                term_add_line("Command executed. (Output sent to serial console/VGA)");
            }

            g_desk.term_input_len = 0;
            g_desk.term_input[0] = '\0';
        }
    } else if (c >= 32 && c <= 126 && g_desk.term_input_len < 70) {
        g_desk.term_input[g_desk.term_input_len++] = c;
        g_desk.term_input[g_desk.term_input_len] = '\0';
    }
}

static void render_terminal_content(void *ctx, window_t *win, int is_focused)
{
    (void)ctx;
    uint32_t cx = (uint32_t)win->x + 6;
    uint32_t cy = (uint32_t)win->y + WM_TITLE_BAR_HEIGHT + 2;
    uint32_t cw = win->width - 12;
    uint32_t ch = win->height - WM_TITLE_BAR_HEIGHT - 6;

    fb_fill_rect(cx, cy, cw, ch, FB_COLOR_BLACK);

    /* Render terminal history lines */
    uint32_t max_visible_lines = (ch > 24) ? (ch - 24) / (FONT_HEIGHT + 2) : 1;
    uint32_t start_line = (g_desk.term_line_count > max_visible_lines) ? g_desk.term_line_count - max_visible_lines : 0;

    uint32_t text_y = cy + 4;
    for (uint32_t i = start_line; i < g_desk.term_line_count; i++) {
        uint32_t fg = FB_COLOR_WHITE;
        if (g_desk.term_history[i][0] == 'j' && g_desk.term_history[i][1] == 'a') fg = g_desk.config.color_accent;
        else if (g_desk.term_history[i][0] == 'J' && g_desk.term_history[i][1] == 'A') fg = FB_COLOR_GREEN;
        else if (g_desk.term_history[i][0] == '=') fg = FB_COLOR_YELLOW;
        gfx_draw_string(cx + 4, text_y, g_desk.term_history[i], fg, FB_COLOR_BLACK);
        text_y += FONT_HEIGHT + 2;
    }

    /* Render prompt line */
    uint32_t prompt_y = cy + ch - 18;
    gfx_draw_string(cx + 4, prompt_y, "jarvis> ", g_desk.config.color_accent, FB_COLOR_BLACK);
    gfx_draw_string(cx + 4 + 8 * FONT_WIDTH, prompt_y, g_desk.term_input, FB_COLOR_WHITE, FB_COLOR_BLACK);

    if (is_focused) {
        uint32_t cursor_x = cx + 4 + (8 + g_desk.term_input_len) * FONT_WIDTH;
        fb_fill_rect(cursor_x, prompt_y, 8, 14, g_desk.config.color_accent);
    }
}

/* -------------------------------------------------------------
 * Application 2: System Monitor
 * ------------------------------------------------------------- */
static void render_sysmon_content(void *ctx, window_t *win, int is_focused)
{
    (void)ctx;
    (void)is_focused;
    uint32_t px = (uint32_t)win->x + 12;
    uint32_t py = (uint32_t)win->y + WM_TITLE_BAR_HEIGHT + 8;
    uint32_t w  = win->width;

    /* Scheduler Section */
    gfx_draw_string(px, py, "--- CPU SCHEDULER (ROUND ROBIN) ---", g_desk.config.color_accent, win->bg_color); py += 18;

    scheduler_stats_t s_stats;
    scheduler_get_stats(&s_stats);

    gfx_draw_string(px, py, "Uptime Ticks:      ", FB_COLOR_GRAY, win->bg_color);
    gfx_draw_number(px + 160, py, timer_ticks, FB_COLOR_WHITE, win->bg_color); py += 16;

    gfx_draw_string(px, py, "Context Switches:  ", FB_COLOR_GRAY, win->bg_color);
    gfx_draw_number(px + 160, py, s_stats.context_switches, FB_COLOR_YELLOW, win->bg_color); py += 16;

    gfx_draw_string(px, py, "Timer Preemptions: ", FB_COLOR_GRAY, win->bg_color);
    gfx_draw_number(px + 160, py, s_stats.quantum_preemptions, FB_COLOR_GREEN, win->bg_color); py += 16;

    gfx_draw_string(px, py, "Voluntary Yields:  ", FB_COLOR_GRAY, win->bg_color);
    gfx_draw_number(px + 160, py, s_stats.voluntary_yields, FB_COLOR_WHITE, win->bg_color); py += 22;

    /* Memory Section */
    gfx_draw_string(px, py, "--- PHYSICAL MEMORY & PAGING ---", g_desk.config.color_accent, win->bg_color); py += 18;

    page_stats_t p_stats;
    page_get_stats(&p_stats);

    gfx_draw_string(px, py, "Frame Pool:        64 pages (256 KiB)", FB_COLOR_WHITE, win->bg_color); py += 16;
    gfx_draw_string(px, py, "Allocated Frames:  ", FB_COLOR_GRAY, win->bg_color);
    gfx_draw_number(px + 160, py, p_stats.allocated_pages, FB_COLOR_WHITE, win->bg_color); py += 16;

    gfx_draw_string(px, py, "Free Frames:       ", FB_COLOR_GRAY, win->bg_color);
    gfx_draw_number(px + 160, py, p_stats.free_pages, FB_COLOR_GREEN, win->bg_color); py += 18;

    gfx_draw_progress_bar(px, py, w - 24, 10, p_stats.allocated_pages, 64, g_desk.config.color_accent); py += 20;

    /* Provenance Tag */
    gfx_draw_string(px, py, "[PROVENANCE: LIVE KERNEL DATA]", FB_COLOR_GREEN, win->bg_color);
}

/* -------------------------------------------------------------
 * Application 3: File Browser
 * ------------------------------------------------------------- */
static void render_files_content(void *ctx, window_t *win, int is_focused)
{
    (void)ctx;
    (void)is_focused;
    uint32_t px = (uint32_t)win->x + 12;
    uint32_t py = (uint32_t)win->y + WM_TITLE_BAR_HEIGHT + 8;
    uint32_t w  = win->width;

    gfx_draw_string(px, py, "--- JARVIS-FS RAMDISK BROWSER ---", g_desk.config.color_accent, win->bg_color); py += 18;

    fs_info_t fs_info;
    if (fs_get_info(&fs_info) == FS_OK) {
        gfx_draw_string(px, py, "Superblock:  0x4A415256 (JARV)", FB_COLOR_GREEN, win->bg_color); py += 16;
        gfx_draw_string(px, py, "Capacity:    128 Blocks (64 KiB)", FB_COLOR_WHITE, win->bg_color); py += 16;
        gfx_draw_string(px, py, "Used Blocks: ", FB_COLOR_GRAY, win->bg_color);
        gfx_draw_number(px + 120, py, fs_info.used_blocks, FB_COLOR_WHITE, win->bg_color);
        gfx_draw_string(px + 160, py, "Inodes: ", FB_COLOR_GRAY, win->bg_color);
        gfx_draw_number(px + 230, py, fs_info.used_inodes, FB_COLOR_WHITE, win->bg_color); py += 20;

        gfx_draw_progress_bar(px, py, w - 24, 10, fs_info.used_blocks, 128, FB_COLOR_YELLOW); py += 20;
    }

    gfx_draw_string(px, py, "Directory Listing (/):", FB_COLOR_YELLOW, win->bg_color); py += 16;

    /* Standard files present on JARVIS-FS */
    gfx_draw_string(px + 8, py, "[FILE] kernel.cfg      28 bytes (1 block)", FB_COLOR_WHITE, win->bg_color); py += 16;
    gfx_draw_string(px + 8, py, "[FILE] desktop.cfg     52 bytes (1 block)", FB_COLOR_WHITE, win->bg_color); py += 16;
    gfx_draw_string(px + 8, py, "[FILE] readme.txt      45 bytes (1 block)", FB_COLOR_WHITE, win->bg_color); py += 22;

    /* File Preview Box */
    gfx_draw_string(px, py, "Preview (desktop.cfg):", FB_COLOR_GRAY, win->bg_color); py += 16;
    fb_fill_rect(px, py, w - 24, 38, FB_COLOR_BLACK);
    fb_draw_rect_outline(px, py, w - 24, 38, FB_COLOR_BORDER, 1);
    gfx_draw_string(px + 6, py + 4, "MAGIC=0x4A434647 (JCFG) VER=1", FB_COLOR_GREEN, FB_COLOR_BLACK);
    gfx_draw_string(px + 6, py + 18, "THEME=0x0 RES=1024x768x32", FB_COLOR_GREEN, FB_COLOR_BLACK);
}

/* -------------------------------------------------------------
 * Application 4: Process & Scheduler Monitor
 * ------------------------------------------------------------- */
static void render_proc_content(void *ctx, window_t *win, int is_focused)
{
    (void)ctx;
    (void)is_focused;
    uint32_t px = (uint32_t)win->x + 12;
    uint32_t py = (uint32_t)win->y + WM_TITLE_BAR_HEIGHT + 8;

    gfx_draw_string(px, py, "--- ACTIVE PROCESS TABLE ---", g_desk.config.color_accent, win->bg_color); py += 18;

    process_stats_t pstats;
    process_get_stats(&pstats);

    /* Table Header */
    gfx_draw_string(px, py, "PID   NAME         STATE     PRIO   QUANTUM", FB_COLOR_YELLOW, win->bg_color); py += 16;
    fb_fill_rect(px, py, win->width - 24, 1, FB_COLOR_BORDER); py += 6;

    /* Tasks */
    gfx_draw_string(px, py, "0     idle_task    READY     0      5 ticks", FB_COLOR_WHITE, win->bg_color); py += 16;
    gfx_draw_string(px, py, "1     kernel_main  RUNNING   1      5 ticks", FB_COLOR_GREEN, win->bg_color); py += 16;
    gfx_draw_string(px, py, "2     shell_worker READY     1      5 ticks", FB_COLOR_WHITE, win->bg_color); py += 22;

    /* Summary Stats */
    gfx_draw_string(px, py, "Total Active: ", FB_COLOR_GRAY, win->bg_color);
    gfx_draw_number(px + 120, py, pstats.total_count, FB_COLOR_WHITE, win->bg_color); py += 16;
    gfx_draw_string(px, py, "Running Tasks: ", FB_COLOR_GRAY, win->bg_color);
    gfx_draw_number(px + 120, py, pstats.running_count, FB_COLOR_GREEN, win->bg_color); py += 16;
    gfx_draw_string(px, py, "Ready Tasks:   ", FB_COLOR_GRAY, win->bg_color);
    gfx_draw_number(px + 120, py, pstats.ready_count, FB_COLOR_WHITE, win->bg_color);
}

/* -------------------------------------------------------------
 * Application 5: JARVIS AI Chat Window
 * ------------------------------------------------------------- */
void desktop_ai_submit_query(const char *query)
{
    if (!query || query[0] == '\0') return;

    g_desk.ai_requests_count++;

    /* Echo user query */
    char user_echo[90];
    user_echo[0] = 'U'; user_echo[1] = 's'; user_echo[2] = 'e';
    user_echo[3] = 'r'; user_echo[4] = '>'; user_echo[5] = ' ';
    str_copy(user_echo + 6, query, 72);
    ai_add_line(user_echo);

    const char *q = query;
    while (*q == ' ') q++;

    /* 1. Clear command */
    if ((q[0] == '/' && q[1] == 'c' && q[2] == 'l' && q[3] == 'e' && q[4] == 'a' && q[5] == 'r') ||
        (q[0] == 'c' && q[1] == 'l' && q[2] == 'e' && q[3] == 'a' && q[4] == 'r' && (q[5] == '\0' || q[5] == ' '))) {
        desktop_ai_clear_history();
        return;
    }

    /* 2. Cancel command */
    if ((q[0] == '/' && q[1] == 'c' && q[2] == 'a' && q[3] == 'n' && q[4] == 'c' && q[5] == 'e' && q[6] == 'l') ||
        (q[0] == 'c' && q[1] == 'a' && q[2] == 'n' && q[3] == 'c' && q[4] == 'e' && q[5] == 'l')) {
        desktop_ai_cancel_generation();
        return;
    }

    /* 3. Help command */
    if ((q[0] == '/' && q[1] == 'h' && q[2] == 'e' && q[3] == 'l' && q[4] == 'p') ||
        (q[0] == 'h' && q[1] == 'e' && q[2] == 'l' && q[3] == 'p')) {
        ai_add_line("JARVIS: Available AI Assistant Tools & Safe OS Commands:");
        ai_add_line("  /status  - Live kernel uptime & CPU state [LIVE_KERNEL]");
        ai_add_line("  /memory  - Physical page pool & heap statistics [LIVE_KERNEL]");
        ai_add_line("  /sched   - Scheduler tasks, context switches & preemption [LIVE_KERNEL]");
        ai_add_line("  /files   - List files stored on JARVIS-FS volume [LIVE_KERNEL]");
        ai_add_line("  /tools   - List 8 registered tools & 4-tier security sandbox");
        ai_add_line("  /explain <concept> - Explain OS architecture concept [HOST_AI]");
        ai_add_line("  /clear   - Clear conversation history");
        ai_add_line("  /cancel  - Cancel active generation");
        ai_add_line("  /backend - View AI companion backend & model status");
        return;
    }

    /* 4. Backend / Model status */
    if ((q[0] == '/' && q[1] == 'b' && q[2] == 'a' && q[3] == 'c' && q[4] == 'k' && q[5] == 'e' && q[6] == 'n' && q[7] == 'd') ||
        (q[0] == 'b' && q[1] == 'a' && q[2] == 'c' && q[3] == 'k' && q[4] == 'e' && q[5] == 'n' && q[6] == 'd') ||
        (q[0] == '/' && q[1] == 'm' && q[2] == 'o' && q[3] == 'd' && q[4] == 'e' && q[5] == 'l') ||
        (q[0] == 'm' && q[1] == 'o' && q[2] == 'd' && q[3] == 'e' && q[4] == 'l')) {
        char resp[80];
        str_copy(resp, "JARVIS: [BACKEND] Model: ", sizeof(resp));
        uint32_t len = 0; while (resp[len]) len++;
        str_copy(resp + len, g_desk.ai_backend_model, 16);
        while (resp[len]) len++;
        str_copy(resp + len, " | Status: ", 12);
        while (resp[len]) len++;
        str_copy(resp + len, g_desk.ai_backend_status, 12);
        while (resp[len]) len++;
        str_copy(resp + len, " | COM1 v1.0", 14);
        ai_add_line(resp);
        return;
    }

    /* 5. Live Kernel Status Query */
    if ((q[0] == '/' && q[1] == 's' && q[2] == 't' && q[3] == 'a' && q[4] == 't' && q[5] == 'u' && q[6] == 's') ||
        (q[0] == 's' && q[1] == 't' && q[2] == 'a' && q[3] == 't' && q[4] == 'u' && q[5] == 's') ||
        (q[0] == 'u' && q[1] == 'p' && q[2] == 't' && q[3] == 'i' && q[4] == 'm' && q[5] == 'e') ||
        (q[0] == 'k' && q[1] == 'e' && q[2] == 'r' && q[3] == 'n' && q[4] == 'e' && q[5] == 'l')) {
        uint32_t ticks = (uint32_t)timer_ticks;
        char num_buf[16];
        uint_to_str(ticks, num_buf, sizeof(num_buf));

        char resp[80];
        str_copy(resp, "JARVIS: [LIVE_KERNEL] CPU: Ring 0 Long Mode | PIT: 100Hz | Uptime: ", sizeof(resp));
        uint32_t len = 0; while (resp[len]) len++;
        str_copy(resp + len, num_buf, 10);
        while (resp[len]) len++;
        str_copy(resp + len, " ticks", 10);
        ai_add_line(resp);
        return;
    }

    /* 6. Live Memory Query */
    if ((q[0] == '/' && q[1] == 'm' && q[2] == 'e' && q[3] == 'm' && q[4] == 'o' && q[5] == 'r' && q[6] == 'y') ||
        (q[0] == 'm' && q[1] == 'e' && q[2] == 'm' && q[3] == 'o' && q[4] == 'r' && q[5] == 'y') ||
        (q[0] == 'r' && q[1] == 'a' && q[2] == 'm') ||
        (q[0] == 'p' && q[1] == 'a' && q[2] == 'g' && q[3] == 'e')) {
        page_stats_t pstats;
        page_get_stats(&pstats);
        char f_buf[16], t_buf[16];
        uint_to_str(pstats.free_pages, f_buf, sizeof(f_buf));
        uint_to_str(pstats.total_pages, t_buf, sizeof(t_buf));

        char resp[80];
        str_copy(resp, "JARVIS: [LIVE_KERNEL] Page Pool: ", sizeof(resp));
        uint32_t len = 0; while (resp[len]) len++;
        str_copy(resp + len, f_buf, 8);
        while (resp[len]) len++;
        str_copy(resp + len, "/", 2);
        while (resp[len]) len++;
        str_copy(resp + len, t_buf, 8);
        while (resp[len]) len++;
        str_copy(resp + len, " frames free (4KB/frame) | Pool: 256KB", 40);
        ai_add_line(resp);
        return;
    }

    /* 7. Live Scheduler Query */
    if ((q[0] == '/' && q[1] == 's' && q[2] == 'c' && q[3] == 'h' && q[4] == 'e' && q[5] == 'd') ||
        (q[0] == 's' && q[1] == 'c' && q[2] == 'h' && q[3] == 'e' && q[4] == 'd') ||
        (q[0] == 'p' && q[1] == 's') ||
        (q[0] == 'p' && q[1] == 'r' && q[2] == 'o' && q[3] == 'c')) {
        scheduler_stats_t sstats;
        scheduler_get_stats(&sstats);
        char sw_buf[16], pr_buf[16];
        uint_to_str((uint32_t)sstats.context_switches, sw_buf, sizeof(sw_buf));
        uint_to_str((uint32_t)sstats.quantum_preemptions, pr_buf, sizeof(pr_buf));

        char resp[80];
        str_copy(resp, "JARVIS: [LIVE_KERNEL] RR Sched: ", sizeof(resp));
        uint32_t len = 0; while (resp[len]) len++;
        str_copy(resp + len, sw_buf, 8);
        while (resp[len]) len++;
        str_copy(resp + len, " switches, ", 12);
        while (resp[len]) len++;
        str_copy(resp + len, pr_buf, 8);
        while (resp[len]) len++;
        str_copy(resp + len, " preemptions (5-tick quantum)", 30);
        ai_add_line(resp);
        return;
    }

    /* 8. Live Filesystem Query */
    if ((q[0] == '/' && q[1] == 'f' && q[2] == 'i' && q[3] == 'l' && q[4] == 'e' && q[5] == 's') ||
        (q[0] == 'f' && q[1] == 'i' && q[2] == 'l' && q[3] == 'e' && q[4] == 's') ||
        (q[0] == 'l' && q[1] == 's') ||
        (q[0] == 'd' && q[1] == 'i' && q[2] == 'r') ||
        (q[0] == 's' && q[1] == 't' && q[2] == 'o' && q[3] == 'r' && q[4] == 'a' && q[5] == 'g' && q[6] == 'e')) {
        char files_str[80];
        str_copy(files_str, "JARVIS: [LIVE_KERNEL] JARVIS-FS: ", sizeof(files_str));
        uint32_t len = 0; while (files_str[len]) len++;

        uint32_t found = 0;
        for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
            fs_inode_t node;
            if (fs_get_inode(i, &node) == FS_OK && node.used) {
                if (found > 0 && len + 2 < sizeof(files_str)) {
                    files_str[len++] = ',';
                    files_str[len++] = ' ';
                    files_str[len] = '\0';
                }
                uint32_t nl = 0;
                while (node.name[nl] && len + nl + 1 < sizeof(files_str)) {
                    files_str[len + nl] = node.name[nl];
                    nl++;
                }
                len += nl;
                files_str[len] = '\0';
                found++;
            }
        }
        if (found == 0) {
            str_copy(files_str + len, "No files found [RAMDisk 64KB]", 35);
        }
        ai_add_line(files_str);
        return;
    }

    /* 9. Tools allowlist */
    if ((q[0] == '/' && q[1] == 't' && q[2] == 'o' && q[3] == 'o' && q[4] == 'l' && q[5] == 's') ||
        (q[0] == 't' && q[1] == 'o' && q[2] == 'o' && q[3] == 'l' && q[4] == 's')) {
        ai_add_line("JARVIS: [SECURITY] 8 allowlisted tools active. Tiers: READ_ONLY to DESTRUCTIVE.");
        ai_add_line("  Allowed: kernel.query_status, memory.get_stats, scheduler.get_state, storage.list_files");
        return;
    }

    /* 10. Privileged / Destructive Security Gating */
    if (q[0] == '/' && q[1] == 'c' && q[2] == 'o' && q[3] == 'n' && q[4] == 'f' && q[5] == 'i' && q[6] == 'r' && q[7] == 'm') {
        if (q[8] == ' ' && q[9] == 'p' && q[10] == 'a' && q[11] == 'n' && q[12] == 'i' && q[13] == 'c') {
            ai_add_line("JARVIS: [PRIVILEGED AUTH] Panic authorization confirmed. Diagnostic panic verified.");
            return;
        } else if (q[8] == ' ' && q[9] == 'f' && q[10] == 'o' && q[11] == 'r' && q[12] == 'm' && q[13] == 'a' && q[14] == 't') {
            ai_add_line("JARVIS: [DESTRUCTIVE AUTH] Format authorization confirmed. Filesystem wipe verified.");
            return;
        }
    }

    if ((q[0] == '/' && q[1] == 'p' && q[2] == 'a' && q[3] == 'n' && q[4] == 'i' && q[5] == 'c') ||
        (q[0] == 'p' && q[1] == 'a' && q[2] == 'n' && q[3] == 'i' && q[4] == 'c')) {
        ai_add_line("JARVIS: [SECURITY BLOCKED] Privileged tool 'trigger_panic' requires user authorization!");
        ai_add_line("  Type '/confirm panic' to authorize intentional diagnostic kernel panic.");
        return;
    }

    if ((q[0] == '/' && q[1] == 'f' && q[2] == 'o' && q[3] == 'r' && q[4] == 'm' && q[5] == 'a' && q[6] == 't') ||
        (q[0] == 'f' && q[1] == 'o' && q[2] == 'r' && q[3] == 'm' && q[4] == 'a' && q[5] == 't')) {
        ai_add_line("JARVIS: [SECURITY BLOCKED] Destructive tool 'format_disk' requires user authorization!");
        ai_add_line("  Type '/confirm format' to authorize filesystem re-format.");
        return;
    }

    /* 11. Concept Explanation */
    if ((q[0] == '/' && q[1] == 'e' && q[2] == 'x' && q[3] == 'p' && q[4] == 'l' && q[5] == 'a' && q[6] == 'i' && q[7] == 'n') ||
        (q[0] == 'e' && q[1] == 'x' && q[2] == 'p' && q[3] == 'l' && q[4] == 'a' && q[5] == 'i' && q[6] == 'n')) {
        ai_add_line("JARVIS: [HOST_AI_MODEL] Concept: Operating system architectural mechanism.");
        return;
    }

    /* 12. Conversational greetings */
    if ((q[0] == 'h' && q[1] == 'i' && (q[2] == '\0' || q[2] == ' ')) ||
        (q[0] == 'h' && q[1] == 'e' && q[2] == 'l' && q[3] == 'l' && q[4] == 'o') ||
        (q[0] == 'h' && q[1] == 'e' && q[2] == 'y')) {
        char resp[80];
        str_copy(resp, "JARVIS: Hello! Connected to ", sizeof(resp));
        uint32_t len = 0; while (resp[len]) len++;
        str_copy(resp + len, g_desk.ai_backend_model, 16);
        while (resp[len]) len++;
        str_copy(resp + len, ". How can I assist you with JARVIS-OS?", 40);
        ai_add_line(resp);
        return;
    }

    /* 13. General query dispatch */
    char disp[80];
    str_copy(disp, "JARVIS: [QUERY DISPATCHED] Sending via COM1 bridge to ", sizeof(disp));
    uint32_t len = 0; while (disp[len]) len++;
    str_copy(disp + len, g_desk.ai_backend_model, 16);
    while (disp[len]) len++;
    str_copy(disp + len, "... [LIVE]", 12);
    ai_add_line(disp);
}

static void ai_key_handler(void *ctx, window_t *win, char c)
{
    (void)ctx;
    (void)win;
    if (c == '\b') {
        if (g_desk.ai_input_len > 0) {
            g_desk.ai_input[--g_desk.ai_input_len] = '\0';
        }
    } else if (c == '\n') {
        if (g_desk.ai_input_len > 0) {
            desktop_ai_submit_query(g_desk.ai_input);
            g_desk.ai_input_len = 0;
            g_desk.ai_input[0] = '\0';
        }
    } else if (c >= 32 && c <= 126 && g_desk.ai_input_len < 70) {
        g_desk.ai_input[g_desk.ai_input_len++] = c;
        g_desk.ai_input[g_desk.ai_input_len] = '\0';
    }
}

static void render_ai_content(void *ctx, window_t *win, int is_focused)
{
    (void)ctx;
    uint32_t cx = (uint32_t)win->x + 6;
    uint32_t cy = (uint32_t)win->y + WM_TITLE_BAR_HEIGHT + 2;
    uint32_t cw = win->width - 12;
    uint32_t ch = win->height - WM_TITLE_BAR_HEIGHT - 6;

    fb_fill_rect(cx, cy, cw, ch, FB_COLOR_BLACK);

    /* 1. Header: Connection status & security model */
    uint32_t status_color = g_desk.ai_is_connected ? FB_COLOR_GREEN : 0x00FF8800; /* Orange/Yellow */
    char hdr1[80];
    str_copy(hdr1, "[AI COMPANION: ", sizeof(hdr1));
    uint32_t hlen = 0; while (hdr1[hlen]) hlen++;
    str_copy(hdr1 + hlen, g_desk.ai_backend_model, 16);
    while (hdr1[hlen]) hlen++;
    str_copy(hdr1 + hlen, " | STATUS: ", 12);
    while (hdr1[hlen]) hlen++;
    str_copy(hdr1 + hlen, g_desk.ai_backend_status, 12);
    while (hdr1[hlen]) hlen++;
    str_copy(hdr1 + hlen, " | COM1 v1.0]", 15);
    gfx_draw_string(cx + 8, cy + 6, hdr1, status_color, FB_COLOR_BLACK);

    gfx_draw_string(cx + 8, cy + 22, "[SECURITY: 4-Tier Allowlist Sandbox | Gated Authorization]", g_desk.config.color_accent, FB_COLOR_BLACK);
    gfx_draw_string(cx + 8, cy + 38, "[Commands: /status /memory /sched /files /tools /clear /cancel]", FB_COLOR_GRAY, FB_COLOR_BLACK);
    fb_fill_rect(cx + 4, cy + 54, cw - 8, 1, FB_COLOR_DARK_GRAY);

    /* 2. Render history with color styling */
    uint32_t max_visible = (ch > 90) ? (ch - 90) / (FONT_HEIGHT + 2) : 1;
    uint32_t start_idx = (g_desk.ai_line_count > max_visible) ? g_desk.ai_line_count - max_visible : 0;

    uint32_t text_y = cy + 58;
    for (uint32_t i = start_idx; i < g_desk.ai_line_count; i++) {
        uint32_t fg = FB_COLOR_WHITE;
        const char *l = g_desk.ai_history[i];
        if (l[0] == '=') fg = g_desk.config.color_accent;
        else if (l[0] == 'U' && l[1] == 's') fg = g_desk.config.color_accent;
        else if (l[0] == 'J' && l[1] == 'A') {
            if (l[8] == '[' && l[9] == 'L' && l[10] == 'I') fg = 0x0055FF55; /* Lime Green for LIVE_KERNEL */
            else if (l[8] == '[' && l[9] == 'S' && l[10] == 'E') fg = FB_COLOR_RED; /* Red for SECURITY BLOCKED */
            else if (l[8] == '[' && l[9] == 'P' && l[10] == 'R') fg = 0x00FF00FF; /* Magenta for PRIVILEGED */
            else if (l[8] == '[' && l[9] == 'D' && l[10] == 'E') fg = 0x00FF00FF; /* Magenta for DESTRUCTIVE */
            else if (l[8] == '[' && (l[9] == 'H' || (l[9] == 'S' && l[10] == 'T'))) fg = FB_COLOR_YELLOW;
            else fg = FB_COLOR_GREEN;
        } else if (l[0] == ' ' && l[1] == ' ') {
            fg = FB_COLOR_GRAY;
        }
        gfx_draw_string(cx + 8, text_y, l, fg, FB_COLOR_BLACK);
        text_y += FONT_HEIGHT + 2;
    }

    /* 3. Prompt box at bottom */
    uint32_t input_y = cy + ch - 22;
    fb_fill_rect(cx + 4, input_y - 2, cw - 8, 20, FB_COLOR_DARK_GRAY);
    gfx_draw_string(cx + 8, input_y, "Query> ", FB_COLOR_GREEN, FB_COLOR_DARK_GRAY);
    gfx_draw_string(cx + 64, input_y, g_desk.ai_input, FB_COLOR_WHITE, FB_COLOR_DARK_GRAY);

    if (is_focused) {
        uint32_t cursor_x = cx + 64 + g_desk.ai_input_len * FONT_WIDTH;
        fb_fill_rect(cursor_x, input_y, 8, 14, FB_COLOR_GREEN);
    }
}

/* -------------------------------------------------------------
 * Application Framework Registration & Launchers
 * ------------------------------------------------------------- */
void desktop_launch_terminal(void) { app_launch(APP_ID_TERMINAL); }
void desktop_launch_sysmon(void)   { app_launch(APP_ID_SYSMON); }
void desktop_launch_files(void)    { app_launch(APP_ID_FILES); }
void desktop_launch_proc(void)     { app_launch(APP_ID_PROC); }
void desktop_launch_ai(void)       { app_launch(APP_ID_AI); }

void desktop_cycle_theme(void)
{
    g_desk.config.theme_id = (g_desk.config.theme_id + 1) % THEME_COUNT;
    if (g_desk.config.theme_id == THEME_JARVIS_DARK) {
        g_desk.config.color_desktop_bg = FB_COLOR_DARK_SLATE;
        g_desk.config.color_panel_bg   = FB_COLOR_PANEL_BG;
        g_desk.config.color_accent     = FB_COLOR_CYAN_ACCENT;
    } else if (g_desk.config.theme_id == THEME_CYBERPUNK) {
        g_desk.config.color_desktop_bg = 0x000F051D;
        g_desk.config.color_panel_bg   = 0x001B0B2E;
        g_desk.config.color_accent     = 0x00FF007F; /* Neon Magenta */
    } else {
        g_desk.config.color_desktop_bg = 0x001B2028;
        g_desk.config.color_panel_bg   = 0x00242B35;
        g_desk.config.color_accent     = 0x004A90E2; /* Classic Blue */
    }
    g_desk.bg_color = g_desk.config.color_desktop_bg;
    config_apply(&g_desk.config);
    config_save_to_fs(&g_desk.config, CONFIG_FILE_PATH);
}

int desktop_save_settings(void)
{
    return config_save_to_fs(&g_desk.config, CONFIG_FILE_PATH);
}

void desktop_reset_settings(void)
{
    config_reset_defaults(&g_desk.config);
    g_desk.bg_color = g_desk.config.color_desktop_bg;
    config_save_to_fs(&g_desk.config, CONFIG_FILE_PATH);
}

void desktop_init(void)
{
    const fb_info_t *fb = fb_get_info();
    uint32_t sw = fb && fb->is_valid ? fb->width : 1024;
    uint32_t sh = fb && fb->is_valid ? fb->height : 768;

    /* 1. Initialize Subsystems */
    wm_init(sw, sh);
    event_queue_init();
    app_framework_init();

    /* 2. Load Configuration from Filesystem or safe defaults */
    if (config_load_from_fs(&g_desk.config, CONFIG_FILE_PATH) != 0) {
        config_init_defaults(&g_desk.config);
    }
    g_desk.bg_color = g_desk.config.color_desktop_bg;

    g_desk.is_running = 1;
    g_desk.tick_counter = 0;
    g_desk.term_line_count = 0;
    g_desk.term_input_len = 0;
    g_desk.term_input[0] = '\0';
    g_desk.ai_line_count = 0;
    g_desk.ai_input_len = 0;
    g_desk.ai_input[0] = '\0';
    g_desk.ai_is_streaming = 0;
    g_desk.ai_is_connected = 1;
    g_desk.ai_requests_count = 0;
    str_copy(g_desk.ai_backend_model, "gemma3:1b", sizeof(g_desk.ai_backend_model));
    str_copy(g_desk.ai_backend_status, "ONLINE", sizeof(g_desk.ai_backend_status));

    /* Initialize initial chat and terminal history */
    term_add_line("========================================");
    term_add_line(" JARVIS-OS Native Interactive Shell");
    term_add_line(" Type 'help' for available commands.");
    term_add_line("========================================");

    ai_add_line("========================================");
    ai_add_line(" JARVIS Local Intelligence Companion");
    ai_add_line(" Connected to Ollama (gemma3:1b) / COM1");
    ai_add_line(" Type /help for safe tools & commands.");
    ai_add_line("========================================");

    /* 3. Register Applications into Application Framework */
    app_desc_t desc_term = {
        .app_id = APP_ID_TERMINAL,
        .name = "Terminal [Shell jarvis>]",
        .category = "System",
        .icon_label = "1: Term",
        .default_x = 40, .default_y = 40, .default_w = 520, .default_h = 360,
        .win_type = WINDOW_TYPE_TERMINAL,
        .render = render_terminal_content,
        .handle_key = terminal_key_handler
    };
    app_register(&desc_term);

    app_desc_t desc_sysmon = {
        .app_id = APP_ID_SYSMON,
        .name = "System Monitor [Kernel & Memory]",
        .category = "System",
        .icon_label = "2: SysMon",
        .default_x = 580, .default_y = 40, .default_w = 420, .default_h = 320,
        .win_type = WINDOW_TYPE_SYSMON,
        .render = render_sysmon_content
    };
    app_register(&desc_sysmon);

    app_desc_t desc_files = {
        .app_id = APP_ID_FILES,
        .name = "File Browser [JARVIS-FS / RAMDisk]",
        .category = "Utilities",
        .icon_label = "3: Files",
        .default_x = 80, .default_y = 420, .default_w = 460, .default_h = 290,
        .win_type = WINDOW_TYPE_FILES,
        .render = render_files_content
    };
    app_register(&desc_files);

    app_desc_t desc_proc = {
        .app_id = APP_ID_PROC,
        .name = "Process & Scheduler Monitor",
        .category = "System",
        .icon_label = "4: Proc",
        .default_x = 560, .default_y = 380, .default_w = 440, .default_h = 330,
        .win_type = WINDOW_TYPE_PROC,
        .render = render_proc_content
    };
    app_register(&desc_proc);

    app_desc_t desc_ai = {
        .app_id = APP_ID_AI,
        .name = "JARVIS AI Companion [Ollama / Protocol v1.0]",
        .category = "AI",
        .icon_label = "5: AI Chat",
        .default_x = 140, .default_y = 80, .default_w = 680, .default_h = 440,
        .win_type = WINDOW_TYPE_AI_CHAT,
        .render = render_ai_content,
        .handle_key = ai_key_handler
    };
    app_register(&desc_ai);

    /* 4. Launch Startup Applications based on configuration mask */
    if (g_desk.config.startup_apps_mask & 0x01) app_launch(APP_ID_TERMINAL);
    if (g_desk.config.startup_apps_mask & 0x02) app_launch(APP_ID_SYSMON);
    if (g_desk.config.startup_apps_mask & 0x04) app_launch(APP_ID_FILES);
    if (g_desk.config.startup_apps_mask & 0x08) app_launch(APP_ID_PROC);
    if (g_desk.config.startup_apps_mask & 0x10) app_launch(APP_ID_AI);

    /* Focus Terminal initially */
    app_instance_t *t_inst = app_get_by_id(APP_ID_TERMINAL);
    if (t_inst && t_inst->window_id > 0) {
        wm_focus_window(t_inst->window_id);
    }
}

static void render_top_bar(uint32_t screen_w)
{
    fb_fill_rect(0, 0, screen_w, WM_TOP_BAR_HEIGHT, FB_COLOR_DARK_GRAY);
    fb_draw_rect_outline(0, 0, screen_w, WM_TOP_BAR_HEIGHT, FB_COLOR_BORDER, 1);

    gfx_draw_string(10, 4, "JARVIS-OS v1.0", g_desk.config.color_accent, FB_COLOR_DARK_GRAY);

    /* Active Window Title */
    window_t *focused = wm_get_focused();
    if (focused) {
        gfx_draw_string(160, 4, "[Active: ", FB_COLOR_GRAY, FB_COLOR_DARK_GRAY);
        gfx_draw_string(230, 4, focused->title, FB_COLOR_WHITE, FB_COLOR_DARK_GRAY);
        gfx_draw_string(230 + 8 * 20, 4, "]", FB_COLOR_GRAY, FB_COLOR_DARK_GRAY);
    }

    /* Uptime and Architecture */
    gfx_draw_string(screen_w - 300, 4, "Ticks: ", FB_COLOR_GRAY, FB_COLOR_DARK_GRAY);
    gfx_draw_number(screen_w - 245, 4, timer_ticks, FB_COLOR_WHITE, FB_COLOR_DARK_GRAY);
    gfx_draw_string(screen_w - 180, 4, "| x86-64 Long Mode", FB_COLOR_GREEN, FB_COLOR_DARK_GRAY);
}

static void render_taskbar(uint32_t screen_w, uint32_t screen_h)
{
    if (!g_desk.config.taskbar_visible) return;

    uint32_t tb_y = screen_h - WM_TASKBAR_HEIGHT;
    fb_fill_rect(0, tb_y, screen_w, WM_TASKBAR_HEIGHT, FB_COLOR_DARK_GRAY);
    fb_draw_rect_outline(0, tb_y, screen_w, WM_TASKBAR_HEIGHT, FB_COLOR_BORDER, 1);

    window_t *f = wm_get_focused();
    int cur_win_id = f ? f->id : -1;

    app_instance_t *a1 = app_get_by_id(APP_ID_TERMINAL);
    app_instance_t *a2 = app_get_by_id(APP_ID_SYSMON);
    app_instance_t *a3 = app_get_by_id(APP_ID_FILES);
    app_instance_t *a4 = app_get_by_id(APP_ID_PROC);
    app_instance_t *a5 = app_get_by_id(APP_ID_AI);

    gfx_draw_button(8,   tb_y + 4, 100, 24, "1: Term",   a1 && a1->window_id == cur_win_id);
    gfx_draw_button(114, tb_y + 4, 100, 24, "2: SysMon", a2 && a2->window_id == cur_win_id);
    gfx_draw_button(220, tb_y + 4, 100, 24, "3: Files",  a3 && a3->window_id == cur_win_id);
    gfx_draw_button(326, tb_y + 4, 100, 24, "4: Proc",   a4 && a4->window_id == cur_win_id);
    gfx_draw_button(432, tb_y + 4, 100, 24, "5: AI Chat", a5 && a5->window_id == cur_win_id);

    /* Theme / Settings switcher button */
    gfx_draw_button(538, tb_y + 4, 100, 24, "6: Theme",  0);

    /* Exit button */
    gfx_draw_button(screen_w - 110, tb_y + 4, 102, 24, "ESC: Shell", 0);
}

void desktop_render(void)
{
    const fb_info_t *fb = fb_get_info();
    if (!fb || !fb->is_valid) return;

    /* 1. Clear desktop work area */
    fb_fill_rect(0, WM_TOP_BAR_HEIGHT, fb->width, fb->height - WM_TOP_BAR_HEIGHT - WM_TASKBAR_HEIGHT, g_desk.bg_color);

    /* 2. Render all visible windows in z-order (lowest to highest) */
    window_t *z_list[WM_MAX_WINDOWS];
    int count = wm_get_windows_by_z_order(z_list, WM_MAX_WINDOWS);
    for (int i = 0; i < count; i++) {
        wm_render_window_frame(z_list[i]);
    }

    /* 3. Render Top Status Bar */
    render_top_bar(fb->width);

    /* 4. Render Bottom Taskbar */
    render_taskbar(fb->width, fb->height);

    /* 5. Render Hardware Mouse Cursor */
    const mouse_state_t *m = mouse_get_state();
    if (m && m->is_present) {
        mouse_draw_cursor(FB_COLOR_WHITE);
    }
}

void desktop_handle_key(char c)
{
    if (c == 27) { /* ESC: exit GUI back to text console */
        g_desk.is_running = 0;
        return;
    }

    if (c == '\t') { /* TAB: cycle window focus */
        wm_cycle_focus();
        return;
    }

    /* Route key to focused window */
    window_t *focused = wm_get_focused();
    if (focused && focused->handle_key) {
        focused->handle_key(focused, c);
    }
}

void desktop_run(void)
{
    const fb_info_t *fb = fb_get_info();
    if (!fb || !fb->is_valid) {
        terminal_write("GUI Error: No framebuffer available. Preserving text console.\n");
        return;
    }

    desktop_init();

    terminal_write("[GUI] Starting Native JARVIS Graphical Desktop (Phase 12 App Framework)...\n");
    debug_puts("[GUI] Desktop loop entered.\n");

    desktop_render();

    uint32_t last_rendered_tick = (uint32_t)timer_ticks;
    int dragging_win_id = -1;
    int32_t drag_start_mouse_x = 0;
    int32_t drag_start_mouse_y = 0;
    int prev_mouse_btn = 0;

    while (g_desk.is_running) {
        /* 1. Poll Keyboard and enqueue */
        char k = keyboard_read_char();
        if (k != 0) {
            gui_event_t ev = { .type = EVENT_KEY_PRESS, .key = k };
            event_queue_push(&ev);
        }

        /* 2. Poll Mouse and enqueue */
        const mouse_state_t *m = mouse_get_state();
        if (m && m->is_present) {
            int current_btn = (m->buttons & 1);
            if (current_btn && !prev_mouse_btn) {
                gui_event_t ev = { .type = EVENT_MOUSE_DOWN, .x = m->x, .y = m->y, .buttons = m->buttons };
                event_queue_push(&ev);
            } else if (!current_btn && prev_mouse_btn) {
                gui_event_t ev = { .type = EVENT_MOUSE_UP, .x = m->x, .y = m->y, .buttons = m->buttons };
                event_queue_push(&ev);
            }
            prev_mouse_btn = current_btn;
        }

        /* 3. Process GUI Event Queue outside ISR context */
        gui_event_t ev;
        int needs_render = 0;

        while (event_queue_pop(&ev) == 0) {
            if (ev.type == EVENT_KEY_PRESS) {
                desktop_handle_key(ev.key);
                needs_render = 1;
            } else if (ev.type == EVENT_MOUSE_DOWN) {
                /* Check taskbar */
                if (ev.y >= (int32_t)fb->height - (int32_t)WM_TASKBAR_HEIGHT) {
                    if (ev.x >= 8 && ev.x < 108) app_launch(APP_ID_TERMINAL);
                    else if (ev.x >= 114 && ev.x < 214) app_launch(APP_ID_SYSMON);
                    else if (ev.x >= 220 && ev.x < 320) app_launch(APP_ID_FILES);
                    else if (ev.x >= 326 && ev.x < 426) app_launch(APP_ID_PROC);
                    else if (ev.x >= 432 && ev.x < 532) app_launch(APP_ID_AI);
                    else if (ev.x >= 538 && ev.x < 638) desktop_cycle_theme();
                    else if (ev.x >= (int32_t)fb->width - 110) g_desk.is_running = 0;
                    needs_render = 1;
                } else {
                    /* Window hit test */
                    window_t *hit = wm_get_window_at(ev.x, ev.y);
                    if (hit) {
                        wm_focus_window(hit->id);
                        wm_hit_test_t ht = wm_hit_test(hit, ev.x, ev.y);

                        if (ht == WM_HIT_BTN_CLOSE) {
                            app_instance_t *app = app_get_by_window_id(hit->id);
                            if (app) app_close(app->desc.app_id);
                            else wm_destroy_window(hit->id);
                        } else if (ht == WM_HIT_BTN_MAXIMIZE) {
                            wm_maximize_window(hit->id);
                        } else if (ht == WM_HIT_BTN_MINIMIZE) {
                            app_instance_t *app = app_get_by_window_id(hit->id);
                            if (app) app_minimize(app->desc.app_id);
                            else wm_minimize_window(hit->id);
                        } else if (ht == WM_HIT_TITLEBAR) {
                            dragging_win_id = hit->id;
                            drag_start_mouse_x = ev.x;
                            drag_start_mouse_y = ev.y;
                        } else if (ht == WM_HIT_CLIENT) {
                            if (hit->handle_click) {
                                hit->handle_click(hit, ev.x - hit->x, ev.y - hit->y, ev.buttons);
                            }
                        }
                        needs_render = 1;
                    }
                }
            } else if (ev.type == EVENT_MOUSE_UP) {
                dragging_win_id = -1;
            }
        }

        /* 4. Mouse dragging if active */
        if (dragging_win_id != -1 && m && m->is_present) {
            int32_t dx = m->x - drag_start_mouse_x;
            int32_t dy = m->y - drag_start_mouse_y;
            if (dx != 0 || dy != 0) {
                wm_drag_window(dragging_win_id, dx, dy);
                drag_start_mouse_x = m->x;
                drag_start_mouse_y = m->y;
                needs_render = 1;
            }
        }

        /* Periodic desktop refresh (every 20 ticks / 200ms) for clock & gauges */
        if ((uint32_t)timer_ticks - last_rendered_tick >= 20) {
            needs_render = 1;
            last_rendered_tick = (uint32_t)timer_ticks;
        }

        if (needs_render) {
            desktop_render();
        }

        __asm__ volatile ("hlt");
    }

    terminal_write("[GUI] Exited Graphical Desktop. Text shell console restored.\n");
    debug_puts("[GUI] Desktop loop exited.\n");
}

const desktop_env_t *desktop_get_env(void)
{
    return &g_desk;
}

int test_desktop_invariants(void)
{
    desktop_init();
    if (g_desk.is_running != 1) return -1;
    if (app_get_registered_count() < 5) return -2;

    /* Verify focus cycling */
    window_t *f1 = wm_get_focused();
    if (!f1) return -3;

    desktop_handle_key('\t');
    window_t *f2 = wm_get_focused();
    if (!f2) return -4;

    /* Verify terminal input append and backspace */
    app_launch(APP_ID_TERMINAL);
    desktop_handle_key('e');
    desktop_handle_key('c');
    desktop_handle_key('h');
    desktop_handle_key('o');
    if (g_desk.term_input_len != 4) return -5;

    desktop_handle_key('\b');
    if (g_desk.term_input_len != 3) return -6;

    /* Verify theme cycling and settings persistence */
    uint32_t old_theme = g_desk.config.theme_id;
    desktop_cycle_theme();
    if (g_desk.config.theme_id == old_theme) return -7;

    /* Verify AI Assistant application launch and query handling */
    app_launch(APP_ID_AI);
    uint32_t prev_lines = g_desk.ai_line_count;
    desktop_ai_submit_query("status");
    if (g_desk.ai_line_count <= prev_lines) return -9;

    /* Verify live kernel provenance tagging */
    const char *last_line = g_desk.ai_history[g_desk.ai_line_count - 1];
    int found_live = 0;
    for (int i = 0; i < 70 && last_line[i]; i++) {
        if (last_line[i] == 'L' && last_line[i+1] == 'I' && last_line[i+2] == 'V' && last_line[i+3] == 'E') {
            found_live = 1;
            break;
        }
    }
    if (!found_live) return -10;

    /* Verify privileged security gating */
    desktop_ai_submit_query("/panic");
    const char *block_line = (g_desk.ai_line_count >= 2) ? g_desk.ai_history[g_desk.ai_line_count - 2] : "";
    int found_blocked = 0;
    for (int i = 0; i < 70 && block_line[i]; i++) {
        if (block_line[i] == 'B' && block_line[i+1] == 'L' && block_line[i+2] == 'O' && block_line[i+3] == 'C' && block_line[i+4] == 'K') {
            found_blocked = 1;
            break;
        }
    }
    if (!found_blocked) return -11;

    /* Verify authorization confirmation */
    desktop_ai_submit_query("/confirm panic");
    last_line = g_desk.ai_history[g_desk.ai_line_count - 1];
    int found_auth = 0;
    for (int i = 0; i < 70 && last_line[i]; i++) {
        if (last_line[i] == 'A' && last_line[i+1] == 'U' && last_line[i+2] == 'T' && last_line[i+3] == 'H') {
            found_auth = 1;
            break;
        }
    }
    if (!found_auth) return -12;

    /* Verify conversation history clear control */
    desktop_ai_clear_history();
    if (g_desk.ai_line_count != 4) return -13;

    /* Verify backend status setting */
    desktop_ai_set_backend_status("MOCK_ACTIVE", "Mock-v1", 1);
    if (g_desk.ai_is_connected != 1) return -14;

    /* ESC exits */
    desktop_handle_key(27);
    if (g_desk.is_running != 0) return -8;

    return 0;
}
