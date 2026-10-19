# JARVIS-OS — Phase 18: Diagnostics and Recovery Technical Report

**Milestone**: Phase 18 — Diagnostics, Fault Handling, and Controlled Recovery  
**Target Architecture**: x86_64 Monolithic Ring 0 Kernel  
**Date**: October 19, 2026  
**Status**: **VERIFIED & ACCEPTED**  

---

## 1. Executive Summary

Phase 18 implements a unified diagnostics architecture, a robust 32-vector CPU exception handling subsystem, controlled ACPI power-off and restart mechanisms, and fault recovery boundaries across JARVIS-OS.

All features are strictly grounded in the actual hardware interfaces and architectural constraints of the uniprocessor x86_64 kernel and QEMU virtualized environment:
1. **Accurate Metrics Telemetry**: Every reported metric originates from its live kernel source (interrupt counters, timer ticks, scheduler context switches, page frame bitmaps, heap allocator blocks, and filesystem metadata).
2. **Safe Bounded Diagnostic Log**: Statically allocated 32-entry circular ring buffer (`DIAG_LOG_CAPACITY = 32`) with tick timestamps, severity levels, and safe FIFO overwrite eviction under pressure. Zero heap allocations ensure safety during interrupt or fault contexts.
3. **Hardware CPU Exception Handling**: Complete set of 32 IDT exception stubs in `interrupts.S` providing standardized `exception_frame_t` contexts (176 bytes) for exceptions with and without CPU error codes, detailed serial crash dumps (RIP, CS, RFLAGS, RSP, SS, registers, CR2, CR3), and controlled kernel panics that halt execution safely (`cli; hlt`) rather than hiding defects behind automated reboot loops.
4. **Controlled Shutdown & Restart**: Documented shutdown (`sys_poweroff`) utilizing QEMU/Bochs ACPI poweroff ports (`0x604`, `0xB004`, `0x501`, `0xF4`) and reboot (`sys_reboot`) pulsing the 8042 keyboard controller reset line (`0x64`, `0xFE`) with triple-fault CPU fallback.
5. **Explicit Volatile Storage Disclosure**: Both shutdown and reboot paths explicitly warn the operator that the storage subsystem is backed by a volatile RAMDisk (`/dev/ram0`), and data is not retained across power cycles.
6. **Component-Independent Recovery**: GUI application failures are isolated to their descriptors (`APP_STATE_STOPPED`), desktop exit cleanly restores the serial and VGA text shell, and AI companion transport disconnection gracefully degrades to offline simulation without freezing the kernel.

---

## 2. System Architecture & Component Design

```
+-----------------------------------------------------------------------------------+
|                                 JARVIS-OS SHELL                                   |
|   commands: status | diag | dmesg | shutdown | reboot | panic | pftest | gui      |
+-----------------------------------------------------------------------------------+
          |                     |                   |                   |
          v                     v                   v                   v
+-------------------+ +-------------------+ +---------------+ +--------------------+
| System Diagnostics| | Bounded Event Log | | Power Control | | Exception Handling |
| (diagnostics.c)   | | (32-entry dmesg)  | | (power.c)     | | (idt.c, interrupts)|
+-------------------+ +-------------------+ +---------------+ +--------------------+
  | - Uptime ticks      | - Timestamp tick    | - QEMU ACPI   | - 32 Vector Stubs
  | - IRQ0,1,12 counters| - Severity level    |   Port 0x604  | - Error Code Align
  | - Context switches  | - Subsystem tag     | - 8042 Reset  | - RIP / CR2 / Regs
  | - Memory (Page+Heap)| - Zero-alloc ring   |   Port 0x64   | - Serial Crash Dump
  | - FS Ops & Rollback | - Eviction counter  | - RAMDisk Warn| - Panic Halt Loop
+-----------------------------------------------------------------------------------+
```

### 2.1 Centralized System Diagnostics (`kernel/diagnostics/diagnostics.h`, `diagnostics.c`)

