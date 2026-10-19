# JARVIS-OS

**AI-First Educational Operating System · x86-64 · C and Assembly**

JARVIS-OS is an educational operating system built from scratch for the x86-64 architecture, demonstrating core operating-system mechanisms (paging, preemption, scheduling, IPC, synchronization, deadlock avoidance, filesystems) alongside a modular, host-side AI companion.

```
========================================
              JARVIS-OS
========================================
Kernel: x86-64 (Long Mode, Multiboot)
Status: ONLINE
Terminal: VGA (0xB8000) + Serial (COM1 0x3F8) + QEMU Debug (0xE9)
```

---

## Key Features

1. **Bare-Metal x86-64 Kernel**:
   - Boots via GRUB/Multiboot directly into 64-bit Long Mode.
   - Multiboot 0.6.96 hand-off with physical memory-map discovery and upper/lower RAM detection.
   - Identity-mapped PML4, PDPT, and Page Directory (2 MiB huge pages).
   - Early serial debug driver (`kernel/drivers/debug.c`) active on COM1 (`0x3F8`) and QEMU debug port (`0xE9`).
   - Dedicated `kernel_panic` handler reporting CR2, CR3, and RSP with a high-contrast red VGA banner.
   - Global Descriptor Table (GDT), Interrupt Descriptor Table (IDT, 256 gates), 8259 PIC remap, and 100 Hz PIT timer.
   - Dual-output console: VGA text buffer (`0xB8000`), COM1 serial port (`0x3F8`), and QEMU/Bochs debug port (`0xE9`).
   - Dual keyboard input: PS/2 controller (`0x60/0x64`) and COM1 serial input with VT100 terminal handling.

2. **Preemptive & Cooperative Scheduling, Threads & Process Correctness**:
   - Process Control Blocks (TCB), process creation, termination, and strict state transition validation (`process_is_valid_transition`: NEW, READY, RUNNING, BLOCKED, TERMINATED).
   - Dedicated idle task (PID 0) executing `sti; hlt` without starving runnable user tasks.
   - Thread support: `thread_create(entry, arg, priority)` passing `arg` in `%rdi` via System V AMD64 ABI.
   - Blocking and sleeping: `process_block()`, `process_unblock()`, and `process_sleep(ticks)` integrated with PIT timer scheduler wakeups.
   - Interrupt-driven Round-Robin preemption via PIT timer (configurable quantum via `scheduler_set_quantum`, default 5 ticks).
   - Callee-saved register integrity (%rbx, %r12-%r15) and stack canaries verified across repeated yields and involuntary preemption.
   - Scheduler instrumentation tracking context switches, quantum preemptions, voluntary yields, and timer ticks.
   - Cooperative yielding via software interrupt `int $0x81`.
   - Exact 20-quadword x86-64 `iretq` frame restoration (15 GPRs + RIP + CS + RFLAGS + RSP + SS) obeying AMD64 Long Mode 5-field pop semantics for all privilege levels.


3. **Classic OS Algorithm Implementations & Demonstrations**:
   - **CPU Scheduling**: FCFS, SJF, and Round Robin with arrival time, burst time, completion time, turnaround time, and waiting time calculations.
   - **Memory Allocation Fit**: First Fit, Best Fit, and Worst Fit dynamic partition simulations with external fragmentation metrics.
   - **Page Replacement**: FIFO, LRU, and Optimal replacement simulation.
   - **Belady's Anomaly**: Deterministic demonstration showing FIFO page faults increase from 9 to 10 when frames increase from 3 to 4.
   - **Deadlock Management**: Full Banker's Algorithm with Allocation, Max, Need ($Need = Max - Alloc$), and Available matrices; safety algorithm; safe-sequence generation; WFG/Matrix cycle detection; and Havender global ordering prevention.
   - **File Allocation Methods**: Contiguous, Linked, and Indexed block allocation benchmarks with directory lookups and overhead comparisons.
   - **Disk Head Scheduling**: FCFS, SSTF, SCAN (Elevator), and C-SCAN seek calculations and total head movement.
   - **Concurrency & Sync**: Counting semaphores with task wait queues and atomic wakeups, mutex locks with owner PID validation (`MUTEX_ERR_NOT_OWNER`) and recursive deadlock rejection (`MUTEX_ERR_DEADLOCK`), Producer-Consumer bounded buffer, and Reader-Writer problem.
   - **Inter-Process Communication (IPC)**: Multi-queue subsystem (4 isolated queues), typed messages with sender/receiver PIDs, bounded buffer capacity enforcement (`IPC_ERR_FULL`, `IPC_ERR_EMPTY`), scheduler-integrated blocking receive with tick timeout (`IPC_ERR_TIMEOUT`), and queue ownership/destruction safety.

