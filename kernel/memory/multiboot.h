#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

#define MULTIBOOT_INFO_MEMORY      0x00000001
#define MULTIBOOT_INFO_BOOTDEV     0x00000002
#define MULTIBOOT_INFO_CMDLINE     0x00000004
#define MULTIBOOT_INFO_MODS        0x00000008
#define MULTIBOOT_INFO_MEM_MAP     0x00000040
#define MULTIBOOT_INFO_VBE_INFO         0x00000800
#define MULTIBOOT_INFO_FRAMEBUFFER_INFO 0x00001000

#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED  0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB      1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT 2

typedef struct multiboot_mmap_entry
{
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

typedef struct vbe_mode_info
{
    uint16_t attributes;
    uint8_t  window_a, window_b;
    uint16_t granularity;
    uint16_t window_size;
    uint16_t segment_a, segment_b;
    uint32_t win_func_ptr;
    uint16_t pitch;
    uint16_t width, height;
    uint8_t  w_char, y_char, planes, bpp, banks;
    uint8_t  memory_model, bank_size, image_pages;
    uint8_t  reserved0;
    uint8_t  red_mask, red_position;
    uint8_t  green_mask, green_position;
    uint8_t  blue_mask, blue_position;
    uint8_t  rsv_mask, rsv_position;
    uint8_t  direct_color_attributes;
    uint32_t phys_base_ptr;
    uint32_t offscreen_memory_offset;
    uint16_t offscreen_memory_size;
    uint8_t  reserved1[206];
} __attribute__((packed)) vbe_mode_info_t;

typedef struct multiboot_info
{
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    /* Multiboot 0.6.96 Framebuffer Table (Bit 12) */
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  framebuffer_red_field_position;
    uint8_t  framebuffer_red_mask_size;
    uint8_t  framebuffer_green_field_position;
    uint8_t  framebuffer_green_mask_size;
    uint8_t  framebuffer_blue_field_position;
    uint8_t  framebuffer_blue_mask_size;
} __attribute__((packed)) multiboot_info_t;

/* Parse and report bootloader memory map */
void multiboot_dump_memory_map(uint64_t magic, uint64_t mb_addr);

#endif /* MULTIBOOT_H */
