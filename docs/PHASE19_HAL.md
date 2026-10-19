# JARVIS-OS — Phase 19: Hardware Abstraction Layer (HAL)

**Date**: October 19, 2026  
**Status**: VERIFIED & COMPLETE  
**Git Branch**: `main`  
**Kernel Version**: `0.19.0` (x86_64 Monolithic Ring 0)

---

## 1. Executive Summary

Phase 19 establishes a modular, bounded **Hardware Abstraction Layer (HAL)** for JARVIS-OS. The HAL standardizes device driver registration, lifecycle transitions (`INIT`, `ONLINE`, `OFFLINE`, `FAULT`), device capability querying, dependency ordering, hardware discovery probes, and console fallback routing without breaking or destabilizing existing kernel subsystems.

All device drivers (Serial UART, Dual 8259 PIC, 8254 PIT, PS/2 Keyboard, PS/2 Mouse, Linear Framebuffer, RAMDisk Block Device, and ACPI/Power Controller) have been integrated via standardized HAL adapters. Dynamic memory allocation during low-level initialization has been strictly avoided using a bounded static driver table (`HAL_MAX_DRIVERS = 16`).

---

## 2. HAL Architecture & Driver Lifecycle

### A. Device Classification & Backend Identification
Every registered driver is classified by device class and physical backend type:

- **Classes (`hal_device_class_t`)**:
  - `HAL_DEV_CLASS_INTERRUPT_CONTROLLER`: Programmable Interrupt Controller (PIC 8259)
  - `HAL_DEV_CLASS_TIMER`: System Timer (PIT 8254)
  - `HAL_DEV_CLASS_SERIAL`: Serial Communications UART (16550 COM1)
  - `HAL_DEV_CLASS_CONSOLE`: Screen Output Console (VGA 80x25 Text)
  - `HAL_DEV_CLASS_INPUT_KEYBOARD`: Character Input Device (8042 PS/2 Keyboard)
  - `HAL_DEV_CLASS_INPUT_POINTER`: Relative Pointer Device (8042 PS/2 Mouse)
  - `HAL_DEV_CLASS_DISPLAY`: Graphical Framebuffer (VBE / Multiboot)
  - `HAL_DEV_CLASS_BLOCK`: Block Storage Medium (RAMDisk `/dev/ram0`)
  - `HAL_DEV_CLASS_POWER`: Power & Reset Controller (ACPI PM Ports / 8042 Reset)

- **Backends (`hal_backend_type_t`)**:
  - `HAL_BACKEND_HARDWARE`: Physical or hypervisor-emulated I/O ports and MMIO (UART, PIC, PIT, Keyboard, Mouse, Framebuffer, ACPI).
  - `HAL_BACKEND_MEMORY_BACKED`: In-memory volatile RAMDisk (64 KiB, 128 sectors).
  - `HAL_BACKEND_VIRTUAL`: Synthetic simulation or abstract routing layers.

### B. Standard Driver Interface (`hal_driver_t`)
```c
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

    /* Runtime state */
    hal_state_t state;
    int last_error;
    uint32_t init_tier;
    uint32_t op_count;
} hal_driver_t;
```

### C. Driver Lifecycle State Machine
```
[UNREGISTERED]
      | (hal_register_driver)
      v
 [REGISTERED] (READY)
      | (hal_init_driver / hal_init_all)
      v
 [INITIALIZING]
     /          \
    / (Probe/Init\ (Probe=0 -> OFFLINE
   /   Success)   \ Init!=0  -> FAULT)
  v                v
[ONLINE]       [OFFLINE / FAULT]
  |
  +--- (hal_shutdown_driver / hal_shutdown_all) ---> [OFFLINE]
```

---

## 3. Registered Core Drivers & Dependency Ordering

HAL implements a multi-pass topological dependency engine. Drivers declaring prerequisites (e.g. `pit_8254` requiring `pic_8259`) are held until their dependency transitions to `HAL_STATE_ONLINE`.

| ID | Driver Name | Class | Backend | Capabilities | Dependency | IRQ | Hardware Mechanism |
|:---|:---|:---|:---|:---|:---|:---|:---|
| 0 | `pic_8259` | `INTC` | Hardware | `INTERRUPT` | None | N/A | Dual 8259A PIC Ports `0x20`/`0x21`, `0xA0`/`0xA1` |
| 1 | `serial_uart` | `SERIAL` | Hardware | `READ \| WRITE \| FALLBACK` | None | IRQ4 | 16550 UART COM1 Port `0x3F8` + QEMU `0xE9` |
| 2 | `vga_terminal` | `CONSOLE` | Hardware | `WRITE \| FALLBACK` | None | N/A | VGA Text Buffer `0xB8000` (80x25 columns) |
| 3 | `pit_8254` | `TIMER` | Hardware | `INTERRUPT` | `pic_8259` | IRQ0 | 8254 Timer Channel 0 Port `0x40`/`0x43` (100 Hz) |
| 4 | `ps2_keyboard` | `KEYBOARD` | Hardware | `READ \| INTERRUPT` | `pic_8259` | IRQ1 | 8042 Controller Ports `0x60`/`0x64` |
| 5 | `ps2_mouse` | `POINTER` | Hardware | `READ \| INTERRUPT` | `pic_8259` | IRQ12 | 8042 Auxiliary PS/2 Port (Packet Streaming) |
| 6 | `framebuffer` | `DISPLAY` | Hardware | `WRITE \| GRAPHICS` | None | N/A | Multiboot VBE Linear Framebuffer / BGA PCI BAR |
| 7 | `ramdisk_block`| `BLOCK` | Memory-Backed | `READ \| WRITE \| SEEK`| None | N/A | Volatile RAMDisk 64 KiB, 128 Sectors |
| 8 | `acpi_power` | `POWER` | Hardware | `POWER_MGMT` | None | N/A | ACPI PM Ports `0x604`, `0xB004`, 8042 Reset `0x64` |