4. **Storage & Resilient Filesystem (JARVIS-FS)**:
   - Clean block device abstraction with bounds checking and RAM disk backing (128 sectors of 512 bytes = 64 KiB).
   - Lightweight filesystem with superblock (magic `0x4A415256`), inode table, direct data blocks, creation, opening, reading, writing, and directory listing.
   - Filesystem resilience: atomic rollback on partial allocation failure, file truncation (`trunc`), path normalization (leading slashes), active file descriptor invalidation on delete, and block reuse.
   - Host-side image inspection and creation utility (`scripts/jarvis_fs_tool.py`) for offline disk image verification.


5. **AI-First Modular Host Companion & Autonomous Agent**:
   - Host-side Python companion service (`jarvis/`) communicating over a versioned line-delimited JSON bridge (Protocol v1.0).
   - Abstract `LLMInterface` with pluggable backends: **OllamaBackend** (default: local Ollama daemon on `localhost:11434`), **LlamaCppBackend** (local quantized GGUF inference), **AirLLMBackend** (layer-by-layer low-VRAM execution), and **MockLLMBackend** (offline test fixture for CI).
   - 8-Step Autonomous Agent loop (`JarvisAgent.run_cycle`): accepts requests, builds context, determines tools, validates schemas, requests approvals, executes safely, returns telemetry, and synthesizes answers disclosing data provenance (`LIVE_KERNEL` vs `OFFLINE_SIMULATION`).
   - 4-Tier Security Model in `ToolRegistry`: `READ_ONLY`, `SAFE_DIAGNOSTIC`, `PRIVILEGED`, and `DESTRUCTIVE`.
   - Mandatory user confirmation callbacks for privileged (`system.trigger_panic`) and destructive (`storage.format_disk`) operations.
   - Enforced 10-second execution deadlines and sub-millisecond audit trails.
   - Host CLI with `--interactive`, `--explain <concept>`, `--fs-diagnostics`, `--status`, and `--audit`.

6. **Native Graphical Desktop, Window Manager, Settings & Notification Subsystems (Phases 10, 11, 12, 16 & 17)**:
   - High-resolution VBE/Bochs BGA $1024 \times 768 \times 32\text{ bpp}$ linear framebuffer auto-discovery (`0xFD000000`).
   - True Window Manager (`kernel/gui/wm.c` / `wm.h`) supporting window creation, destruction, z-order promotion, title bars, and boundary clipping.
   - Interactive window controls: Minimize (`_`), Maximize (`[]`), Close (`X`), and mouse dragging.
   - Decoupled GUI Event Queue (`kernel/gui/event.c` / `event.h`) processing mouse and keyboard events outside interrupt context.
   - Reusable GUI Application Framework (`kernel/gui/app.c` / `app.h`) with lifecycle states (`REGISTERED`, `RUNNING`, `MINIMIZED`, `STOPPED`), duplicate ID rejection, and clean resource destruction.
   - Desktop Configuration & Persistence Subsystem (`kernel/gui/config.c` / `config.h`): versioned `desktop.cfg` format (Schema v2) with magic header (`0x4A434647`), checksum validation, safe defaults fallback, mouse sensitivity scaling, notification timeouts, AI backend preference, and 3 desktop themes (JARVIS Dark, Cyberpunk, Classic Slate).
   - 6 Dedicated desktop applications: Graphical Terminal (`jarvis>`), System Monitor (live CPU & memory gauges), File Browser (JARVIS-FS RAMDisk explorer), Process & Scheduler Monitor, JARVIS AI Companion, and Modular System Settings (`APP_ID_SETTINGS = 6`).
   - Interactive AI Desktop Companion Window (`desktop.c` / `desktop.h`): 24-line scrollable chat history, live kernel telemetry inspection (`timer_ticks`, `page_get_stats`, `scheduler_get_stats`, `fs_get_inode`), 4-tier allowlist sandbox, interactive privileged/destructive action confirmation (`/panic`, `/format` -> `/confirm <action>`), provenance labeling (`[LIVE_KERNEL]`, `[HOST_AI_MODEL]`), and nonblocking interrupt-safe rendering.
   - Modular System Settings Window: 6 modular configuration tabs (Display, Appearance, Input, Audio, AI Backend, System Info) with explicit hardware disclosure (audio unbacked notice, fixed $1024 \times 768$ mode disclosure, and RAMDisk volatility warnings).
   - Bounded Notification Subsystem (`kernel/gui/notification.c` / `notification.h`): ring buffer capped at 8 entries, severity categorization (`INFO`, `WARN`, `ERROR`), tick-based automatic expiration, oldest non-critical eviction under pressure, visual toast overlays with dismissal buttons (`[X]`), top-bar badge count, and zero-stall event handling.
   - Dual-channel PS/2 mouse driver with packet decoding and hardware arrow cursor.
   - Zero-regression console fallback: `ESC` returns cleanly to text console shell.

