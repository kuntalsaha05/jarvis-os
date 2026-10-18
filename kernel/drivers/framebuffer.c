#include "framebuffer.h"
#include "../memory/multiboot.h"
#include "terminal.h"
#include "debug.h"

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

#define VBE_DISPI_INDEX_ID          0
#define VBE_DISPI_INDEX_XRES        1
#define VBE_DISPI_INDEX_YRES        2
#define VBE_DISPI_INDEX_BPP         3
#define VBE_DISPI_INDEX_ENABLE      4
#define VBE_DISPI_INDEX_BANK        5
#define VBE_DISPI_INDEX_VIRT_WIDTH  6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 7
#define VBE_DISPI_INDEX_X_OFFSET    8
#define VBE_DISPI_INDEX_Y_OFFSET    9

#define VBE_DISPI_DISABLED          0x00
#define VBE_DISPI_ENABLED           0x01
#define VBE_DISPI_LFB_ENABLED       0x40
#define VBE_DISPI_NOCLEARMEM        0x80

#define PCI_CONFIG_ADDRESS          0x0CF8
#define PCI_CONFIG_DATA             0x0CFC

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

static inline void outw(uint16_t port, uint16_t val)
{
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val)
{
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void bga_write(uint16_t index, uint16_t data)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, data);
}

static uint16_t bga_read(uint16_t index)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

