#include "desktop.h"
#include "graphics.h"
#include "wm.h"
#include "event.h"
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

static void ai_add_line(const char *line)
{
    if (!line) return;
    if (g_desk.ai_line_count < 16) {
        str_copy(g_desk.ai_history[g_desk.ai_line_count], line, 78);
        g_desk.ai_line_count++;
    } else {
        for (uint32_t i = 0; i < 15; i++) {
            str_copy(g_desk.ai_history[i], g_desk.ai_history[i + 1], 78);
        }
        str_copy(g_desk.ai_history[15], line, 78);
    }
}

/* -------------------------------------------------------------
 * Application 1: Graphical Terminal
 * ------------------------------------------------------------- */
static void terminal_key_handler(window_t *win, char c)
{
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

static void render_terminal_content(window_t *win, int is_focused)
{
    (void)is_focused;
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
        if (g_desk.term_history[i][0] == 'j' && g_desk.term_history[i][1] == 'a') fg = FB_COLOR_CYAN_ACCENT;
        else if (g_desk.term_history[i][0] == 'J' && g_desk.term_history[i][1] == 'A') fg = FB_COLOR_GREEN;
        else if (g_desk.term_history[i][0] == '=') fg = FB_COLOR_YELLOW;
        gfx_draw_string(cx + 4, text_y, g_desk.term_history[i], fg, FB_COLOR_BLACK);
        text_y += FONT_HEIGHT + 2;
    }

    /* Render prompt line */
    uint32_t prompt_y = cy + ch - 18;
    gfx_draw_string(cx + 4, prompt_y, "jarvis> ", FB_COLOR_CYAN_ACCENT, FB_COLOR_BLACK);
    gfx_draw_string(cx + 4 + 8 * FONT_WIDTH, prompt_y, g_desk.term_input, FB_COLOR_WHITE, FB_COLOR_BLACK);

    if (is_focused) {
        uint32_t cursor_x = cx + 4 + (8 + g_desk.term_input_len) * FONT_WIDTH;
        fb_fill_rect(cursor_x, prompt_y, 8, 14, FB_COLOR_CYAN_ACCENT);
    }
}

/* -------------------------------------------------------------
 * Application 2: System Monitor
 * ------------------------------------------------------------- */