7. **System Diagnostics, Exception Handling & Controlled Recovery (Phase 18)**:
   - Centralized system telemetry (`kernel/diagnostics/diagnostics.c` / `diagnostics.h`): aggregates live metrics across IRQ counters (IRQ0/1/12, syscalls, yields, faults), scheduler switches, physical frame and heap metrics, and filesystem operations/errors/rollbacks via the `diag` shell command.
   - Bounded Diagnostic Ring-Buffer Log (`dmesg`): 32-entry static circular buffer with tick timestamps, severity tags (`DEBUG`, `INFO`, `WARN`, `ERROR`, `PANIC`), and safe overwrite eviction under pressure with zero dynamic memory allocation.
   - 32-Vector Hardware CPU Exception Handling: complete suite of IDT exception stubs in `interrupts.S` establishing standardized 176-byte `exception_frame_t` contexts for both error-code and non-error-code exceptions.
   - Comprehensive Serial Crash Dumps: transmits RIP, CS, RFLAGS, RSP, SS, registers RAX-R15, CR2, and decoded page-fault error codes directly over COM1 serial (`0x3F8` / `0xE9`).
   - Controlled Kernel Panic Path: safely halts execution (`cli; hlt`) rather than masking defects behind infinite automated reboot loops.
   - Controlled Shutdown & Restart (`kernel/drivers/power.c` / `power.h`): clean ACPI poweroff via QEMU port `0x604`, CPU reset via 8042 keyboard controller (`0x64`, `0xFE`), and triple-fault fallback.
   - Transparent Volatile Storage Disclosure: power management operations explicitly warn that storage is backed by a volatile RAMDisk (`/dev/ram0`), ensuring clear operational boundaries.
   - Component-Independent Recovery: GUI application failure cleanly resets app state to `APP_STATE_STOPPED` without desktop panics, desktop exit restores text console shell, and AI companion disconnection falls back to offline simulation safely.