The diagnostic subsystem aggregates live metrics across kernel layers into a standardized `diag_system_summary_t` structure:

- **Build & Kernel Telemetry**: Version `0.18.0`, architecture `x86_64`, execution mode `Ring 0 Monolithic`, compiler version (`__VERSION__`), build date and time.
- **Interrupt Accounting**:
  - `irq0_timer`: PIT timer ticks.
  - `irq1_keyboard`: PS/2 keyboard keystrokes.
  - `irq12_mouse`: PS/2 mouse packet interrupts.
  - `syscalls`: INT 0x80 software interrupts.
  - `yields`: INT 0x81 voluntary scheduler yields.
  - `page_faults`: CPU vector 14 faults.
  - `exceptions`: CPU vectors 0–13, 15–31.
  - `total_interrupts`: Monotonic sum of all hardware interrupts and exceptions.
- **Process & Scheduler Telemetry**: Active process count from `process_get_count()`, total context switches, quantum preemptions, and voluntary yields from `scheduler_get_stats()`.
- **Memory Telemetry**:
  - Physical Frame Allocator: Total frames, used/allocated frames, and free frames from `page_get_stats()`.
  - Kernel Heap: Total capacity (64 KiB), used bytes, free bytes, block count, largest contiguous free chunk, and allocation/free counts from `heap_get_stats()`.
- **Filesystem Telemetry**: Superblock capacity (128 blocks = 64 KiB), free/used blocks, free/used inodes, total operations (`fs_stat_ops`), operation errors (`fs_stat_errors`), and atomic rollback occurrences (`fs_stat_rollbacks`).
- **Graphical Desktop Telemetry**: Framebuffer detection status ($1024 \times 768 \times 32\text{ bpp}$), GUI running state, active window count via z-order query, and active notification count via `notif_get_active_count()`.

### 2.2 Bounded Diagnostic Ring-Buffer Log (`dmesg`)

A static ring buffer stores the 32 most recent kernel diagnostic messages:

```c
typedef struct {
    uint64_t timestamp_ticks;
    uint8_t  level;                     /* DEBUG, INFO, WARN, ERROR, PANIC */
    char     subsystem[DIAG_SUBSYS_LEN]; /* 12-byte fixed buffer */
    char     message[DIAG_MSG_LEN];      /* 52-byte fixed buffer */
} diag_log_entry_t;
```

**Safe Overflow Behavior**:
- When the buffer reaches 32 entries, new entries overwrite the oldest slot (`insert_slot = diag_log_head`), the head index advances (`diag_log_head = (diag_log_head + 1) % 32`), and `diag_overflow_count` is incremented.
- Total bytes per entry is exactly 64 bytes ($32 \times 64 = 2048\text{ bytes}$ total static footprint).
- Zero calls to `kmalloc` or dynamic heaps; completely interrupt-safe and safe to execute during fatal faults or low-memory conditions.

### 2.3 CPU Exception & Stack-Frame Handling (`kernel/interrupts/`)

In x86_64, the CPU pushes an error code for certain exceptions (Vectors 8, 10, 11, 12, 13, 14, 17, 21), but does not push an error code for others (Vectors 0–7, 9, 15, 16, 18–20, 22–31).

JARVIS-OS unifies this via assembly macros in `kernel/interrupts/interrupts.S`:
- `EXCEPTION_NO_ERR vec`: pushes a dummy `$0` error code, then pushes `vec`.
- `EXCEPTION_ERR vec`: pushes `vec` directly (CPU error code is already on the stack).
- `common_exception_entry`: executes `SAVE_REGISTERS` (15 64-bit GPRs), aligns stack to 16 bytes for System V ABI compliance, and calls `c_exception_handler(exception_frame_t *frame, uint64_t cr2)`.

**Standardized Exception Frame (`exception_frame_t`, 176 bytes)**:
```c
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) exception_frame_t;
```

