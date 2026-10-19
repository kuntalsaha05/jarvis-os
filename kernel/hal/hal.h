#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stddef.h>

/* Maximum registered drivers in the static HAL registry */
#define HAL_MAX_DRIVERS 16

/* Maximum length of driver name and dependency name strings */
#define HAL_NAME_MAX_LEN 32
#define HAL_DESC_MAX_LEN 64

/* HAL Return & Error Codes */
#define HAL_SUCCESS                  0
#define HAL_ERR_INVALID_PARAM       -1
#define HAL_ERR_ALREADY_REGISTERED  -2
#define HAL_ERR_NOT_FOUND           -3
#define HAL_ERR_NOT_INITIALIZED     -4
#define HAL_ERR_INIT_FAILED         -5
#define HAL_ERR_IO                  -6
#define HAL_ERR_DEP_UNMET           -7
#define HAL_ERR_CAPACITY            -8
#define HAL_ERR_NOT_SUPPORTED       -9

/* Device Classification */
typedef enum {
    HAL_DEV_CLASS_UNKNOWN = 0,
    HAL_DEV_CLASS_INTERRUPT_CONTROLLER,
    HAL_DEV_CLASS_TIMER,
    HAL_DEV_CLASS_SERIAL,
    HAL_DEV_CLASS_CONSOLE,
    HAL_DEV_CLASS_INPUT_KEYBOARD,
    HAL_DEV_CLASS_INPUT_POINTER,
    HAL_DEV_CLASS_DISPLAY,
    HAL_DEV_CLASS_BLOCK,
    HAL_DEV_CLASS_POWER
} hal_device_class_t;

/* Backend Implementation Type */
typedef enum {
    HAL_BACKEND_HARDWARE = 0,     /* Physical/Emulated I/O ports & MMIO */
    HAL_BACKEND_MEMORY_BACKED,    /* In-memory buffer/RAMDisk */
    HAL_BACKEND_VIRTUAL           /* Abstract/synthetic layer */
} hal_backend_type_t;

/* Device Operational Lifecycle States */
typedef enum {
    HAL_STATE_UNREGISTERED = 0,
    HAL_STATE_REGISTERED,
    HAL_STATE_INITIALIZING,
    HAL_STATE_ONLINE,
    HAL_STATE_OFFLINE,
    HAL_STATE_FAULT
} hal_state_t;

/* Capability Bitmask Flags */
#define HAL_CAP_NONE        0x00000000
#define HAL_CAP_READ        0x00000001
#define HAL_CAP_WRITE       0x00000002
#define HAL_CAP_INTERRUPT   0x00000004
#define HAL_CAP_SEEK        0x00000008
#define HAL_CAP_DMA         0x00000010
#define HAL_CAP_POWER_MGMT  0x00000020
#define HAL_CAP_GRAPHICS    0x00000040
#define HAL_CAP_FALLBACK    0x00000080

/* Standard Driver Interface Definition */
typedef struct hal_driver {
    char name[HAL_NAME_MAX_LEN];               /* Unique identifier */
    char description[HAL_DESC_MAX_LEN];        /* Human-readable description */
    hal_device_class_t dev_class;              /* Device classification */
    hal_backend_type_t backend;                /* Hardware or Memory-backed */
    uint32_t capabilities;                     /* HAL_CAP_* bitmask */
    char dependency[HAL_NAME_MAX_LEN];         /* Name of required dependency, or "" */
    uint32_t irq_number;                       /* Hardware IRQ or 0xFFFFFFFF if none */

    /* Lifecycle callbacks */
    int (*probe)(void);                        /* Hardware discovery check: 1 = present, 0 = absent */
    int (*init)(const void *config);           /* Hardware/driver initialization */
    int (*shutdown)(void);                     /* Graceful shutdown */
    int (*reset)(void);                        /* Device reset */

    /* I/O and Control callbacks */
    int (*read)(void *buffer, uint64_t offset, uint32_t count);
    int (*write)(const void *buffer, uint64_t offset, uint32_t count);
    int (*ioctl)(uint32_t cmd, void *arg);

    /* Runtime state (managed by HAL core) */
    hal_state_t state;
    int last_error;
    uint32_t init_tier;
    uint32_t op_count;
} hal_driver_t;

/* HAL Subsystem Lifecycle & Registry Management */
void hal_init(void);
int hal_register_driver(const hal_driver_t *driver);
const hal_driver_t *hal_find_driver(const char *name);
hal_driver_t *hal_get_driver_mut(const char *name);
uint32_t hal_get_driver_count(void);
const hal_driver_t *hal_get_driver_by_index(uint32_t index);

/* Driver Execution & Dependency Lifecycle */
int hal_init_driver(const char *name);
int hal_init_all(void);
int hal_shutdown_driver(const char *name);
int hal_shutdown_all(void);
int hal_reset_driver(const char *name);

/* String Conversion Utilities */
const char *hal_class_name(hal_device_class_t dev_class);
const char *hal_state_name(hal_state_t state);
const char *hal_backend_name(hal_backend_type_t backend);

/* Built-in Core Drivers Registration */
void hal_register_core_drivers(void);

/* Unified Console Fallback & I/O Routing */
void hal_console_write(const char *str);
int hal_block_read(uint64_t lba, uint32_t count, void *buffer);
int hal_block_write(uint64_t lba, uint32_t count, const void *buffer);

/* Diagnostics & Status Reporting */
void hal_print_status(void);
uint32_t hal_get_online_count(void);

/* Verification & Invariant Self-Test */
int test_hal_invariants(void);

#endif /* HAL_H */