8. **Hardware Abstraction Layer (HAL) & Modular Device Registry (Phase 19)**:
   - Unified Driver Interface (`kernel/hal/hal.c` / `hal.h`): standardizes device access, capabilities (`READ`, `WRITE`, `INTERRUPT`, `SEEK`, `DMA`, `POWER_MGMT`, `GRAPHICS`, `FALLBACK`), and lifecycle states (`REGISTERED`, `INITIALIZING`, `ONLINE`, `OFFLINE`, `FAULT`).
   - Bounded Static Registry: strictly avoids dynamic memory allocation during boot, supporting up to 16 statically registered drivers (`HAL_MAX_DRIVERS = 16`).
   - 9 Core Driver Adapters: Dual 8259 PIC (`pic_8259`), 16550 UART Serial (`serial_uart`), VGA 80x25 Console (`vga_terminal`), 8254 PIT Timer (`pit_8254`), 8042 PS/2 Keyboard (`ps2_keyboard`), PS/2 Mouse (`ps2_mouse`), Linear Framebuffer (`framebuffer`), RAMDisk Block Device (`ramdisk_block`), and ACPI Power Controller (`acpi_power`).
   - Multi-Pass Dependency Resolution: topological dependency engine resolving prerequisites (`pit_8254`, `ps2_keyboard`, and `ps2_mouse` requiring `pic_8259` to be online), enforcing dependency order, rejecting unmet dependencies (`HAL_ERR_DEP_UNMET`), and shutting down in reverse order.
   - Non-Destructive Hardware Probes: architecture-safe probing via UART scratchpad register readback (`0x3FF`), 8042 PS/2 controller status check (`0x64`), and Multiboot framebuffer structure validation without unbacked PCI/ACPI claims.
   - Console Fallback Routing: `hal_console_write` dynamically mirrors to VGA and COM1 serial, falling back to serial if the display is offline or uninitialized.
   - Interactive Shell Status (`hal` / `devices`): displays formatted driver table with unique identifiers, device classes, operational states, physical backends, dependencies, and IRQ allocations.

9. **Modular Zero-Heap Networking Stack (Phase 20)**:
   - Network-Device Abstraction (`kernel/net/netdev.c` / `netdev.h`): uniform `net_device_t` supporting MTU enforcement (1500B), bounded 16-packet circular receive queues, monotonic packet/byte telemetry, and zero dynamic heap allocation in packet paths.
   - Transparent Loopback & Emulated NIC: distinct memory-backed loopback adapter (`lo0`, MAC `02:00:00:00:00:01`, IP `127.0.0.1`) and QEMU RTL8139 hardware-emulated NIC (`eth0`, MAC `52:54:00:12:34:56`, IP `10.0.2.15`), without unbacked claims of live internet connectivity.
   - Ethernet II Framing (`kernel/net/ethernet.c` / `ethernet.h`): 14-byte MAC header parsing, automatic minimum 60-byte padding, unicast/broadcast MAC filtering, and EtherType demultiplexing (`0x0806` ARP, `0x0800` IPv4).
   - Dynamic ARP Protocol (`kernel/net/arp.c` / `arp.h`): 28-byte ARP header handling, 16-entry static lookup table with FIFO eviction, dynamic request/reply dispatch, and interactive `arp` table inspection.
   - IPv4 Network Layer (`kernel/net/ipv4.c` / `ipv4.h`): 20-byte IPv4 packet validation, 16-bit one's complement Internet checksum computation (RFC 1071), dotted-decimal string parsing, loopback IP matching, and ICMP/UDP protocol demux.
   - ICMP Diagnostics (`kernel/net/icmp.c` / `icmp.h`): Type 8 Echo Request generation, Type 0 Echo Reply processing, sequence number and checksum verification, and ping round-trip telemetry.
   - UDP Transport Layer (`kernel/net/udp.c` / `udp.h`): 8-byte UDP header verification, length validation, 16-slot port registry, and callback dispatch.
   - DHCP Client & Fallback (`kernel/net/dhcp.c` / `dhcp.h`): RFC 2131 state machine (`INIT` -> `DISCOVERING` -> `REQUESTING` -> `BOUND` / `FAILED`), message framing with magic cookie `0x63825363`, options parsing, and safe static fallback (`10.0.2.15`, netmask `255.255.255.0`, gateway `10.0.2.2`, DNS `10.0.2.3`).
   - Diagnostic Shell Commands: `ifconfig` / `ip` (interface telemetry), `arp` (ARP cache), `netstat` (protocol stats), `ping <ip>` (ICMP echo probe), and `nettest` (full networking invariant test suite).