static uint32_t pci_read_config_32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
{
    uint32_t address = 0x80000000 | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) |
                       ((uint32_t)func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static fb_info_t fb_state = {0};

static void str_copy(char *dst, const char *src, uint32_t max)
{
    uint32_t i = 0;
    while (src[i] && i + 1 < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void fb_init(uint64_t magic, uint64_t mb_info_addr)
{
    fb_state.is_valid = 0;
    fb_state.phys_addr = 0;
    fb_state.buffer = NULL;
    fb_state.width = 0;
    fb_state.height = 0;
    fb_state.pitch = 0;
    fb_state.bpp = 0;
    fb_state.type = 0;
    fb_state.method[0] = '\0';

    const multiboot_info_t *mbi = NULL;
    if ((uint32_t)magic == MULTIBOOT_BOOTLOADER_MAGIC && mb_info_addr != 0) {
        mbi = (const multiboot_info_t *)(uintptr_t)mb_info_addr;
    }

    /* Method 1: Multiboot Framebuffer Table (Bit 12) */
    if (mbi && (mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)) {
        if (mbi->framebuffer_addr != 0 &&
            mbi->framebuffer_width >= 320 && mbi->framebuffer_width <= 3840 &&
            mbi->framebuffer_height >= 200 && mbi->framebuffer_height <= 2160 &&
            (mbi->framebuffer_bpp == 32 || mbi->framebuffer_bpp == 24 || mbi->framebuffer_bpp == 16)) {
            
            fb_state.phys_addr = mbi->framebuffer_addr;
            fb_state.width = mbi->framebuffer_width;
            fb_state.height = mbi->framebuffer_height;
            fb_state.pitch = mbi->framebuffer_pitch;
            fb_state.bpp = mbi->framebuffer_bpp;
            fb_state.type = mbi->framebuffer_type;
            fb_state.buffer = (uint32_t *)(uintptr_t)fb_state.phys_addr;
            fb_state.is_valid = 1;
            str_copy(fb_state.method, "Multiboot v1.0 Framebuffer Table", sizeof(fb_state.method));
            debug_puts("[FB] Discovered via Multiboot Framebuffer Table.\n");
            return;
        }
    }

    /* Method 2: Multiboot VBE Mode Info (Bit 11) */
    if (mbi && (mbi->flags & MULTIBOOT_INFO_VBE_INFO) && mbi->vbe_mode_info != 0) {
        const vbe_mode_info_t *vbe = (const vbe_mode_info_t *)(uintptr_t)mbi->vbe_mode_info;
        if (vbe->phys_base_ptr != 0 &&
            vbe->width >= 320 && vbe->width <= 3840 &&
            vbe->height >= 200 && vbe->height <= 2160 &&
            (vbe->bpp == 32 || vbe->bpp == 24 || vbe->bpp == 16)) {
            
            fb_state.phys_addr = (uint64_t)vbe->phys_base_ptr;
            fb_state.width = (uint32_t)vbe->width;
            fb_state.height = (uint32_t)vbe->height;
            fb_state.pitch = (uint32_t)vbe->pitch;
            fb_state.bpp = (uint32_t)vbe->bpp;
            fb_state.type = 1;
            fb_state.buffer = (uint32_t *)(uintptr_t)fb_state.phys_addr;
            fb_state.is_valid = 1;
            str_copy(fb_state.method, "Multiboot VBE Mode Info", sizeof(fb_state.method));
            debug_puts("[FB] Discovered via Multiboot VBE Mode Info.\n");
            return;
        }
    }

    /* Method 3: Bochs Graphics Adapter (BGA) & QEMU PCI VGA Detection */
    uint16_t bga_id = bga_read(VBE_DISPI_INDEX_ID);
    if (bga_id >= 0xB0C0 && bga_id <= 0xB0C6) {
        /* Read PCI VGA Controller (Bus 0, Dev 2, Func 0) BAR 0 */
        uint32_t pci_bar0 = pci_read_config_32(0, 2, 0, 0x10);
        uint32_t lfb_addr = pci_bar0 & ~0x0F;

        if (lfb_addr == 0 || lfb_addr == 0xFFFFFFF0) {
            /* Fallback to standard QEMU / Bochs PCI VRAM aperture */
            lfb_addr = 0xFD000000;
        }

        /* Program 1024x768x32 linear framebuffer */
        bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
        bga_write(VBE_DISPI_INDEX_XRES, 1024);
        bga_write(VBE_DISPI_INDEX_YRES, 768);
        bga_write(VBE_DISPI_INDEX_BPP, 32);
        bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

        fb_state.phys_addr = (uint64_t)lfb_addr;
        fb_state.width = 1024;
        fb_state.height = 768;
        fb_state.pitch = 1024 * 4;
        fb_state.bpp = 32;
        fb_state.type = 1;
        fb_state.buffer = (uint32_t *)(uintptr_t)fb_state.phys_addr;
        fb_state.is_valid = 1;
        str_copy(fb_state.method, "BGA / QEMU PCI VGA Controller", sizeof(fb_state.method));
        debug_puts("[FB] Discovered and programmed via BGA / PCI VGA.\n");
        return;
    }

    /* If no graphical adapter detected, fallback to text mode */
    str_copy(fb_state.method, "None (Text Console Fallback)", sizeof(fb_state.method));
    debug_puts("[FB] No hardware framebuffer discovered. Text console preserved.\n");
}

void fb_init_mock(uint32_t *buffer, uint32_t width, uint32_t height)
{
    fb_state.phys_addr = (uint64_t)(uintptr_t)buffer;
    fb_state.buffer = buffer;
    fb_state.width = width;
    fb_state.height = height;
    fb_state.pitch = width * 4;
    fb_state.bpp = 32;
    fb_state.type = 1;
    fb_state.is_valid = (buffer != NULL && width > 0 && height > 0) ? 1 : 0;
    str_copy(fb_state.method, "Mock Test Framebuffer", sizeof(fb_state.method));
}

const fb_info_t *fb_get_info(void)
{
    return &fb_state;
}

int fb_is_available(void)
{
    return fb_state.is_valid && (fb_state.buffer != NULL);
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!fb_is_available()) return;
    if (x >= fb_state.width || y >= fb_state.height) return;

    if (fb_state.bpp == 32) {
        uint32_t *line = (uint32_t *)((uint8_t *)fb_state.buffer + y * fb_state.pitch);
        line[x] = color;
    } else if (fb_state.bpp == 24) {
        uint8_t *pixel = (uint8_t *)fb_state.buffer + y * fb_state.pitch + x * 3;
        pixel[0] = (uint8_t)(color & 0xFF);         /* Blue */
        pixel[1] = (uint8_t)((color >> 8) & 0xFF);  /* Green */
        pixel[2] = (uint8_t)((color >> 16) & 0xFF); /* Red */
    } else if (fb_state.bpp == 16) {
        uint16_t *line = (uint16_t *)((uint8_t *)fb_state.buffer + y * fb_state.pitch);
        /* RGB565 */
        uint16_t r = (uint16_t)((color >> 19) & 0x1F);
        uint16_t g = (uint16_t)((color >> 10) & 0x3F);
        uint16_t b = (uint16_t)((color >> 3) & 0x1F);
        line[x] = (r << 11) | (g << 5) | b;
    }
}

uint32_t fb_get_pixel(uint32_t x, uint32_t y)
{
    if (!fb_is_available()) return 0;
    if (x >= fb_state.width || y >= fb_state.height) return 0;

    if (fb_state.bpp == 32) {
        const uint32_t *line = (const uint32_t *)((const uint8_t *)fb_state.buffer + y * fb_state.pitch);
        return line[x];
    }
    return 0;
}

void fb_clear(uint32_t color)
{
    if (!fb_is_available()) return;
    fb_fill_rect(0, 0, fb_state.width, fb_state.height, color);
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    if (!fb_is_available()) return;
    if (x >= fb_state.width || y >= fb_state.height) return;

    if (x + w > fb_state.width) w = fb_state.width - x;
    if (y + h > fb_state.height) h = fb_state.height - y;

    if (fb_state.bpp == 32) {
        for (uint32_t cy = 0; cy < h; cy++) {
            uint32_t *line = (uint32_t *)((uint8_t *)fb_state.buffer + (y + cy) * fb_state.pitch);
            for (uint32_t cx = 0; cx < w; cx++) {
                line[x + cx] = color;
            }
        }
    } else {
        for (uint32_t cy = 0; cy < h; cy++) {
            for (uint32_t cx = 0; cx < w; cx++) {
                fb_put_pixel(x + cx, y + cy, color);
            }
        }
    }
}

void fb_draw_rect_outline(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color, uint32_t thickness)
{
    if (!fb_is_available() || thickness == 0) return;
    /* Top horizontal */
    fb_fill_rect(x, y, w, thickness, color);
    /* Bottom horizontal */
    if (h > thickness) {
        fb_fill_rect(x, y + h - thickness, w, thickness, color);
    }
    /* Left vertical */
    fb_fill_rect(x, y, thickness, h, color);
    /* Right vertical */
    if (w > thickness) {
        fb_fill_rect(x + w - thickness, y, thickness, h, color);
    }
}

void fb_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx - dy;

    while (1) {
        if (x0 >= 0 && (uint32_t)x0 < fb_state.width && y0 >= 0 && (uint32_t)y0 < fb_state.height) {
            fb_put_pixel((uint32_t)x0, (uint32_t)y0, color);
        }
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

int test_framebuffer_invariants(void)
{
    /* 1. Verify structure sizing and state sanity */
    if (sizeof(fb_info_t) == 0) return -1;

    /* 2. Boundary clipping verification (off-screen coordinates do not fault) */
    fb_put_pixel(999999, 999999, 0xFFFFFFFF);
    fb_fill_rect(999999, 999999, 100, 100, 0xFFFFFFFF);
    fb_draw_line(-100, -100, -50, -50, 0xFFFFFFFF);

    return 0;
}