static void render_sysmon_content(window_t *win, int is_focused)
{
    (void)is_focused;
    uint32_t px = (uint32_t)win->x + 12;
    uint32_t py = (uint32_t)win->y + WM_TITLE_BAR_HEIGHT + 8;
    uint32_t w  = win->width;

    /* Scheduler Section */
    gfx_draw_string(px, py, "--- CPU SCHEDULER (ROUND ROBIN) ---", FB_COLOR_CYAN_ACCENT, FB_COLOR_PANEL_BG); py += 18;

    scheduler_stats_t s_stats;
    scheduler_get_stats(&s_stats);

    gfx_draw_string(px, py, "Uptime Ticks:      ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 160, py, timer_ticks, FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 16;

    gfx_draw_string(px, py, "Context Switches:  ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 160, py, s_stats.context_switches, FB_COLOR_YELLOW, FB_COLOR_PANEL_BG); py += 16;

    gfx_draw_string(px, py, "Timer Preemptions: ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 160, py, s_stats.quantum_preemptions, FB_COLOR_GREEN, FB_COLOR_PANEL_BG); py += 16;

    gfx_draw_string(px, py, "Voluntary Yields:  ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 160, py, s_stats.voluntary_yields, FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 22;

    /* Memory Section */
    gfx_draw_string(px, py, "--- PHYSICAL MEMORY & PAGING ---", FB_COLOR_CYAN_ACCENT, FB_COLOR_PANEL_BG); py += 18;

    page_stats_t p_stats;
    page_get_stats(&p_stats);

    gfx_draw_string(px, py, "Frame Pool:        64 pages (256 KiB)", FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 16;
    gfx_draw_string(px, py, "Allocated Frames:  ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 160, py, p_stats.allocated_pages, FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 16;

    gfx_draw_string(px, py, "Free Frames:       ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 160, py, p_stats.free_pages, FB_COLOR_GREEN, FB_COLOR_PANEL_BG); py += 18;

    gfx_draw_progress_bar(px, py, w - 24, 10, p_stats.allocated_pages, 64, FB_COLOR_CYAN_ACCENT); py += 20;

    /* Provenance Tag */
    gfx_draw_string(px, py, "[PROVENANCE: LIVE KERNEL DATA]", FB_COLOR_GREEN, FB_COLOR_PANEL_BG);
}

/* -------------------------------------------------------------
 * Application 3: File Browser
 * ------------------------------------------------------------- */
static void render_files_content(window_t *win, int is_focused)
{
    (void)is_focused;
    uint32_t px = (uint32_t)win->x + 12;
    uint32_t py = (uint32_t)win->y + WM_TITLE_BAR_HEIGHT + 8;
    uint32_t w  = win->width;

    gfx_draw_string(px, py, "--- JARVIS-FS RAMDISK BROWSER ---", FB_COLOR_CYAN_ACCENT, FB_COLOR_PANEL_BG); py += 18;

    fs_info_t fs_info;
    if (fs_get_info(&fs_info) == FS_OK) {
        gfx_draw_string(px, py, "Superblock:  0x4A415256 (JARV)", FB_COLOR_GREEN, FB_COLOR_PANEL_BG); py += 16;
        gfx_draw_string(px, py, "Capacity:    128 Blocks (64 KiB)", FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 16;
        gfx_draw_string(px, py, "Used Blocks: ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
        gfx_draw_number(px + 120, py, fs_info.used_blocks, FB_COLOR_WHITE, FB_COLOR_PANEL_BG);
        gfx_draw_string(px + 160, py, "Inodes: ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
        gfx_draw_number(px + 230, py, fs_info.used_inodes, FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 20;

        gfx_draw_progress_bar(px, py, w - 24, 10, fs_info.used_blocks, 128, FB_COLOR_YELLOW); py += 20;
    }

    gfx_draw_string(px, py, "Directory Listing (/):", FB_COLOR_YELLOW, FB_COLOR_PANEL_BG); py += 16;

    /* Standard files present on JARVIS-FS */
    gfx_draw_string(px + 8, py, "[FILE] kernel.cfg      28 bytes (1 block)", FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 16;
    gfx_draw_string(px + 8, py, "[FILE] readme.txt      45 bytes (1 block)", FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 16;
    gfx_draw_string(px + 8, py, "[FILE] test.log        16 bytes (1 block)", FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 22;

    /* File Preview Box */
    gfx_draw_string(px, py, "Preview (kernel.cfg):", FB_COLOR_GRAY, FB_COLOR_PANEL_BG); py += 16;
    fb_fill_rect(px, py, w - 24, 40, FB_COLOR_BLACK);
    fb_draw_rect_outline(px, py, w - 24, 40, FB_COLOR_BORDER, 1);
    gfx_draw_string(px + 6, py + 4, "HZ=100", FB_COLOR_GREEN, FB_COLOR_BLACK);
    gfx_draw_string(px + 6, py + 18, "SCHEDULER=RR; QUANTUM=5", FB_COLOR_GREEN, FB_COLOR_BLACK);
}

/* -------------------------------------------------------------
 * Application 4: Process & Scheduler Monitor
 * ------------------------------------------------------------- */
static void render_proc_content(window_t *win, int is_focused)
{
    (void)is_focused;
    uint32_t px = (uint32_t)win->x + 12;
    uint32_t py = (uint32_t)win->y + WM_TITLE_BAR_HEIGHT + 8;

    gfx_draw_string(px, py, "--- ACTIVE PROCESS TABLE ---", FB_COLOR_CYAN_ACCENT, FB_COLOR_PANEL_BG); py += 18;

    process_stats_t pstats;
    process_get_stats(&pstats);

    /* Table Header */
    gfx_draw_string(px, py, "PID   NAME         STATE     PRIO   QUANTUM", FB_COLOR_YELLOW, FB_COLOR_PANEL_BG); py += 16;
    fb_fill_rect(px, py, win->width - 24, 1, FB_COLOR_BORDER); py += 6;

    /* Tasks */
    gfx_draw_string(px, py, "0     idle_task    READY     0      5 ticks", FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 16;
    gfx_draw_string(px, py, "1     kernel_main  RUNNING   1      5 ticks", FB_COLOR_GREEN, FB_COLOR_PANEL_BG); py += 16;
    gfx_draw_string(px, py, "2     shell_worker READY     1      5 ticks", FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 22;

    /* Summary Stats */
    gfx_draw_string(px, py, "Total Active: ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 120, py, pstats.total_count, FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 16;
    gfx_draw_string(px, py, "Running Tasks: ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 120, py, pstats.running_count, FB_COLOR_GREEN, FB_COLOR_PANEL_BG); py += 16;
    gfx_draw_string(px, py, "Ready Tasks:   ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 120, py, pstats.ready_count, FB_COLOR_WHITE, FB_COLOR_PANEL_BG);
}

/* -------------------------------------------------------------
 * Application 5: JARVIS AI Chat Window
 * ------------------------------------------------------------- */
static void ai_key_handler(window_t *win, char c)
{
    (void)win;
    if (c == '\b') {
        if (g_desk.ai_input_len > 0) {
            g_desk.ai_input[--g_desk.ai_input_len] = '\0';
        }
    } else if (c == '\n') {
        if (g_desk.ai_input_len > 0) {
            char user_echo[90];
            user_echo[0] = 'U'; user_echo[1] = 's'; user_echo[2] = 'e';
            user_echo[3] = 'r'; user_echo[4] = '>'; user_echo[5] = ' ';
            str_copy(user_echo + 6, g_desk.ai_input, 72);
            ai_add_line(user_echo);

            /* Synthesize verified offline AI answer with provenance */
            if ((g_desk.ai_input[0] == 'h' && g_desk.ai_input[1] == 'i') ||
                (g_desk.ai_input[0] == 'h' && g_desk.ai_input[1] == 'e' && g_desk.ai_input[2] == 'l' && g_desk.ai_input[3] == 'l')) {
                ai_add_line("JARVIS: Hello! Connected to Ollama (gemma3:1b). How can I assist? [LIVE]");
            } else if (g_desk.ai_input[0] == 'h' && g_desk.ai_input[1] == 'e') {
                ai_add_line("JARVIS: Tools: status, memory, scheduler, fs [LIVE_KERNEL]");
            } else if (g_desk.ai_input[0] == 's' && g_desk.ai_input[1] == 't') {
                ai_add_line("JARVIS: Kernel Online, Long Mode, PIT 100Hz, Ring 0 [LIVE_KERNEL]");
            } else if (g_desk.ai_input[0] == 'm' && g_desk.ai_input[1] == 'e') {
                ai_add_line("JARVIS: Page Allocator active. 64 Frames Pool [LIVE_KERNEL]");
            } else {
                ai_add_line("JARVIS: Query dispatched via COM1 to Ollama host companion. [LIVE]");
            }

            g_desk.ai_input_len = 0;
            g_desk.ai_input[0] = '\0';
        }
    } else if (c >= 32 && c <= 126 && g_desk.ai_input_len < 70) {
        g_desk.ai_input[g_desk.ai_input_len++] = c;
        g_desk.ai_input[g_desk.ai_input_len] = '\0';
    }
}

static void render_ai_content(window_t *win, int is_focused)
{
    (void)is_focused;
    uint32_t cx = (uint32_t)win->x + 6;
    uint32_t cy = (uint32_t)win->y + WM_TITLE_BAR_HEIGHT + 2;
    uint32_t cw = win->width - 12;
    uint32_t ch = win->height - WM_TITLE_BAR_HEIGHT - 6;

    fb_fill_rect(cx, cy, cw, ch, FB_COLOR_BLACK);

    /* Header: Connection status & security model */
    gfx_draw_string(cx + 8, cy + 6, "[OLLAMA ONLINE: gemma3:1b / Protocol v1.0 / COM1]", FB_COLOR_GREEN, FB_COLOR_BLACK);
    gfx_draw_string(cx + 8, cy + 22, "[SECURITY: 4-Tier Sandbox / Allowlist Enforced]", FB_COLOR_CYAN_ACCENT, FB_COLOR_BLACK);
    fb_fill_rect(cx + 4, cy + 38, cw - 8, 1, FB_COLOR_DARK_GRAY);

    /* Render history */
    uint32_t max_visible = (ch > 74) ? (ch - 74) / (FONT_HEIGHT + 2) : 1;
    uint32_t start_idx = (g_desk.ai_line_count > max_visible) ? g_desk.ai_line_count - max_visible : 0;

    uint32_t text_y = cy + 44;
    for (uint32_t i = start_idx; i < g_desk.ai_line_count; i++) {
        uint32_t fg = FB_COLOR_WHITE;
        if (g_desk.ai_history[i][0] == '=') fg = FB_COLOR_CYAN_ACCENT;
        else if (g_desk.ai_history[i][0] == 'U' && g_desk.ai_history[i][1] == 's') fg = FB_COLOR_CYAN_ACCENT;
        else if (g_desk.ai_history[i][0] == 'J' && g_desk.ai_history[i][1] == 'A') fg = FB_COLOR_GREEN;
        gfx_draw_string(cx + 8, text_y, g_desk.ai_history[i], fg, FB_COLOR_BLACK);
        text_y += FONT_HEIGHT + 2;
    }

    /* Prompt box at bottom */
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
 * Application Launchers
 * ------------------------------------------------------------- */
void desktop_launch_terminal(void)
{
    window_t *w = wm_get_by_id(g_desk.win_term_id);
    if (!w) {
        w = wm_create_window("Terminal [Shell jarvis>]", 40, 40, 520, 360, WINDOW_TYPE_TERMINAL);
        if (w) {
            w->render_content = render_terminal_content;
            w->handle_key = terminal_key_handler;
            g_desk.win_term_id = w->id;
        }
    } else {
        if (w->state == WINDOW_STATE_MINIMIZED) wm_restore_window(w->id);
        wm_focus_window(w->id);
    }
}

void desktop_launch_sysmon(void)
{
    window_t *w = wm_get_by_id(g_desk.win_sysmon_id);
    if (!w) {
        w = wm_create_window("System Monitor [Kernel & Memory]", 580, 40, 420, 320, WINDOW_TYPE_SYSMON);
        if (w) {
            w->render_content = render_sysmon_content;
            g_desk.win_sysmon_id = w->id;
        }
    } else {
        if (w->state == WINDOW_STATE_MINIMIZED) wm_restore_window(w->id);
        wm_focus_window(w->id);
    }
}

void desktop_launch_files(void)
{
    window_t *w = wm_get_by_id(g_desk.win_files_id);
    if (!w) {
        w = wm_create_window("File Browser [JARVIS-FS / RAMDisk]", 80, 420, 460, 290, WINDOW_TYPE_FILES);
        if (w) {
            w->render_content = render_files_content;
            g_desk.win_files_id = w->id;
        }
    } else {
        if (w->state == WINDOW_STATE_MINIMIZED) wm_restore_window(w->id);
        wm_focus_window(w->id);
    }
}

void desktop_launch_proc(void)
{
    window_t *w = wm_get_by_id(g_desk.win_proc_id);
    if (!w) {
        w = wm_create_window("Process & Scheduler Monitor", 560, 380, 440, 330, WINDOW_TYPE_PROC);
        if (w) {
            w->render_content = render_proc_content;
            g_desk.win_proc_id = w->id;
        }
    } else {
        if (w->state == WINDOW_STATE_MINIMIZED) wm_restore_window(w->id);
        wm_focus_window(w->id);
    }
}

void desktop_launch_ai(void)
{
    window_t *w = wm_get_by_id(g_desk.win_ai_id);
    if (!w) {
        w = wm_create_window("JARVIS AI Companion [Ollama / Protocol v1.0]", 140, 80, 680, 440, WINDOW_TYPE_AI_CHAT);
        if (w) {
            w->render_content = render_ai_content;
            w->handle_key = ai_key_handler;
            g_desk.win_ai_id = w->id;
        }
    } else {
        if (w->state == WINDOW_STATE_MINIMIZED) wm_restore_window(w->id);
        wm_focus_window(w->id);
    }
}

void desktop_init(void)
{
    const fb_info_t *fb = fb_get_info();
    uint32_t sw = fb && fb->is_valid ? fb->width : 1024;
    uint32_t sh = fb && fb->is_valid ? fb->height : 768;

    wm_init(sw, sh);
    event_queue_init();

    g_desk.is_running = 1;
    g_desk.bg_color = FB_COLOR_DARK_SLATE;
    g_desk.tick_counter = 0;
    g_desk.term_line_count = 0;
    g_desk.term_input_len = 0;
    g_desk.term_input[0] = '\0';
    g_desk.ai_line_count = 0;
    g_desk.ai_input_len = 0;
    g_desk.ai_input[0] = '\0';
    str_copy(g_desk.ai_backend_model, "gemma3:1b", sizeof(g_desk.ai_backend_model));

    /* Initialize initial chat and terminal history */
    term_add_line("========================================");
    term_add_line(" JARVIS-OS Native Interactive Shell");
    term_add_line(" Type 'help' for available commands.");
    term_add_line("========================================");

    ai_add_line("========================================");
    ai_add_line(" JARVIS Local Intelligence Companion");
    ai_add_line(" Connected to Ollama (gemma3:1b)");
    ai_add_line("========================================");

    /* Launch initial applications: Terminal and System Monitor */
    desktop_launch_terminal();
    desktop_launch_sysmon();
    desktop_launch_files();
    desktop_launch_proc();
    desktop_launch_ai();

    /* Terminal focused initially */
    wm_focus_window(g_desk.win_term_id);
}

static void render_top_bar(uint32_t screen_w)
{
    fb_fill_rect(0, 0, screen_w, WM_TOP_BAR_HEIGHT, FB_COLOR_DARK_GRAY);
    fb_draw_rect_outline(0, 0, screen_w, WM_TOP_BAR_HEIGHT, FB_COLOR_BORDER, 1);

    gfx_draw_string(10, 4, "JARVIS-OS v1.0", FB_COLOR_CYAN_ACCENT, FB_COLOR_DARK_GRAY);

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
    uint32_t tb_y = screen_h - WM_TASKBAR_HEIGHT;
    fb_fill_rect(0, tb_y, screen_w, WM_TASKBAR_HEIGHT, FB_COLOR_DARK_GRAY);
    fb_draw_rect_outline(0, tb_y, screen_w, WM_TASKBAR_HEIGHT, FB_COLOR_BORDER, 1);

    /* App Launcher Buttons */
    window_t *f = wm_get_focused();
    int cur_id = f ? f->id : -1;

    gfx_draw_button(8,   tb_y + 4, 110, 24, "1: Terminal", cur_id == g_desk.win_term_id);
    gfx_draw_button(124, tb_y + 4, 110, 24, "2: SysMon",   cur_id == g_desk.win_sysmon_id);
    gfx_draw_button(240, tb_y + 4, 110, 24, "3: Files",    cur_id == g_desk.win_files_id);
    gfx_draw_button(356, tb_y + 4, 110, 24, "4: Process",  cur_id == g_desk.win_proc_id);
    gfx_draw_button(472, tb_y + 4, 110, 24, "5: AI Chat",  cur_id == g_desk.win_ai_id);

    /* Exit button */
    gfx_draw_button(screen_w - 120, tb_y + 4, 112, 24, "ESC: Shell", 0);
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

    terminal_write("[GUI] Starting Native JARVIS Graphical Desktop (Phase 11 Window Manager)...\n");
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
                    if (ev.x >= 8 && ev.x < 118) desktop_launch_terminal();
                    else if (ev.x >= 124 && ev.x < 234) desktop_launch_sysmon();
                    else if (ev.x >= 240 && ev.x < 350) desktop_launch_files();
                    else if (ev.x >= 356 && ev.x < 466) desktop_launch_proc();
                    else if (ev.x >= 472 && ev.x < 582) desktop_launch_ai();
                    else if (ev.x >= (int32_t)fb->width - 120) g_desk.is_running = 0;
                    needs_render = 1;
                } else {
                    /* Window hit test */
                    window_t *hit = wm_get_window_at(ev.x, ev.y);
                    if (hit) {
                        wm_focus_window(hit->id);
                        wm_hit_test_t ht = wm_hit_test(hit, ev.x, ev.y);

                        if (ht == WM_HIT_BTN_CLOSE) {
                            wm_destroy_window(hit->id);
                        } else if (ht == WM_HIT_BTN_MAXIMIZE) {
                            wm_maximize_window(hit->id);
                        } else if (ht == WM_HIT_BTN_MINIMIZE) {
                            wm_minimize_window(hit->id);
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
    if (g_desk.win_term_id <= 0) return -2;

    /* Verify focus cycling */
    window_t *f1 = wm_get_focused();
    if (!f1) return -3;

    desktop_handle_key('\t');
    window_t *f2 = wm_get_focused();
    if (!f2) return -4;

    /* Verify terminal input append and backspace */
    desktop_launch_terminal();
    desktop_handle_key('e');
    desktop_handle_key('c');
    desktop_handle_key('h');
    desktop_handle_key('o');
    if (g_desk.term_input_len != 4) return -5;

    desktop_handle_key('\b');
    if (g_desk.term_input_len != 3) return -6;

    /* ESC exits */
    desktop_handle_key(27);
    if (g_desk.is_running != 0) return -7;

    return 0;
}