---

## 4. Hardware Discovery & Fallback Strategy

### A. Non-Destructive Hardware Probes
- **UART 16550 Scratchpad Probe**: Writes test patterns (`0x55`, `0xAA`) to `COM1_PORT + 7` (`0x3FF`) and verifies readback. Real 16550 UARTs and QEMU preserve scratchpad data; missing hardware returns floating bus values (`0xFF`).
- **PS/2 Controller Status Probe**: Reads status register `0x64`. If `0xFF` is returned, no keyboard controller exists.
- **Multiboot Framebuffer Probe**: Validates Multiboot information flags and linear framebuffer physical address.
- **RAMDisk Block Probe**: Verifies block buffer presence and sector alignment.
- **Scope Limitation**: ACPI AML table parsing and dynamic PCI bus enumeration are intentionally deferred to avoid unbacked claims; probing is strictly limited to verified architecture registers.

### B. Console Fallback Routing
`hal_console_write(str)` dynamically routes output based on hardware state:
1. Primary output routes to `vga_terminal` if online.
2. Output is mirrored to `serial_uart` if online.
3. If graphical or text display is unavailable, output automatically falls back to `serial_uart` without kernel panic.

---

## 5. Verification & Test Evidence

### A. Automated Test Matrix

| Test Suite | Scope | Target | Result |
|:---|:---|:---|:---|
| **Host Unit Suites** | 19 Unit Suites | `make test` | **19 / 19 PASS** |
| **HAL Invariant Suite** | Registration, Duplicates, Multi-Pass Deps, Probes, Faults | Suite #19 | **PASS** |
| **Python AI Suite** | HAL Limits, Schemas, Offline Fallback Routing | `make test-ai` | **29 / 29 PASS** |
| **QEMU Smoke Suite** | Kernel Boot, IDT Gates, HAL Invariants, Shell `hal` | `make test-qemu` | **72 / 72 PASS** |
| **ACPI Shutdown Test** | ACPI `0x604` Power-off with Volatile Storage Warning | `test-shutdown` | **PASS** |
| **GUI Visual Suite** | 7 Native Desktop App Screendumps | `make test-gui` | **7 / 7 PASS** |
| **Release Validation** | 13-Step Automated Release Workflow | `make test-release` | **13 / 13 PASS** |
| **Performance Bench** | Memory, Storage, Framebuffer, Event Throughput | `make bench-full` | **PASS** |

### B. Shell Demonstration (`hal` command)
```
jarvis> hal
======================================================================
            JARVIS-OS HARDWARE ABSTRACTION LAYER (HAL)               
======================================================================
ID  DRIVER NAME      CLASS     STATE   BACKEND    DEPENDENCY   IRQ    
----------------------------------------------------------------------
0   pic_8259         INTC      ONLINE  HW-Port    None         N/A
1   serial_uart      SERIAL    ONLINE  HW-Port    None         IRQ4
2   vga_terminal     CONSOLE   ONLINE  HW-Port    None         N/A
3   pit_8254         TIMER     ONLINE  HW-Port    pic_8259     IRQ0
4   ps2_keyboard     KEYBOARD  ONLINE  HW-Port    pic_8259     IRQ1
5   ps2_mouse        POINTER   ONLINE  HW-Port    pic_8259     IRQ12
6   framebuffer      DISPLAY   ONLINE  HW-Port    None         N/A
7   ramdisk_block    BLOCK     ONLINE  RAM-Disk   None         N/A
8   acpi_power       POWER     ONLINE  HW-Port    None         N/A
----------------------------------------------------------------------
Registered Drivers: 9 | Online: 9 | Registry Capacity: 16
======================================================================
```

---

## 6. Known Limitations & Architectural Scope

1. **Static Driver Table**: The driver registry supports up to 16 static boot-time drivers (`HAL_MAX_DRIVERS = 16`). Hot-plug device addition is deliberately unsupported.
2. **Volatile Storage Backing**: Block device operations route to the in-memory RAMDisk (`/dev/ram0`, 64 KiB). HAL block drivers disclose storage volatility across reboots.
3. **PCI / ACPI Extensions**: HAL provides a foundation for future PCI device scanning and ACPI AML table parsing, but does not claim their presence until hardware drivers are implemented.