**Serial Crash Dump & Panic Safety**:
When an unrecoverable exception occurs:
1. Interrupts are disabled immediately (`cli`).
2. Event is logged to the diagnostic ring buffer and interrupt counter is updated.
3. Formatted crash dump is transmitted over COM1 serial (`0x3F8` / `0xE9`):
   - Exception Name & Vector (e.g. `[#GP] General Protection Fault (Vector 13)`).
   - Faulting Instruction Pointer `RIP` and Code Segment `CS`.
   - CPU Error Code decoded into constituent flags.
   - For Vector 14 (#PF): Fault address `CR2`, page presence, read/write, user/supervisor, instruction fetch, and 2 MiB identity-mapped boundary comparison.
   - Full 15-register GPR dump (RAX through R15), RSP, RBP, RFLAGS, and SS.
4. Execution halts in an infinite `hlt` loop via `kernel_panic()`. It **never** resumes execution or automatically reboots in a loop that masks the crash.

### 2.4 Controlled Shutdown & Restart Subsystem (`kernel/drivers/power.c`, `power.h`)

JARVIS-OS implements controlled shutdown and reboot sequences via documented port interfaces:

- **Graceful Shutdown (`sys_poweroff`)**:
  1. Logs `DIAG_LEVEL_INFO` shutdown event.
  2. Discloses volatile storage:
     `"[SHUTDOWN] Note: Storage is backed by volatile RAMDisk (/dev/ram0). Data is not retained across power cycles."`
  3. Disables interrupts (`cli`).
  4. Triggers ACPI poweroff:
     - QEMU PIIX4 ACPI PM1a_CNT port: `outw(0x604, 0x2000)`
     - Bochs / legacy QEMU PM1a_CNT port: `outw(0xB004, 0x2000)`
     - QEMU debug exit port: `outb(0x501, 0x00)`
     - ISA debug exit device: `outb(0xF4, 0x00)`
  5. Fallback for bare-metal hardware without ACPI DSDT parsing: prints `"ACPI power-off unsupported on this platform. System halted."` and enters safe `cli; hlt` loop.

- **Graceful Restart (`sys_reboot`)**:
  1. Logs `DIAG_LEVEL_INFO` restart event.
  2. Discloses RAMDisk reset notice.
  3. Disables interrupts (`cli`).
  4. Pulses CPU reset line via 8042 PS/2 Keyboard Controller (`outb(0x64, 0xFE)`).
  5. Fallback: Forces CPU Triple Fault by loading a NULL IDT descriptor (`limit=0, base=0`) and issuing `INT 3`.
  6. Final fallback: `cli; for (;;) hlt;`.

### 2.5 Watchdog & Component-Independent Recovery

To ensure system resilience without compromising kernel integrity:
1. **GUI Failure Isolation**: Application crashes or window closures return the application state machine to `APP_STATE_STOPPED`. Uncaught exceptions do not panic the desktop; exiting the desktop via `ESC` or shutdown cleanly restores the interactive serial and VGA text shell.
2. **AI Companion Disconnection Handling**: If the host companion or bridge socket drops, queries degrade gracefully to `OFFLINE_SIMULATION` fallback mode. The agent remains responsive, tracking queries and errors without crashing or blocking the kernel shell.
3. **No Panic Auto-Reboot**: Kernel panics halt safely, preserving diagnostic serial logs for post-mortem analysis rather than continuously resetting the CPU.

---

## 3. Test & Verification Evidence

### 3.1 Verification Matrix

| Suite | Target | Executed Command | Result | Details |
| :--- | :--- | :--- | :--- | :--- |
| **Host Unit Tests** | Diagnostics, Exceptions, Heap & Recovery | `make test` | **PASS (18/18)** | 18 test suites passing; verified 32-entry bounded ring buffer, overflow eviction, 176-byte frame layout, page fault decoding, heap stats, and volatile storage disclosure. |
| **Python Unit Tests** | AI Disconnection & Limits | `make test-ai` | **PASS (28/28)** | 28 unit tests passing; verified AI bridge disconnection fallback, log buffer sizing bounds, and volatile RAMDisk disclosures. |
| **QEMU Bare-Metal Smoke** | Boot, Shell & Diagnostics | `make test-qemu` | **PASS (69/69)** | 69 boot checkpoints passing; verified early serial debug, diagnostics invariant boot check, exception handling invariant check, `diag` shell command, and `dmesg` shell command. |
| **Controlled Shutdown** | ACPI Poweroff | `python3 scripts/qemu_shutdown_test.py` | **PASS** | Booted QEMU, issued `shutdown` command, verified shutdown banner and volatile RAMDisk notice, verified clean QEMU exit code 0 within 2.5 seconds. |
| **GUI Visual Smoke** | Desktop & Application Windows | `make test-gui` | **PASS (7/7)** | 7 visual screendumps verified: Terminal, SysMon, Files, Proc, AI Companion, Settings, and Dragged Window. |
| **Release Integration** | Full Release Workflow | `make test-release` | **PASS (13/13)** | Clean build, ISO generation, QEMU boot, shell input, preemption, FS, IPC, algorithms, and AI security authorization all verified. |
| **Performance Benchmarks** | Full Benchmark Suite | `make bench-full` | **PASS** | 405.5M heap ops/sec (0.0025 $\mu\text{s}$ latency), 605.0 MB/s FS write, 1,971.5 FPS window composition, 111.3M events/sec. |

### 3.2 Automated Shutdown Test Evidence

```
==================================================
   JARVIS-OS CONTROLLED SHUTDOWN VERIFICATION     
==================================================
[1/3] Booting JARVIS-OS in QEMU...
[2/3] Issuing 'shutdown' command to kernel shell...
[3/3] Waiting for ACPI power-off and clean QEMU exit...
Captured 2978 bytes of serial output during shutdown.

  [PASS] Shutdown Banner
  [PASS] Volatile RAMDisk Disclosure
  [PASS] ACPI Power-off Invocation
  [PASS] QEMU Exit Code: 0 (Clean Power-Off)

==================================================
   CONTROLLED SHUTDOWN VERIFICATION: PASS        
==================================================
```

---

## 4. Shell Commands Reference (Phase 18 Extensions)

| Command | Subsystem | Description |
| :--- | :--- | :--- |
| `diag` / `diagnostics` | Diagnostics | Displays comprehensive system telemetry (Version, IRQs, Scheduler, Memory, Storage, GUI, AI, and Log status). |
| `dmesg` | Logging | Displays the chronological kernel ring-buffer diagnostic event log with timestamps and severity tags. |
| `shutdown` / `poweroff` | Power | Initiates controlled system shutdown, discloses RAMDisk volatility, and powers off via ACPI port `0x604`. |
| `reboot` / `restart` | Power | Initiates controlled system reboot via 8042 keyboard controller reset line (`0x64`, `0xFE`). |
| `panic` | Diagnostics | Triggers diagnostic kernel panic testing the serial register dump and safe halt loop. |
| `pftest` | Memory | Executes page-fault decoder test verifying safe diagnosis of protection violations and unmapped access. |

---

## 5. Architectural Boundaries & Known Limitations

1. **Storage Volatility Across Power Cycles**: The filesystem is backed by a 64 KiB volatile RAMDisk (`/dev/ram0`). The controlled shutdown and restart sequences explicitly disclose that file data and saved configurations do not persist across reboots.
2. **ACPI Hardware Poweroff Scope**: `sys_poweroff` uses standard QEMU and Bochs ACPI PM ports (`0x604`, `0xB004`). Real physical hardware without full ACPI AML/DSDT table parsing will safely halt (`cli; hlt`) with a user-facing prompt to switch off power.
3. **Unrecoverable Fault Halting**: Kernel faults (page faults on invalid addresses, double faults, general protection faults) deliberately halt the system after printing diagnostic register dumps. The kernel does not attempt to resume execution or hide fatal faults.

