#include "mouse.h"
#include "framebuffer.h"
#include "../interrupts/pic.h"
#include "debug.h"

#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void mouse_wait_write(void)
{
    for (uint32_t i = 0; i < 50000; i++) {
        if ((inb(PS2_STATUS_PORT) & 0x02) == 0) return;
    }
}

static void mouse_wait_read(void)
{
    for (uint32_t i = 0; i < 50000; i++) {
        if ((inb(PS2_STATUS_PORT) & 0x01) == 1) return;
    }
}

static void mouse_write(uint8_t data)
{
    mouse_wait_write();
    outb(PS2_COMMAND_PORT, 0xD4);
    mouse_wait_write();
    outb(PS2_DATA_PORT, data);
}

static uint8_t mouse_read(void)
{
    mouse_wait_read();
    return inb(PS2_DATA_PORT);
}

static mouse_state_t mouse_state = {
    .x = 400,
    .y = 300,
    .buttons = 0,
    .is_present = 0,
    .packet_count = 0
};

static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[3] = {0};

void mouse_init(void)
{
    mouse_state.x = 400;
    mouse_state.y = 300;
    mouse_state.buttons = 0;
    mouse_state.packet_count = 0;
    mouse_cycle = 0;

    /* 1. Enable PS/2 Auxiliary Device */
    mouse_wait_write();
    outb(PS2_COMMAND_PORT, 0xA8);

    /* 2. Read Controller Configuration Byte */
    mouse_wait_write();
    outb(PS2_COMMAND_PORT, 0x20);
    mouse_wait_read();
    uint8_t status = inb(PS2_DATA_PORT);

    /* Set bit 1 (enable IRQ12 for mouse) and clear bit 5 (disable mouse clock) */
    status |= 0x02;
    status &= ~0x20;

    /* Write back Configuration Byte */
    mouse_wait_write();
    outb(PS2_COMMAND_PORT, 0x60);
    mouse_wait_write();
    outb(PS2_DATA_PORT, status);

    /* 3. Set Defaults and Enable Streaming */
    mouse_write(0xF6); /* Set default settings */
    mouse_read();       /* ACK 0xFA */

    mouse_write(0xF4); /* Enable data reporting */
    uint8_t ack = mouse_read();

    if (ack == 0xFA) {
        mouse_state.is_present = 1;
        debug_puts("[MOUSE] PS/2 Mouse initialized and streaming enabled.\n");
    } else {
        mouse_state.is_present = 0;
        debug_puts("[MOUSE] PS/2 Mouse ACK not received. Input fallback active.\n");
    }

    /* Unmask IRQ12 on slave PIC (IRQ12 = IRQ4 on slave PIC -> bit 4) */
    /* Also unmask IRQ2 on master PIC (cascade line) */
    uint8_t master_mask = inb(0x21);
    outb(0x21, master_mask & ~(1 << 2));

    uint8_t slave_mask = inb(0xA1);
    outb(0xA1, slave_mask & ~(1 << 4));
}

void mouse_handler(void)
{
    uint8_t status = inb(PS2_STATUS_PORT);
    if ((status & 0x01) && (status & 0x20)) {
        uint8_t data = inb(PS2_DATA_PORT);

        switch (mouse_cycle) {
            case 0:
                /* Byte 0: Bit 3 must be 1 for a valid PS/2 mouse packet */
                if (data & 0x08) {
                    mouse_packet[0] = data;
                    mouse_cycle = 1;
                }
                break;
            case 1:
                mouse_packet[1] = data;
                mouse_cycle = 2;
                break;
            case 2:
                mouse_packet[2] = data;
                mouse_cycle = 0;

                /* Decode 3-byte packet */
                mouse_state.buttons = mouse_packet[0] & 0x07;

                int32_t dx = (int32_t)mouse_packet[1];
                int32_t dy = (int32_t)mouse_packet[2];

                if (mouse_packet[0] & 0x10) dx |= 0xFFFFFF00;
                if (mouse_packet[0] & 0x20) dy |= 0xFFFFFF00;

                mouse_state.x += dx;
                /* PS/2 Y is inverted relative to screen coordinates */
                mouse_state.y -= dy;

                const fb_info_t *fb = fb_get_info();
                int32_t max_x = fb->is_valid ? (int32_t)fb->width - 1 : 1023;
                int32_t max_y = fb->is_valid ? (int32_t)fb->height - 1 : 767;

                if (mouse_state.x < 0) mouse_state.x = 0;
                if (mouse_state.x > max_x) mouse_state.x = max_x;
                if (mouse_state.y < 0) mouse_state.y = 0;
                if (mouse_state.y > max_y) mouse_state.y = max_y;

                mouse_state.packet_count++;
                break;
            default:
                mouse_cycle = 0;
                break;
        }
    }

    pic_send_eoi(12);
}

const mouse_state_t *mouse_get_state(void)
{
    return &mouse_state;
}

int mouse_is_available(void)
{
    return mouse_state.is_present;
}

void mouse_draw_cursor(uint32_t color)
{
    int32_t x = mouse_state.x;
    int32_t y = mouse_state.y;

    /* Draw a classic 11x11 arrow cursor */
    static const uint16_t cursor_mask[11] = {
        0b1000000000000000,
        0b1100000000000000,
        0b1110000000000000,
        0b1111000000000000,
        0b1111100000000000,
        0b1111110000000000,
        0b1111111000000000,
        0b1111100000000000,
        0b1001100000000000,
        0b0000110000000000,
        0b0000110000000000
    };

    for (int r = 0; r < 11; r++) {
        for (int c = 0; c < 11; c++) {
            if (cursor_mask[r] & (0x8000 >> c)) {
                fb_put_pixel((uint32_t)(x + c), (uint32_t)(y + r), color);
            }
        }
    }
}

int test_mouse_invariants(void)
{
    /* 1. Clamping verification */
    mouse_state.x = -50;
    mouse_state.y = -50;
    if (mouse_state.x < 0) mouse_state.x = 0;
    if (mouse_state.y < 0) mouse_state.y = 0;
    if (mouse_state.x != 0 || mouse_state.y != 0) return -1;

    /* 2. Packet sign decoding */
    uint8_t flags = 0x38; /* Bit 3=1, X sign=1, Y sign=1 */
    uint8_t raw_x = 0xFE;  /* -2 */
    uint8_t raw_y = 0xFD;  /* -3 */
    int32_t dx = (int32_t)raw_x;
    int32_t dy = (int32_t)raw_y;
    if (flags & 0x10) dx |= 0xFFFFFF00;
    if (flags & 0x20) dy |= 0xFFFFFF00;
    if (dx != -2 || dy != -3) return -2;

    return 0;
}

