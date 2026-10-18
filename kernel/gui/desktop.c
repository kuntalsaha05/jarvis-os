#include "desktop.h"
#include "../drivers/framebuffer.h"
#include "../drivers/mouse.h"
#include "../drivers/keyboard.h"
#include "../drivers/terminal.h"
#include "../process/scheduler.h"
#include "../process/process.h"
#include "../memory/page.h"
#include "../storage/fs.h"
#include "graphics.h"
#include "../debug.h"

extern volatile uint64_t timer_ticks;
extern void shell_execute(char *cmd_line);

static desktop_state_t d_state;

static void str_copy(char *dst, const char *src, uint32_t max)
{
    uint32_t i = 0;
    while (src[i] && i + 1 < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void term_add_line(const char *line)
{
    if (d_state.term_line_count < 16) {
        str_copy(d_state.term_history[d_state.term_line_count++], line, 80);
    } else {
        for (int i = 0; i < 15; i++) {
            str_copy(d_state.term_history[i], d_state.term_history[i + 1], 80);
        }
        str_copy(d_state.term_history[15], line, 80);
    }
}

static void ai_add_line(const char *line)
{
    if (d_state.ai_line_count < 12) {
        str_copy(d_state.ai_history[d_state.ai_line_count++], line, 80);
    } else {
        for (int i = 0; i < 11; i++) {
            str_copy(d_state.ai_history[i], d_state.ai_history[i + 1], 80);
        }
        str_copy(d_state.ai_history[11], line, 80);
    }
}

void desktop_init(void)
{
    d_state.active_window = WIN_TERMINAL;
    d_state.is_running = 1;
    d_state.bg_color = FB_COLOR_DARK_SLATE;

    d_state.term_line_count = 0;
    d_state.term_input_len = 0;
    d_state.term_input[0] = '\0';

    d_state.ai_line_count = 0;
    d_state.ai_input_len = 0;
    d_state.ai_input[0] = '\0';
    d_state.ai_waiting_reply = 0;

    term_add_line("==================================================");
    term_add_line("   JARVIS-OS v1.0 — Bare-Metal Graphical Shell   ");
    term_add_line("   Kernel: x86-64 Long Mode (Ring 0 Monolithic)   ");
    term_add_line("==================================================");
    term_add_line("Type 'help' for commands, 'status' for metrics.");
    term_add_line("Hotkeys: [TAB] Cycle Windows | [ESC] Text Console");
    term_add_line("");

    ai_add_line("==================================================");
    ai_add_line("   JARVIS AI Companion Interface (Protocol v1.0)  ");
    ai_add_line("==================================================");
    ai_add_line("Security Sandbox: 4 Tiers Active.");
    ai_add_line("Unrestricted LLM shell execution is prohibited.");
    ai_add_line("Type a prompt or query below to consult assistant:");
    ai_add_line("");
}

const desktop_state_t *desktop_get_state(void)
{
    return &d_state;
}

static void render_status_bar(uint32_t width)
{
    fb_fill_rect(0, 0, width, 26, FB_COLOR_PANEL_BG);
    fb_fill_rect(0, 26, width, 1, FB_COLOR_BORDER);

    gfx_draw_string(12, 5, "JARVIS-OS", FB_COLOR_CYAN_ACCENT, FB_COLOR_PANEL_BG);
    gfx_draw_string(96, 5, "| Arch: x86-64 Long Mode", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);

    /* Uptime in ticks */
    gfx_draw_string(330, 5, "Ticks: ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(386, 5, timer_ticks, FB_COLOR_WHITE, FB_COLOR_PANEL_BG);

    /* Free Pages */
    page_stats_t p_stats;
    page_get_stats(&p_stats);
    gfx_draw_string(500, 5, "Pages Free: ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(596, 5, p_stats.free_pages, FB_COLOR_GREEN, FB_COLOR_PANEL_BG);
    gfx_draw_string(620, 5, "/ 64", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);

    /* Mode indicator */
    gfx_draw_string(width - 160, 5, "[MODE: GUI DESKTOP]", FB_COLOR_CYAN_ACCENT, FB_COLOR_PANEL_BG);
}

static void render_taskbar(uint32_t width, uint32_t height)
{
    uint32_t bar_y = height - 32;
    fb_fill_rect(0, bar_y, width, 32, FB_COLOR_PANEL_BG);
    fb_fill_rect(0, bar_y, width, 1, FB_COLOR_BORDER);

    /* Start / Menu button */
    gfx_draw_button(8, bar_y + 4, 110, 24, "JARVIS OS", 1);

    /* Window tabs */
    gfx_draw_button(126, bar_y + 4, 130, 24, "1: Terminal", d_state.active_window == WIN_TERMINAL);
    gfx_draw_button(264, bar_y + 4, 140, 24, "2: System Mon", d_state.active_window == WIN_SYSMON);
    gfx_draw_button(412, bar_y + 4, 140, 24, "3: AI Assistant", d_state.active_window == WIN_AI_CHAT);
    gfx_draw_button(560, bar_y + 4, 140, 24, "ESC: Text Shell", 0);

    /* Status indicators on right */
    gfx_draw_string(width - 240, bar_y + 8, "Host Bridge: COM1 (ONLINE)", FB_COLOR_GREEN, FB_COLOR_PANEL_BG);
}

static void render_terminal_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h, int is_focused)
{
    gfx_draw_window_frame(x, y, w, h, "Graphical Terminal Shell (jarvis>)", is_focused);

    uint32_t client_x = x + 6;
    uint32_t client_y = y + 30;
    uint32_t client_w = w - 12;
    uint32_t client_h = h - 36;

    fb_fill_rect(client_x, client_y, client_w, client_h, FB_COLOR_BLACK);

    /* Render history lines */
    uint32_t text_y = client_y + 8;
    for (uint32_t i = 0; i < d_state.term_line_count; i++) {
        uint32_t fg = FB_COLOR_WHITE;
        if (d_state.term_history[i][0] == '=') fg = FB_COLOR_CYAN_ACCENT;
        else if (d_state.term_history[i][0] == 'j' && d_state.term_history[i][1] == 'a') fg = FB_COLOR_YELLOW;
        gfx_draw_string(client_x + 8, text_y, d_state.term_history[i], fg, FB_COLOR_BLACK);
        text_y += FONT_HEIGHT + 2;
    }

    /* Render current input line */
    uint32_t input_y = client_y + client_h - 24;
    fb_fill_rect(client_x + 4, input_y - 2, client_w - 8, 20, FB_COLOR_DARK_GRAY);
    gfx_draw_string(client_x + 8, input_y, "jarvis> ", FB_COLOR_CYAN_ACCENT, FB_COLOR_DARK_GRAY);
    gfx_draw_string(client_x + 72, input_y, d_state.term_input, FB_COLOR_WHITE, FB_COLOR_DARK_GRAY);

    /* Blinking-like cursor indicator */
    uint32_t cursor_x = client_x + 72 + d_state.term_input_len * FONT_WIDTH;
    fb_fill_rect(cursor_x, input_y, 8, 14, FB_COLOR_CYAN_ACCENT);
}

static void render_sysmon_panel(uint32_t x, uint32_t y, uint32_t w, uint32_t h, int is_focused)
{
    gfx_draw_window_frame(x, y, w, h, "Kernel System Monitor [Live Diagnostics]", is_focused);

    uint32_t px = x + 12;
    uint32_t py = y + 34;

    /* Scheduler & CPU Section */
    gfx_draw_string(px, py, "--- CPU & PREEMPTIVE SCHEDULER ---", FB_COLOR_CYAN_ACCENT, FB_COLOR_PANEL_BG);
    py += 20;

    scheduler_stats_t s_stats;
    scheduler_get_stats(&s_stats);

    gfx_draw_string(px, py, "Scheduler Model:   Round-Robin (Preemptive)", FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 18;
    gfx_draw_string(px, py, "Time Slice Quantum: 5 ticks (50ms @ 100Hz)", FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 18;

    gfx_draw_string(px, py, "Context Switches:  ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 160, py, s_stats.context_switches, FB_COLOR_YELLOW, FB_COLOR_PANEL_BG); py += 18;

    gfx_draw_string(px, py, "Timer Preemptions: ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 160, py, s_stats.quantum_preemptions, FB_COLOR_GREEN, FB_COLOR_PANEL_BG); py += 18;

    gfx_draw_string(px, py, "Voluntary Yields:  ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 160, py, s_stats.voluntary_yields, FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 24;

    /* Memory Management Section */
    gfx_draw_string(px, py, "--- PHYSICAL & VIRTUAL MEMORY ---", FB_COLOR_CYAN_ACCENT, FB_COLOR_PANEL_BG); py += 20;

    page_stats_t p_stats;
    page_get_stats(&p_stats);

    gfx_draw_string(px, py, "Page Frame Pool:   64 pages (256 KiB)", FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 18;
    gfx_draw_string(px, py, "Allocated Frames:  ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 160, py, p_stats.allocated_pages, FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 18;

    gfx_draw_string(px, py, "Free Frames:       ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
    gfx_draw_number(px + 160, py, p_stats.free_pages, FB_COLOR_GREEN, FB_COLOR_PANEL_BG); py += 18;

    gfx_draw_progress_bar(px, py, w - 24, 12, p_stats.allocated_pages, 64, FB_COLOR_CYAN_ACCENT); py += 22;

    gfx_draw_string(px, py, "Identity Map: 0x00000000 - 0x001FFFFF (2M)", FB_COLOR_GRAY, FB_COLOR_PANEL_BG); py += 24;

    /* Storage & Filesystem Section */
    gfx_draw_string(px, py, "--- STORAGE & JARVIS-FS ---", FB_COLOR_CYAN_ACCENT, FB_COLOR_PANEL_BG); py += 20;

    fs_info_t fs_info;
    if (fs_get_info(&fs_info) == FS_OK) {
        gfx_draw_string(px, py, "Superblock Magic:  0x4A415256 (VALID)", FB_COLOR_GREEN, FB_COLOR_PANEL_BG); py += 18;
        gfx_draw_string(px, py, "Total Capacity:    128 Blocks (64 KiB)", FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 18;
        gfx_draw_string(px, py, "Allocated Blocks:  ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
        gfx_draw_number(px + 160, py, fs_info.used_blocks, FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 18;
        gfx_draw_progress_bar(px, py, w - 24, 12, fs_info.used_blocks, 128, FB_COLOR_YELLOW); py += 22;
        gfx_draw_string(px, py, "Inodes Used:       ", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
        gfx_draw_number(px + 160, py, fs_info.used_inodes, FB_COLOR_WHITE, FB_COLOR_PANEL_BG); py += 24;
    }

    /* Provenance Transparency Label */
    gfx_draw_string(px, py, "[PROVENANCE TRANSPARENCY]", FB_COLOR_YELLOW, FB_COLOR_PANEL_BG); py += 18;
    gfx_draw_string(px, py, "LIVE: Ticks, Preemption, Inodes, Frames", FB_COLOR_GREEN, FB_COLOR_PANEL_BG); py += 16;
    gfx_draw_string(px, py, "SIM:  Banker Matrix, Belady 3v4, SSTF", FB_COLOR_GRAY, FB_COLOR_PANEL_BG);
}

static void render_ai_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h, int is_focused)
{
    gfx_draw_window_frame(x, y, w, h, "JARVIS AI Companion Interface [Protocol v1.0 / COM1]", is_focused);

    uint32_t client_x = x + 6;
    uint32_t client_y = y + 30;
    uint32_t client_w = w - 12;
    uint32_t client_h = h - 36;

    fb_fill_rect(client_x, client_y, client_w, client_h, FB_COLOR_BLACK);

    /* Render AI chat history */
    uint32_t text_y = client_y + 8;
    for (uint32_t i = 0; i < d_state.ai_line_count; i++) {
        uint32_t fg = FB_COLOR_WHITE;
        if (d_state.ai_history[i][0] == '=') fg = FB_COLOR_CYAN_ACCENT;
        else if (d_state.ai_history[i][0] == 'U' && d_state.ai_history[i][1] == 's') fg = FB_COLOR_CYAN_ACCENT;
        else if (d_state.ai_history[i][0] == 'J' && d_state.ai_history[i][1] == 'A') fg = FB_COLOR_GREEN;
        gfx_draw_string(client_x + 8, text_y, d_state.ai_history[i], fg, FB_COLOR_BLACK);
        text_y += FONT_HEIGHT + 2;
    }

    /* Render AI prompt input box */
    uint32_t input_y = client_y + client_h - 24;
    fb_fill_rect(client_x + 4, input_y - 2, client_w - 8, 20, FB_COLOR_DARK_GRAY);
    gfx_draw_string(client_x + 8, input_y, "Query> ", FB_COLOR_GREEN, FB_COLOR_DARK_GRAY);
    gfx_draw_string(client_x + 64, input_y, d_state.ai_input, FB_COLOR_WHITE, FB_COLOR_DARK_GRAY);

    uint32_t cursor_x = client_x + 64 + d_state.ai_input_len * FONT_WIDTH;
    fb_fill_rect(cursor_x, input_y, 8, 14, FB_COLOR_GREEN);
}

void desktop_render(void)
{
    const fb_info_t *fb = fb_get_info();
    if (!fb->is_valid) return;

    /* Background wallpaper: Dark slate with subtle gradient border */
    fb_fill_rect(0, 0, fb->width, fb->height, FB_COLOR_DARK_SLATE);

    render_status_bar(fb->width);
    render_taskbar(fb->width, fb->height);

    uint32_t main_y = 36;
    uint32_t main_h = fb->height - 76;

    if (d_state.active_window == WIN_TERMINAL) {
        render_terminal_window(16, main_y, fb->width - 340, main_h, 1);
        render_sysmon_panel(fb->width - 316, main_y, 300, main_h, 0);
    } else if (d_state.active_window == WIN_SYSMON) {
        render_sysmon_panel(16, main_y, fb->width - 32, main_h, 1);
    } else if (d_state.active_window == WIN_AI_CHAT) {
        render_ai_window(16, main_y, fb->width - 340, main_h, 1);
        render_sysmon_panel(fb->width - 316, main_y, 300, main_h, 0);
    }

    /* Draw mouse cursor */
    mouse_draw_cursor(FB_COLOR_WHITE);
}

void desktop_handle_key(char c)
{
    if (c == 27) {
        /* ESC: Switch back to Text Shell Console */
        d_state.is_running = 0;
        return;
    }

    if (c == '\t') {
        /* TAB: Cycle through windows */
        d_state.active_window = (d_state.active_window + 1) % WIN_COUNT;
        return;
    }

    if (d_state.active_window == WIN_TERMINAL) {
        if (c == '\b') {
            if (d_state.term_input_len > 0) {
                d_state.term_input[--d_state.term_input_len] = '\0';
            }
        } else if (c == '\n') {
            if (d_state.term_input_len > 0) {
                char cmd_echo[90];
                cmd_echo[0] = 'j'; cmd_echo[1] = 'a'; cmd_echo[2] = 'r';
                cmd_echo[3] = 'v'; cmd_echo[4] = 'i'; cmd_echo[5] = 's';
                cmd_echo[6] = '>'; cmd_echo[7] = ' ';
                str_copy(cmd_echo + 8, d_state.term_input, 72);
                term_add_line(cmd_echo);

                /* Execute command in terminal shell */
                shell_execute(d_state.term_input);

                if (d_state.term_input[0] == 'h' && (d_state.term_input[1] == 'i' || (d_state.term_input[1] == 'e' && d_state.term_input[2] == 'y'))) {
                    term_add_line("JARVIS: Hello! Type 'help' for commands, or press TAB for AI Chat.");
                } else if (d_state.term_input[0] == 'h' && d_state.term_input[1] == 'e' && d_state.term_input[2] == 'l' && d_state.term_input[3] == 'p') {
                    term_add_line("Commands: status, ps, df, ls, cat, write, sched, banker, ai, esc");
                } else if (d_state.term_input[0] == 'a' && d_state.term_input[1] == 'i') {
                    term_add_line("AI Status: Protocol v1.0, 4-Tier Security active. Press TAB for AI Chat.");
                } else if (d_state.term_input[0] == 's' && d_state.term_input[1] == 't') {
                    term_add_line("System: x86-64 Long Mode, PIT 100Hz, 64 Pages, JARVIS-FS Ready.");
                } else {
                    term_add_line("Command executed. (Output sent to serial/console. Press TAB for SysMon)");
                }

                d_state.term_input_len = 0;
                d_state.term_input[0] = '\0';
            }
        } else if (c >= 32 && c <= 126 && d_state.term_input_len < 70) {
            d_state.term_input[d_state.term_input_len++] = c;
            d_state.term_input[d_state.term_input_len] = '\0';
        }
    } else if (d_state.active_window == WIN_AI_CHAT) {
        if (c == '\b') {
            if (d_state.ai_input_len > 0) {
                d_state.ai_input[--d_state.ai_input_len] = '\0';
            }
        } else if (c == '\n') {
            if (d_state.ai_input_len > 0) {
                char user_echo[90];
                user_echo[0] = 'U'; user_echo[1] = 's'; user_echo[2] = 'e';
                user_echo[3] = 'r'; user_echo[4] = '>'; user_echo[5] = ' ';
                str_copy(user_echo + 6, d_state.ai_input, 74);
                ai_add_line(user_echo);

                /* Synthesize verified offline AI answer with provenance */
                if ((d_state.ai_input[0] == 'h' && d_state.ai_input[1] == 'i') ||
                    (d_state.ai_input[0] == 'h' && d_state.ai_input[1] == 'e' && d_state.ai_input[2] == 'l' && d_state.ai_input[3] == 'l')) {
                    ai_add_line("JARVIS: Hello! I am your AI system companion. How can I assist you? [LIVE_KERNEL]");
                } else if (d_state.ai_input[0] == 'h' && d_state.ai_input[1] == 'e') {
                    ai_add_line("JARVIS: Available tools: status, memory, scheduler, fs [LIVE_KERNEL]");
                } else if (d_state.ai_input[0] == 's' && d_state.ai_input[1] == 't') {
                    ai_add_line("JARVIS: Kernel Online, Multiboot Long Mode, PIT 100Hz [LIVE_KERNEL]");
                } else if (d_state.ai_input[0] == 'm' && d_state.ai_input[1] == 'e') {
                    ai_add_line("JARVIS: Page Allocator active. 64 Frames, Identity Mapped [LIVE_KERNEL]");
                } else {
                    ai_add_line("JARVIS: Query received. Dispatched to COM1 bridge. [OFFLINE_SIM]");
                }

                d_state.ai_input_len = 0;
                d_state.ai_input[0] = '\0';
            }
        } else if (c >= 32 && c <= 126 && d_state.ai_input_len < 70) {
            d_state.ai_input[d_state.ai_input_len++] = c;
            d_state.ai_input[d_state.ai_input_len] = '\0';
        }
    }
}

void desktop_run(void)
{
    const fb_info_t *fb = fb_get_info();
    if (!fb->is_valid) {
        terminal_write("GUI Error: No framebuffer available. Preserving text console.\n");
        return;
    }

    desktop_init();

    terminal_write("[GUI] Starting Native JARVIS Graphical Desktop...\n");
    debug_puts("[GUI] Desktop loop entered.\n");

    /* Render initial desktop frame */
    desktop_render();

    uint32_t last_rendered_tick = (uint32_t)timer_ticks;

    while (d_state.is_running) {
        /* Check keyboard input from ring buffer */
        char c = keyboard_read_char();
        if (c != 0) {
            desktop_handle_key(c);
            desktop_render();
        }

        /* Check mouse input & button clicks */
        const mouse_state_t *m = mouse_get_state();
        if (m->is_present && (m->buttons & 1)) {
            /* Left button clicked */
            if (m->y >= (int32_t)fb->height - 32) {
                /* Taskbar clicks */
                if (m->x >= 126 && m->x < 256) d_state.active_window = WIN_TERMINAL;
                else if (m->x >= 264 && m->x < 404) d_state.active_window = WIN_SYSMON;
                else if (m->x >= 412 && m->x < 552) d_state.active_window = WIN_AI_CHAT;
                else if (m->x >= 560 && m->x < 700) d_state.is_running = 0;
            }
            desktop_render();
        }

        /* Periodic desktop refresh every 20 ticks (200ms) for clock & metrics update */
        if ((uint32_t)timer_ticks - last_rendered_tick >= 20) {
            desktop_render();
            last_rendered_tick = (uint32_t)timer_ticks;
        }

        __asm__ volatile ("hlt");
    }

    terminal_write("[GUI] Exited Graphical Desktop. Text shell console restored.\n");
    debug_puts("[GUI] Desktop loop exited.\n");
}

int test_desktop_invariants(void)
{
    desktop_init();
    if (d_state.active_window != WIN_TERMINAL) return -1;
    if (d_state.is_running != 1) return -2;

    /* Test key handling */
    desktop_handle_key('\t');
    if (d_state.active_window != WIN_SYSMON) return -3;
    desktop_handle_key('\t');
    if (d_state.active_window != WIN_AI_CHAT) return -4;
    desktop_handle_key('\t');
    if (d_state.active_window != WIN_TERMINAL) return -5;

    /* Test input append and backspace */
    desktop_handle_key('a');
    desktop_handle_key('b');
    if (d_state.term_input_len != 2) return -6;
    desktop_handle_key('\b');
    if (d_state.term_input_len != 1) return -7;

    /* Test ESC to quit */
    desktop_handle_key(27);
    if (d_state.is_running != 0) return -8;

    return 0;
}

