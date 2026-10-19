#include "hal.h"
#include "../drivers/terminal.h"
#include "../debug.h"
#include "../interrupts/pic.h"
#include "../interrupts/pit.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/framebuffer.h"
#include "../storage/block.h"
#include "../storage/ramdisk.h"
#include "../drivers/power.h"
#include "../diagnostics/diagnostics.h"

#ifdef HOST_TEST
#include <stdio.h>
#endif

/* Static Driver Registry */
static hal_driver_t s_registry[HAL_MAX_DRIVERS];
static uint32_t s_driver_count = 0;

/* Helper String Functions (zero libc dependency) */
static int hal_strcmp(const char *s1, const char *s2)
{
    if (!s1 && !s2) return 0;
    if (!s1 || !s2) return (s1 ? 1 : -1);
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static void hal_strncpy(char *dst, const char *src, size_t max_len)
{
    if (!dst || max_len == 0) return;
    size_t i = 0;
    if (src) {
        while (src[i] && i < max_len - 1) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static size_t hal_strlen(const char *str)
{
    if (!str) return 0;
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

#ifndef HOST_TEST
static inline uint8_t hal_inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void hal_outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
#endif

/* HAL Subsystem Lifecycle & Registry Management */
void hal_init(void)
{
    s_driver_count = 0;
    for (uint32_t i = 0; i < HAL_MAX_DRIVERS; i++) {
        s_registry[i].name[0] = '\0';
        s_registry[i].description[0] = '\0';
        s_registry[i].dev_class = HAL_DEV_CLASS_UNKNOWN;
        s_registry[i].backend = HAL_BACKEND_HARDWARE;
        s_registry[i].capabilities = HAL_CAP_NONE;
        s_registry[i].dependency[0] = '\0';
        s_registry[i].irq_number = 0xFFFFFFFF;
        s_registry[i].probe = NULL;
        s_registry[i].init = NULL;
        s_registry[i].shutdown = NULL;
        s_registry[i].reset = NULL;
        s_registry[i].read = NULL;
        s_registry[i].write = NULL;
        s_registry[i].ioctl = NULL;
        s_registry[i].state = HAL_STATE_UNREGISTERED;
        s_registry[i].last_error = HAL_SUCCESS;
        s_registry[i].init_tier = 0;
        s_registry[i].op_count = 0;
    }
}

int hal_register_driver(const hal_driver_t *driver)
{
    if (!driver || driver->name[0] == '\0') {
        return HAL_ERR_INVALID_PARAM;
    }

    if (s_driver_count >= HAL_MAX_DRIVERS) {
        return HAL_ERR_CAPACITY;
    }

    /* Check for duplicate driver name */
    for (uint32_t i = 0; i < s_driver_count; i++) {
        if (hal_strcmp(s_registry[i].name, driver->name) == 0) {
            return HAL_ERR_ALREADY_REGISTERED;
        }
    }

    hal_driver_t *slot = &s_registry[s_driver_count];
    hal_strncpy(slot->name, driver->name, HAL_NAME_MAX_LEN);
    hal_strncpy(slot->description, driver->description, HAL_DESC_MAX_LEN);
    slot->dev_class = driver->dev_class;
    slot->backend = driver->backend;
    slot->capabilities = driver->capabilities;
    hal_strncpy(slot->dependency, driver->dependency, HAL_NAME_MAX_LEN);
    slot->irq_number = driver->irq_number;
    slot->probe = driver->probe;
    slot->init = driver->init;
    slot->shutdown = driver->shutdown;
    slot->reset = driver->reset;
    slot->read = driver->read;
    slot->write = driver->write;
    slot->ioctl = driver->ioctl;
    slot->state = HAL_STATE_REGISTERED;
    slot->last_error = HAL_SUCCESS;
    slot->init_tier = (slot->dependency[0] == '\0') ? 0 : 1;
    slot->op_count = 0;

    s_driver_count++;
    return HAL_SUCCESS;
}

const hal_driver_t *hal_find_driver(const char *name)
{
    if (!name) return NULL;
    for (uint32_t i = 0; i < s_driver_count; i++) {
        if (hal_strcmp(s_registry[i].name, name) == 0) {
            return &s_registry[i];
        }
    }
    return NULL;
}

hal_driver_t *hal_get_driver_mut(const char *name)
{
    if (!name) return NULL;
    for (uint32_t i = 0; i < s_driver_count; i++) {
        if (hal_strcmp(s_registry[i].name, name) == 0) {
            return &s_registry[i];
        }
    }
    return NULL;
}

uint32_t hal_get_driver_count(void)
{
    return s_driver_count;
}

const hal_driver_t *hal_get_driver_by_index(uint32_t index)
{
    if (index >= s_driver_count) return NULL;
    return &s_registry[index];
}

uint32_t hal_get_online_count(void)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < s_driver_count; i++) {
        if (s_registry[i].state == HAL_STATE_ONLINE) {
            count++;
        }
    }
    return count;
}

/* Driver Lifecycle & Dependency Resolution */
int hal_init_driver(const char *name)
{
    hal_driver_t *drv = hal_get_driver_mut(name);
    if (!drv) return HAL_ERR_NOT_FOUND;

    if (drv->state == HAL_STATE_ONLINE) {
        return HAL_SUCCESS; /* Already initialized */
    }

    /* Verify dependency if specified */
    if (drv->dependency[0] != '\0') {
        const hal_driver_t *dep = hal_find_driver(drv->dependency);
        if (!dep || dep->state != HAL_STATE_ONLINE) {
            drv->last_error = HAL_ERR_DEP_UNMET;
            return HAL_ERR_DEP_UNMET;
        }
    }

    drv->state = HAL_STATE_INITIALIZING;

    /* Execute probe if available */
    if (drv->probe) {
        int present = drv->probe();
        if (present <= 0) {
            drv->state = HAL_STATE_OFFLINE;
            drv->last_error = HAL_ERR_NOT_FOUND;
            return HAL_ERR_NOT_FOUND;
        }
    }

    /* Execute init */
    if (drv->init) {
        int res = drv->init(NULL);
        if (res != 0) {
            drv->state = HAL_STATE_FAULT;
            drv->last_error = HAL_ERR_INIT_FAILED;
            return HAL_ERR_INIT_FAILED;
        }
    }

    drv->state = HAL_STATE_ONLINE;
    drv->last_error = HAL_SUCCESS;
    return HAL_SUCCESS;
}

int hal_init_all(void)
{
    int progress = 1;
    uint32_t passes = 0;

    /* Multi-pass resolution: pass 0 initializes independent drivers,
       subsequent passes initialize dependent drivers as prerequisites come online */
    while (progress && passes < HAL_MAX_DRIVERS) {
        progress = 0;
        passes++;

        for (uint32_t i = 0; i < s_driver_count; i++) {
            if (s_registry[i].state == HAL_STATE_REGISTERED) {
                /* Check if dependency is met */
                if (s_registry[i].dependency[0] == '\0') {
                    if (hal_init_driver(s_registry[i].name) == HAL_SUCCESS) {
                        progress = 1;
                    }
                } else {
                    const hal_driver_t *dep = hal_find_driver(s_registry[i].dependency);
                    if (dep && dep->state == HAL_STATE_ONLINE) {
                        if (hal_init_driver(s_registry[i].name) == HAL_SUCCESS) {
                            progress = 1;
                        }
                    }
                }
            }
        }
    }

    return (int)hal_get_online_count();
}

int hal_shutdown_driver(const char *name)
{
    hal_driver_t *drv = hal_get_driver_mut(name);
    if (!drv) return HAL_ERR_NOT_FOUND;

    if (drv->state == HAL_STATE_ONLINE && drv->shutdown) {
        drv->shutdown();
    }
    drv->state = HAL_STATE_OFFLINE;
    return HAL_SUCCESS;
}

int hal_shutdown_all(void)
{
    /* Shut down in reverse order */
    for (int32_t i = (int32_t)s_driver_count - 1; i >= 0; i--) {
        if (s_registry[i].state == HAL_STATE_ONLINE) {
            if (s_registry[i].shutdown) {
                s_registry[i].shutdown();
            }
            s_registry[i].state = HAL_STATE_OFFLINE;
        }
    }
    return HAL_SUCCESS;
}

int hal_reset_driver(const char *name)
{
    hal_driver_t *drv = hal_get_driver_mut(name);
    if (!drv) return HAL_ERR_NOT_FOUND;

    if (drv->reset) {
        return drv->reset();
    }
    return HAL_ERR_NOT_SUPPORTED;
}

/* String Conversion Utilities */
const char *hal_class_name(hal_device_class_t dev_class)
{
    switch (dev_class) {
        case HAL_DEV_CLASS_INTERRUPT_CONTROLLER: return "INTC";
        case HAL_DEV_CLASS_TIMER:                return "TIMER";
        case HAL_DEV_CLASS_SERIAL:               return "SERIAL";
        case HAL_DEV_CLASS_CONSOLE:              return "CONSOLE";
        case HAL_DEV_CLASS_INPUT_KEYBOARD:       return "KEYBOARD";
        case HAL_DEV_CLASS_INPUT_POINTER:        return "POINTER";
        case HAL_DEV_CLASS_DISPLAY:              return "DISPLAY";
        case HAL_DEV_CLASS_BLOCK:                return "BLOCK";
        case HAL_DEV_CLASS_POWER:                return "POWER";
        case HAL_DEV_CLASS_NETWORK:              return "NETWORK";
        default:                                 return "UNKNOWN";
    }
}

const char *hal_state_name(hal_state_t state)
{
    switch (state) {
        case HAL_STATE_UNREGISTERED: return "UNREG";
        case HAL_STATE_REGISTERED:   return "READY";
        case HAL_STATE_INITIALIZING: return "INIT";
        case HAL_STATE_ONLINE:       return "ONLINE";
        case HAL_STATE_OFFLINE:      return "OFFLINE";
        case HAL_STATE_FAULT:        return "FAULT";
        default:                     return "UNKNOWN";
    }
}

const char *hal_backend_name(hal_backend_type_t backend)
{
    switch (backend) {
        case HAL_BACKEND_HARDWARE:      return "Hardware (I/O/MMIO)";
        case HAL_BACKEND_MEMORY_BACKED: return "Memory-Backed (RAM)";
        case HAL_BACKEND_VIRTUAL:       return "Virtual / Software";
        default:                        return "Unknown";
    }
}

/* Core Driver Adapters */

/* 1. PIC 8259 Adapter */
static int adapter_pic_probe(void)
{
    return 1; /* Standard x86 legacy hardware interrupt controller */
}

static int adapter_pic_init(const void *config)
{
    (void)config;
    pic_init();
    return 0;
}

/* 2. Serial UART COM1 Adapter */
static int adapter_serial_probe(void)
{
#ifdef HOST_TEST
    return 1;
#else
    /* UART Scratchpad Register test (COM1 + 7 = 0x3FF) */
    hal_outb(0x3FF, 0x55);
    if (hal_inb(0x3FF) != 0x55) return 0;
    hal_outb(0x3FF, 0xAA);
    if (hal_inb(0x3FF) != 0xAA) return 0;
    return 1;
#endif
}

static int adapter_serial_init(const void *config)
{
    (void)config;
    debug_init();
    return 0;
}

static int adapter_serial_write(const void *buffer, uint64_t offset, uint32_t count)
{
    (void)offset;
    const char *str = (const char *)buffer;
    if (!str) return -1;
    for (uint32_t i = 0; i < count; i++) {
        debug_putchar(str[i]);
    }
    return (int)count;
}

static int adapter_serial_read(void *buffer, uint64_t offset, uint32_t count)
{
    (void)offset;
    char *buf = (char *)buffer;
    if (!buf || count == 0) return -1;
#ifdef HOST_TEST
    return 0;
#else
    uint32_t read_bytes = 0;
    while (read_bytes < count) {
        if (hal_inb(0x3FD) & 0x01) {
            buf[read_bytes++] = (char)hal_inb(0x3F8);
        } else {
            break;
        }
    }
    return (int)read_bytes;
#endif
}

/* 3. VGA Terminal Adapter */
static int adapter_terminal_probe(void)
{
    return 1; /* Standard 0xB8000 text console */
}

static int adapter_terminal_init(const void *config)
{
    (void)config;
    terminal_initialize();
    return 0;
}

static int adapter_terminal_write(const void *buffer, uint64_t offset, uint32_t count)
{
    (void)offset;
    const char *str = (const char *)buffer;
    if (!str) return -1;
    for (uint32_t i = 0; i < count; i++) {
        terminal_putchar(str[i]);
    }
    return (int)count;
}

/* 4. PIT 8254 Timer Adapter */
static int adapter_pit_probe(void)
{
    return 1; /* Standard x86 8254 PIT */
}

static int adapter_pit_init(const void *config)
{
    (void)config;
    pit_init(100); /* 100 Hz timer tick */
    return 0;
}

/* 5. PS/2 Keyboard Adapter */
static int adapter_keyboard_probe(void)
{
#ifdef HOST_TEST
    return 1;
#else
    uint8_t status = hal_inb(0x64);
    return (status != 0xFF) ? 1 : 0;
#endif
}

static int adapter_keyboard_init(const void *config)
{
    (void)config;
    keyboard_init();
    return 0;
}

static int adapter_keyboard_read(void *buffer, uint64_t offset, uint32_t count)
{
    (void)offset;
    char *buf = (char *)buffer;
    if (!buf || count == 0) return -1;
    uint32_t read_bytes = 0;
    while (read_bytes < count) {
        buf[read_bytes++] = keyboard_read_char();
    }
    return (int)read_bytes;
}

/* 6. PS/2 Mouse Adapter */
static int adapter_mouse_probe(void)
{
#ifdef HOST_TEST
    return 1;
#else
    uint8_t status = hal_inb(0x64);
    return (status != 0xFF) ? 1 : 0;
#endif
}

static int adapter_mouse_init(const void *config)
{
    (void)config;
#ifdef HOST_TEST
    return 0;
#else
    mouse_init();
    return 0;
#endif
}

/* 7. Framebuffer Adapter */
static int adapter_framebuffer_probe(void)
{
    return fb_is_available() ? 1 : 0;
}

static int adapter_framebuffer_init(const void *config)
{
    (void)config;
    return fb_is_available() ? 0 : -1;
}

static int adapter_framebuffer_write(const void *buffer, uint64_t offset, uint32_t count)
{
    (void)offset;
    (void)buffer;
    (void)count;
    return 0;
}

/* 8. RAMDisk Block Device Adapter */
static int adapter_ramdisk_probe(void)
{
    return (ramdisk_init() == 0) ? 1 : 0;
}

static int adapter_ramdisk_init(const void *config)
{
    (void)config;
    return ramdisk_init();
}

static int adapter_ramdisk_read(void *buffer, uint64_t offset, uint32_t count)
{
    return block_read(offset, count, buffer);
}

static int adapter_ramdisk_write(const void *buffer, uint64_t offset, uint32_t count)
{
    return block_write(offset, count, buffer);
}

/* 9. ACPI / Power Management Adapter */
static int adapter_power_probe(void)
{
    return 1;
}

static int adapter_power_init(const void *config)
{
    (void)config;
    power_init();
    return 0;
}

static int adapter_power_shutdown(void)
{
#ifdef HOST_TEST
    return 0;
#else
    sys_poweroff();
    return 0;
#endif
}

static int adapter_power_reset(void)
{
#ifdef HOST_TEST
    return 0;
#else
    sys_reboot();
    return 0;
#endif
}

static int adapter_net_loopback_probe(void)
{
    return 1;
}

static int adapter_net_loopback_init(const void *config)
{
    (void)config;
    return 0;
}

/* Core Drivers Registration */
void hal_register_core_drivers(void)
{
    hal_driver_t pic_drv = {
        .name = "pic_8259",
        .description = "Dual 8259A PIC Interrupt Controller",
        .dev_class = HAL_DEV_CLASS_INTERRUPT_CONTROLLER,
        .backend = HAL_BACKEND_HARDWARE,
        .capabilities = HAL_CAP_INTERRUPT,
        .dependency = "",
        .irq_number = 0xFFFFFFFF,
        .probe = adapter_pic_probe,
        .init = adapter_pic_init,
        .shutdown = NULL,
        .reset = NULL,
        .read = NULL,
        .write = NULL,
        .ioctl = NULL
    };
    hal_register_driver(&pic_drv);

    hal_driver_t serial_drv = {
        .name = "serial_uart",
        .description = "16550 UART Serial Controller (COM1 0x3F8)",
        .dev_class = HAL_DEV_CLASS_SERIAL,
        .backend = HAL_BACKEND_HARDWARE,
        .capabilities = HAL_CAP_READ | HAL_CAP_WRITE | HAL_CAP_FALLBACK,
        .dependency = "",
        .irq_number = 4,
        .probe = adapter_serial_probe,
        .init = adapter_serial_init,
        .shutdown = NULL,
        .reset = NULL,
        .read = adapter_serial_read,
        .write = adapter_serial_write,
        .ioctl = NULL
    };
    hal_register_driver(&serial_drv);

    hal_driver_t terminal_drv = {
        .name = "vga_terminal",
        .description = "VGA 80x25 Text Console (0xB8000)",
        .dev_class = HAL_DEV_CLASS_CONSOLE,
        .backend = HAL_BACKEND_HARDWARE,
        .capabilities = HAL_CAP_WRITE | HAL_CAP_FALLBACK,
        .dependency = "",
        .irq_number = 0xFFFFFFFF,
        .probe = adapter_terminal_probe,
        .init = adapter_terminal_init,
        .shutdown = NULL,
        .reset = NULL,
        .read = NULL,
        .write = adapter_terminal_write,
        .ioctl = NULL
    };
    hal_register_driver(&terminal_drv);

    hal_driver_t pit_drv = {
        .name = "pit_8254",
        .description = "8254 Programmable Interval Timer (100 Hz)",
        .dev_class = HAL_DEV_CLASS_TIMER,
        .backend = HAL_BACKEND_HARDWARE,
        .capabilities = HAL_CAP_INTERRUPT,
        .dependency = "pic_8259",
        .irq_number = 0,
        .probe = adapter_pit_probe,
        .init = adapter_pit_init,
        .shutdown = NULL,
        .reset = NULL,
        .read = NULL,
        .write = NULL,
        .ioctl = NULL
    };
    hal_register_driver(&pit_drv);

    hal_driver_t kbd_drv = {
        .name = "ps2_keyboard",
        .description = "8042 PS/2 Keyboard Driver (Ports 0x60/0x64)",
        .dev_class = HAL_DEV_CLASS_INPUT_KEYBOARD,
        .backend = HAL_BACKEND_HARDWARE,
        .capabilities = HAL_CAP_READ | HAL_CAP_INTERRUPT,
        .dependency = "pic_8259",
        .irq_number = 1,
        .probe = adapter_keyboard_probe,
        .init = adapter_keyboard_init,
        .shutdown = NULL,
        .reset = NULL,
        .read = adapter_keyboard_read,
        .write = NULL,
        .ioctl = NULL
    };
    hal_register_driver(&kbd_drv);

    hal_driver_t mouse_drv = {
        .name = "ps2_mouse",
        .description = "8042 PS/2 Auxiliary Mouse Driver",
        .dev_class = HAL_DEV_CLASS_INPUT_POINTER,
        .backend = HAL_BACKEND_HARDWARE,
        .capabilities = HAL_CAP_READ | HAL_CAP_INTERRUPT,
        .dependency = "pic_8259",
        .irq_number = 12,
        .probe = adapter_mouse_probe,
        .init = adapter_mouse_init,
        .shutdown = NULL,
        .reset = NULL,
        .read = NULL,
        .write = NULL,
        .ioctl = NULL
    };
    hal_register_driver(&mouse_drv);

    hal_driver_t fb_drv = {
        .name = "framebuffer",
        .description = "Linear Framebuffer Graphics (VBE / Multiboot)",
        .dev_class = HAL_DEV_CLASS_DISPLAY,
        .backend = HAL_BACKEND_HARDWARE,
        .capabilities = HAL_CAP_WRITE | HAL_CAP_GRAPHICS,
        .dependency = "",
        .irq_number = 0xFFFFFFFF,
        .probe = adapter_framebuffer_probe,
        .init = adapter_framebuffer_init,
        .shutdown = NULL,
        .reset = NULL,
        .read = NULL,
        .write = adapter_framebuffer_write,
        .ioctl = NULL
    };
    hal_register_driver(&fb_drv);

    hal_driver_t ramdisk_drv = {
        .name = "ramdisk_block",
        .description = "RAM-Backed Volatile Block Device (64 KiB, 128 Sectors)",
        .dev_class = HAL_DEV_CLASS_BLOCK,
        .backend = HAL_BACKEND_MEMORY_BACKED,
        .capabilities = HAL_CAP_READ | HAL_CAP_WRITE | HAL_CAP_SEEK,
        .dependency = "",
        .irq_number = 0xFFFFFFFF,
        .probe = adapter_ramdisk_probe,
        .init = adapter_ramdisk_init,
        .shutdown = NULL,
        .reset = NULL,
        .read = adapter_ramdisk_read,
        .write = adapter_ramdisk_write,
        .ioctl = NULL
    };
    hal_register_driver(&ramdisk_drv);

    hal_driver_t pwr_drv = {
        .name = "acpi_power",
        .description = "ACPI Poweroff Ports & 8042 Reset Controller",
        .dev_class = HAL_DEV_CLASS_POWER,
        .backend = HAL_BACKEND_HARDWARE,
        .capabilities = HAL_CAP_POWER_MGMT,
        .dependency = "",
        .irq_number = 0xFFFFFFFF,
        .probe = adapter_power_probe,
        .init = adapter_power_init,
        .shutdown = adapter_power_shutdown,
        .reset = adapter_power_reset,
        .read = NULL,
        .write = NULL,
        .ioctl = NULL
    };
    hal_register_driver(&pwr_drv);

    hal_driver_t net_drv = {
        .name = "net_loopback",
        .description = "Loopback In-Memory Virtual Network Adapter (lo0)",
        .dev_class = HAL_DEV_CLASS_NETWORK,
        .backend = HAL_BACKEND_MEMORY_BACKED,
        .capabilities = HAL_CAP_READ | HAL_CAP_WRITE | HAL_CAP_NETWORK,
        .dependency = "",
        .irq_number = 0xFFFFFFFF,
        .probe = adapter_net_loopback_probe,
        .init = adapter_net_loopback_init,
        .shutdown = NULL,
        .reset = NULL,
        .read = NULL,
        .write = NULL,
        .ioctl = NULL
    };
    hal_register_driver(&net_drv);
}

/* Console Fallback & I/O Routing */
void hal_console_write(const char *str)
{
    if (!str) return;
    const hal_driver_t *term = hal_find_driver("vga_terminal");
    const hal_driver_t *serial = hal_find_driver("serial_uart");

    int written = 0;
    if (term && term->state == HAL_STATE_ONLINE && term->write) {
        term->write(str, 0, (uint32_t)hal_strlen(str));
        written = 1;
    }

    if (serial && serial->state == HAL_STATE_ONLINE && serial->write) {
        serial->write(str, 0, (uint32_t)hal_strlen(str));
        written = 1;
    }

    /* Absolute fallback if drivers aren't registered yet */
    if (!written) {
        terminal_write(str);
        debug_puts(str);
    }
}

int hal_block_read(uint64_t lba, uint32_t count, void *buffer)
{
    const hal_driver_t *bdev = hal_find_driver("ramdisk_block");
    if (bdev && bdev->state == HAL_STATE_ONLINE && bdev->read) {
        return bdev->read(buffer, lba, count);
    }
    return block_read(lba, count, buffer);
}

int hal_block_write(uint64_t lba, uint32_t count, const void *buffer)
{
    const hal_driver_t *bdev = hal_find_driver("ramdisk_block");
    if (bdev && bdev->state == HAL_STATE_ONLINE && bdev->write) {
        return bdev->write(buffer, lba, count);
    }
    return block_write(lba, count, buffer);
}

/* Status & Diagnostic Table Printing */
void hal_print_status(void)
{
    terminal_write("======================================================================\n");
    terminal_write("            JARVIS-OS HARDWARE ABSTRACTION LAYER (HAL)               \n");
    terminal_write("======================================================================\n");
    terminal_write("ID  DRIVER NAME      CLASS     STATE   BACKEND    DEPENDENCY   IRQ    \n");
    terminal_write("----------------------------------------------------------------------\n");

    for (uint32_t i = 0; i < s_driver_count; i++) {
        const hal_driver_t *d = &s_registry[i];

        /* Print Index */
        terminal_write_number(i);
        if (i < 10) terminal_write("   ");
        else terminal_write("  ");

        /* Print Driver Name */
        terminal_write(d->name);
        size_t nlen = hal_strlen(d->name);
        for (size_t s = nlen; s < 17; s++) terminal_putchar(' ');

        /* Print Class */
        const char *cname = hal_class_name(d->dev_class);
        terminal_write(cname);
        size_t clen = hal_strlen(cname);
        for (size_t s = clen; s < 10; s++) terminal_putchar(' ');

        /* Print State */
        const char *sname = hal_state_name(d->state);
        terminal_write(sname);
        size_t slen = hal_strlen(sname);
        for (size_t s = slen; s < 8; s++) terminal_putchar(' ');

        /* Print Backend abbreviation */
        if (d->backend == HAL_BACKEND_HARDWARE) terminal_write("HW-Port   ");
        else if (d->backend == HAL_BACKEND_MEMORY_BACKED) terminal_write("RAM-Disk  ");
        else terminal_write("Virtual   ");

        /* Print Dependency */
        if (d->dependency[0] != '\0') {
            terminal_write(d->dependency);
            size_t dlen = hal_strlen(d->dependency);
            for (size_t s = dlen; s < 13; s++) terminal_putchar(' ');
        } else {
            terminal_write("None         ");
        }

        /* Print IRQ */
        if (d->irq_number != 0xFFFFFFFF) {
            terminal_write("IRQ");
            terminal_write_number(d->irq_number);
        } else {
            terminal_write("N/A");
        }
        terminal_putchar('\n');
    }

    terminal_write("----------------------------------------------------------------------\n");
    terminal_write("Registered Drivers: ");
    terminal_write_number(s_driver_count);
    terminal_write(" | Online: ");
    terminal_write_number(hal_get_online_count());
    terminal_write(" | Registry Capacity: ");
    terminal_write_number(HAL_MAX_DRIVERS);
    terminal_write("\n======================================================================\n");
}

/* Invariant & Self-Test Verification */
static int s_mock_test_init_fail(const void *config)
{
    (void)config;
    return -1; /* Simulated initialization failure */
}

static int s_mock_test_probe_fail(void)
{
    return 0; /* Hardware not found */
}

static int s_mock_test_init_success(const void *config)
{
    (void)config;
    return 0;
}

int test_hal_invariants(void)
{
    /* Save current registry state */
    hal_driver_t saved_registry[HAL_MAX_DRIVERS];
    uint32_t saved_count = s_driver_count;
    for (uint32_t i = 0; i < saved_count; i++) {
        saved_registry[i] = s_registry[i];
    }

    /* Initialize clean HAL for testing */
    hal_init();
    if (hal_get_driver_count() != 0) return -1;
    if (hal_get_online_count() != 0) return -2;

    /* 1. Register a valid driver */
    hal_driver_t d1 = {
        .name = "mock_parent",
        .description = "Mock Parent INTC",
        .dev_class = HAL_DEV_CLASS_INTERRUPT_CONTROLLER,
        .backend = HAL_BACKEND_HARDWARE,
        .capabilities = HAL_CAP_INTERRUPT,
        .dependency = "",
        .irq_number = 0xFFFFFFFF,
        .probe = NULL,
        .init = s_mock_test_init_success,
        .shutdown = NULL,
        .reset = NULL
    };
    if (hal_register_driver(&d1) != HAL_SUCCESS) return -3;
    if (hal_get_driver_count() != 1) return -4;

    /* 2. Reject duplicate driver registration */
    if (hal_register_driver(&d1) != HAL_ERR_ALREADY_REGISTERED) return -5;

    /* 3. Register a child driver with unmet dependency */
    hal_driver_t d2 = {
        .name = "mock_child",
        .description = "Mock Child Timer",
        .dev_class = HAL_DEV_CLASS_TIMER,
        .backend = HAL_BACKEND_HARDWARE,
        .capabilities = HAL_CAP_INTERRUPT,
        .dependency = "mock_parent",
        .irq_number = 0,
        .probe = NULL,
        .init = s_mock_test_init_success,
        .shutdown = NULL,
        .reset = NULL
    };
    if (hal_register_driver(&d2) != HAL_SUCCESS) return -6;

    /* Initializing child while parent is not online must fail with HAL_ERR_DEP_UNMET */
    if (hal_init_driver("mock_child") != HAL_ERR_DEP_UNMET) return -7;

    /* 4. Multi-pass resolution: hal_init_all resolves parent first, then child */
    int online = hal_init_all();
    if (online != 2) return -8;
    const hal_driver_t *chk_parent = hal_find_driver("mock_parent");
    const hal_driver_t *chk_child = hal_find_driver("mock_child");
    if (!chk_parent || chk_parent->state != HAL_STATE_ONLINE) return -9;
    if (!chk_child || chk_child->state != HAL_STATE_ONLINE) return -10;

    /* 5. Driver probe failure detection */
    hal_driver_t d_absent = {
        .name = "mock_absent",
        .description = "Mock Absent Hardware",
        .dev_class = HAL_DEV_CLASS_DISPLAY,
        .backend = HAL_BACKEND_HARDWARE,
        .capabilities = HAL_CAP_GRAPHICS,
        .dependency = "",
        .irq_number = 0xFFFFFFFF,
        .probe = s_mock_test_probe_fail,
        .init = s_mock_test_init_success,
        .shutdown = NULL,
        .reset = NULL
    };
    if (hal_register_driver(&d_absent) != HAL_SUCCESS) return -11;
    if (hal_init_driver("mock_absent") != HAL_ERR_NOT_FOUND) return -12;
    const hal_driver_t *chk_absent = hal_find_driver("mock_absent");
    if (!chk_absent || chk_absent->state != HAL_STATE_OFFLINE) return -13;

    /* 6. Driver initialization failure handling */
    hal_driver_t d_fail = {
        .name = "mock_faulty",
        .description = "Mock Faulty Device",
        .dev_class = HAL_DEV_CLASS_SERIAL,
        .backend = HAL_BACKEND_HARDWARE,
        .capabilities = HAL_CAP_WRITE,
        .dependency = "",
        .irq_number = 0xFFFFFFFF,
        .probe = NULL,
        .init = s_mock_test_init_fail,
        .shutdown = NULL,
        .reset = NULL
    };
    if (hal_register_driver(&d_fail) != HAL_SUCCESS) return -14;
    if (hal_init_driver("mock_faulty") != HAL_ERR_INIT_FAILED) return -15;
    const hal_driver_t *chk_fail = hal_find_driver("mock_faulty");
    if (!chk_fail || chk_fail->state != HAL_STATE_FAULT) return -16;

    /* 7. Capacity exhaustion boundary */
    for (uint32_t i = s_driver_count; i < HAL_MAX_DRIVERS; i++) {
        hal_driver_t dummy = {
            .dev_class = HAL_DEV_CLASS_BLOCK,
            .backend = HAL_BACKEND_MEMORY_BACKED,
            .dependency = ""
        };
        dummy.name[0] = 'd';
        dummy.name[1] = 'a' + (char)i;
        dummy.name[2] = '\0';
        if (hal_register_driver(&dummy) != HAL_SUCCESS) return -17;
    }
    /* Next registration must fail with HAL_ERR_CAPACITY */
    hal_driver_t overflow_drv = { .name = "overflow", .dependency = "" };
    if (hal_register_driver(&overflow_drv) != HAL_ERR_CAPACITY) return -18;

    /* 8. Graceful shutdown */
    hal_shutdown_all();
    if (hal_get_online_count() != 0) return -19;

    /* Restore original registry */
    s_driver_count = saved_count;
    for (uint32_t i = 0; i < saved_count; i++) {
        s_registry[i] = saved_registry[i];
    }

    return 0;
}