10. **Security & Access Control Subsystem (Phase 21)**:
    - Granular 32-Bit Permission Model (`kernel/security/security.h` / `security.c`): explicit bitmask permissions (`PERM_READ_DIAG`, `PERM_FS_READ`, `PERM_FS_WRITE`, `PERM_PROC_OP`, `PERM_NET_ACCESS`, `PERM_AI_EXEC`, `PERM_ADMIN_OP`) and 5-tier role hierarchy (`ANONYMOUS`, `GUEST`, `USER`, `AI_AGENT`, `ADMIN`) with deny-by-default execution policy.
    - Transparent Privilege Model Disclosure: explicit disclosure that the kernel operates at CPL 0 / Ring 0 with identity-mapped memory; software access controls provide logical policy boundaries rather than hardware Ring 3 user page isolation.
    - Constant-Time Elevation Token Authentication: admin privilege escalation requires valid elevation token verified in constant time to thwart timing analysis.
    - Kernel Boundary Validation: enforces buffer range and wrap-around checks (`security_validate_buffer`), directory traversal sanitization rejecting `..` sequences (`security_validate_path`), and permission gates on syscalls, IPC message queues, and network packet transmission.
    - Host AI Companion Hardening (`jarvis/tools/registry.py`): strict schema validation, input argument size bounds (1024B arg, 8192B payload, 4096B output), dangerous shell metacharacter regex filters, recursive credential/secret redaction (`[REDACTED]`), and confirmation requirements for destructive operations.
    - Bounded In-Kernel Circular Audit Buffer: 32-entry static circular ring buffer logging timestamp, role, event type, action (ALLOW/DENY), subsystem, and description with safe FIFO eviction.
    - Interactive Security Commands: `secstat` (status & telemetry), `whoami` (active principal), `auth <role> [token]` (role switching), and `audit` (security event log inspection).

---

## Quickstart

### Prerequisites
- `gcc`, `make`, `python3`, `qemu-system-x86_64`, `xorriso`, `grub-mkrescue`
- `x86_64-elf-gcc` and `x86_64-elf-ld` (already present in environment)

### Build Commands

```bash
# Build kernel binary
make

# Generate bootable ISO
make iso

# Run interactive QEMU session (VGA + Serial)
make run

# Run headless QEMU session (Serial only)
make run-headless
```

### Testing Commands

```bash
# Run host algorithm unit tests (C)
make test

# Run AI companion unit tests (Python)
make test-ai

# Run automated QEMU boot and shell verification
make test-qemu

# Run QEMU native graphical desktop visual smoke test
make test-gui

# Run 13-step automated release and integration workflow
make test-release

# Run all test suites
make test-all

# Run unified end-to-end demo script (Phase 8-10 verification & boot)
./scripts/demo.sh

# Run interactive QEMU shell demo via script
./scripts/demo.sh --interactive

# Run release demonstration script
./scripts/release_demo.sh
```

---

## Interactive Shell Reference

Launch `make run` or `make run-headless` to interact with the `jarvis>` shell:

| Command | Description |
|---------|-------------|
| `help` | Display list of available commands |
| `status` | Show kernel state, uptime ticks, context switches, preemptions, and free pages |
| `ticks` | Display current PIT timer tick counter |
| `irq` | Display interrupt subsystem status (IRQ0/1, IDT gates 0x20/0x21/0x80/0x81) |
| `process` / `threads` / `ps` | Display process table, thread states, priorities, and stacks |
| `scheduler` | Display Round-Robin quantum and scheduler statistics |
| `memory` | Display physical page allocator statistics and available frames |
| `pftest` | Test safe page-fault diagnostics and address decoding without crashing |
| `gui` / `desktop` | Launch native dark graphical desktop with terminal, monitor & AI chat |
| `sched` | Run CPU Scheduling Benchmark (FCFS, SJF, Round Robin) |
| `paging` | Run Page Replacement Benchmark and Belady's Anomaly demo |
| `fit` | Run Memory Allocation Fit demo (First, Best, Worst Fit & fragmentation) |
| `alloc` | Run File Allocation Methods demo (Contiguous, Linked, Indexed) |
| `deadlock` | Run Banker's Deadlock Avoidance and safe sequence search |
| `detect` | Run Deadlock Detection (WFG/Matrix) & Havender Prevention demo |
| `disk` | Run Disk Head Scheduling (FCFS, SSTF, SCAN, C-SCAN) |
| `sync` | Run Producer-Consumer and Reader-Writer synchronization tests |
| `files` | Run JARVIS-FS filesystem formatting, file creation, and directory listing |
| `ls` | List files stored in JARVIS-FS with sizes and block counts |
| `cat <file>` | Display content of a file stored in JARVIS-FS (e.g. `cat kernel.cfg`) |
| `touch <file>` | Create an empty file in JARVIS-FS |
| `write <f> <txt>` | Write text payload to a file |
| `trunc <f> <sz>` | Truncate file to specified byte size (frees or allocates blocks) |
| `rm <file>` | Delete file from JARVIS-FS (reclaims direct blocks and inode) |
| `df` / `fsstat` | Display filesystem capacity, block allocation, and inode metrics |
| `fstest` | Run in-kernel storage, filesystem, and scheduling invariant tests |
| `syscall` | Run software interrupt `int $0x80` System Call Dispatch demo |
| `ipc` | Run Bounded Message Queue IPC test |
| `panic` | Trigger a test kernel panic with CR2/CR3/RSP register dump and red banner |
| `ai` | Display AI companion protocol and active allowlisted tools |
| `hal` / `devices` | Display registered hardware devices, states, and capabilities |
| `diag` / `telemetry` | Display system diagnostics counters (IRQs, switches, memory, FS) |
| `dmesg` | Display circular kernel diagnostic event log |
| `ifconfig` / `ip` | Display network interface configuration, MAC/IP, and TX/RX counters |
| `arp` | Display ARP cache table entries and IP-to-MAC resolution status |
| `netstat` | Display networking protocol packet and byte telemetry counters |
| `ping <ip>` | Send ICMP Echo Request probe to target IPv4 address |
| `nettest` | Run complete in-kernel networking stack invariant test suite |
| `secstat` / `security` | Display security subsystem status, active role, permissions mask, and audit metrics |
| `whoami` | Display active principal role identifier, name, and elevation state |
| `auth <role> [token]` | Switch security principal role (`anon`, `guest`, `user`, `ai`, `admin`) |
| `audit` | Display circular kernel security audit event log |
| `poweroff` / `shutdown` | Controlled ACPI system poweroff |
| `reboot` | Controlled CPU reset via 8042 controller |
| `tests` | Run the complete in-kernel benchmark suite |
| `clear` | Clear the screen |

---

## Documentation

Detailed technical architecture, reports, and design docs are available in `docs/`:

### Release Candidate Deliverables (v1.0.0-RC1)
- [`docs/RELEASE_NOTES.md`](docs/RELEASE_NOTES.md): Official Release Notes for v1.0.0-RC1, ISO checksums, toolchain matrix, and subsystem summaries.
- [`docs/DEMO_GUIDE.md`](docs/DEMO_GUIDE.md): End-to-end demonstration guide with step-by-step reproduction instructions for all OS capabilities.
- [`docs/KNOWN_LIMITATIONS.md`](docs/KNOWN_LIMITATIONS.md): Transparent catalog of architectural limitations, RAMDisk volatility, and security boundaries.
- [`docs/RELEASE_CHECKLIST.md`](docs/RELEASE_CHECKLIST.md): 30-point pre-release verification checklist, security audit findings, and verification matrix.
- [`docs/FEATURE_TRACEABILITY.md`](docs/FEATURE_TRACEABILITY.md): 66-point complete feature requirement traceability matrix (100% verified complete).

### Final Project Reports & Submission Deliverables
- [`docs/FINAL_ACCEPTANCE_REPORT.md`](docs/FINAL_ACCEPTANCE_REPORT.md): Phase 9 independent correctness audit, test reproduction, and final acceptance verdict.
- [`docs/FINAL_PROJECT_REPORT.md`](docs/FINAL_PROJECT_REPORT.md): Comprehensive final project technical report covering all 14 project sections (Abstract through References).
- [`docs/FINAL_VERIFICATION.md`](docs/FINAL_VERIFICATION.md): Phase 8 final release verification evidence package, test logs, and capability matrix.
- [`docs/ARCHITECTURE_DIAGRAMS.md`](docs/ARCHITECTURE_DIAGRAMS.md): Full set of editable Mermaid diagrams covering kernel, memory, scheduling, filesystem, and AI architecture.
- [`docs/PRESENTATION_OUTLINE.md`](docs/PRESENTATION_OUTLINE.md): Engineering defense and presentation slide outline with anticipated Q&A.

### Phase Verification Reports
- [`docs/PHASE21_SECURITY.md`](docs/PHASE21_SECURITY.md): Phase 21 security and access control, RBAC permissions, kernel boundary hardening, AI tool execution control, and audit log report.
- [`docs/PHASE20_NETWORKING.md`](docs/PHASE20_NETWORKING.md): Phase 20 modular zero-heap networking stack, Ethernet, ARP, IPv4, ICMP, UDP, and DHCP report.
- [`docs/PHASE19_HAL.md`](docs/PHASE19_HAL.md): Phase 19 hardware abstraction layer, driver registry, dependencies, and probes report.
- [`docs/PHASE18_DIAGNOSTICS_RECOVERY.md`](docs/PHASE18_DIAGNOSTICS_RECOVERY.md): Phase 18 system diagnostics, 32-vector exception crash dumps, and controlled recovery report.
- [`docs/PHASE17_SETTINGS_NOTIFICATIONS.md`](docs/PHASE17_SETTINGS_NOTIFICATIONS.md): Phase 17 modular system settings application, configuration schema v2, and bounded notification subsystem report.
- [`docs/PHASE16_AI_DESKTOP.md`](docs/PHASE16_AI_DESKTOP.md): Phase 16 AI desktop experience, live kernel telemetry, and allowlist sandbox security report.
- [`docs/PHASE13_STABILITY_REPORT.md`](docs/PHASE13_STABILITY_REPORT.md): Phase 13 security audit, stability hardening, stress testing, and performance baselines report.
- [`docs/PHASE12_APP_FRAMEWORK.md`](docs/PHASE12_APP_FRAMEWORK.md): Phase 12 application framework, desktop configuration, themes, and persistence report.
- [`docs/PHASE11_GUI_VERIFICATION.md`](docs/PHASE11_GUI_VERIFICATION.md): Phase 11 window manager, event queue, dragging, and desktop application suite report.
- [`docs/PHASE10_GUI.md`](docs/PHASE10_GUI.md): Phase 10 native graphical desktop specification, visual tests, and architecture report.
- [`docs/SETUP_AND_BUILD.md`](docs/SETUP_AND_BUILD.md): Step-by-step setup, toolchain requirements, build, and run guide.
- [`docs/PHASE7_VERIFICATION.md`](docs/PHASE7_VERIFICATION.md): Phase 7 integration and release validation report.
- [`docs/PHASE6_VERIFICATION.md`](docs/PHASE6_VERIFICATION.md): Phase 6 AI integration and local intelligence verification report.
- [`docs/PHASE5_VERIFICATION.md`](docs/PHASE5_VERIFICATION.md): Phase 5 storage and filesystem reliability verification report.
- [`docs/PHASE4_VERIFICATION.md`](docs/PHASE4_VERIFICATION.md): Phase 4 IPC, synchronization, and deadlocks verification report.
- [`docs/PHASE3_VERIFICATION.md`](docs/PHASE3_VERIFICATION.md): Phase 3 process, context-switch, and memory correctness report.
- [`docs/PHASE2_VERIFICATION.md`](docs/PHASE2_VERIFICATION.md): Phase 2 hardware, interrupt, and IRQ1 verification report.

### Deep Dives & Specifications
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): Comprehensive system architecture and interrupt flow.
- [`docs/IMPLEMENTATION_STATUS.md`](docs/IMPLEMENTATION_STATUS.md): Implementation status tracker with verified evidence.
- [`docs/BUILD_AND_RUN.md`](docs/BUILD_AND_RUN.md): Build targets, toolchains, and QEMU options.
- [`docs/OS_CONCEPTS.md`](docs/OS_CONCEPTS.md): Academic formulations of scheduling, paging, Banker's algorithm, and disk seek.
- [`docs/AI_ARCHITECTURE.md`](docs/AI_ARCHITECTURE.md): Host companion design, security allowlists, and local LLM backends.
- [`docs/TESTING.md`](docs/TESTING.md): Verification methodology across host tests, Python tests, and QEMU smoke tests.
- [`docs/LIMITATIONS.md`](docs/LIMITATIONS.md): Architectural boundaries, uniprocessor assumptions, and educational scope.


