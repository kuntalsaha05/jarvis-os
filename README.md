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

6. **Native Graphical Desktop & Window Manager (Phases 10 & 11)**:
   - High-resolution VBE/Bochs BGA $1024 \times 768 \times 32\text{ bpp}$ linear framebuffer auto-discovery (`0xFD000000`).
   - True Window Manager (`kernel/gui/wm.c` / `wm.h`) supporting window creation, destruction, z-order promotion, title bars, and boundary clipping.
   - Interactive window controls: Minimize (`_`), Maximize (`[]`), Close (`X`), and mouse dragging.
   - Decoupled GUI Event Queue (`kernel/gui/event.c` / `event.h`) processing mouse and keyboard events outside interrupt context.
   - 5 Dedicated desktop applications: Graphical Terminal (`jarvis>`), System Monitor (live CPU & memory gauges), File Browser (JARVIS-FS RAMDisk explorer), Process & Scheduler Monitor, and JARVIS AI Companion.
   - Dual-channel PS/2 mouse driver with packet decoding and hardware arrow cursor.
   - Zero-regression console fallback: `ESC` returns cleanly to text console shell.

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
| `tests` | Run the complete in-kernel benchmark suite |
| `clear` | Clear the screen |

---

## Documentation

Detailed technical architecture, reports, and design docs are available in `docs/`:

### Final Project Reports & Submission Deliverables
- [`docs/FINAL_ACCEPTANCE_REPORT.md`](docs/FINAL_ACCEPTANCE_REPORT.md): Phase 9 independent correctness audit, test reproduction, and final acceptance verdict.
- [`docs/FINAL_PROJECT_REPORT.md`](docs/FINAL_PROJECT_REPORT.md): Comprehensive final project technical report covering all 14 project sections (Abstract through References).
- [`docs/FINAL_VERIFICATION.md`](docs/FINAL_VERIFICATION.md): Phase 8 final release verification evidence package, test logs, and capability matrix.
- [`docs/ARCHITECTURE_DIAGRAMS.md`](docs/ARCHITECTURE_DIAGRAMS.md): Full set of editable Mermaid diagrams covering kernel, memory, scheduling, filesystem, and AI architecture.
- [`docs/PRESENTATION_OUTLINE.md`](docs/PRESENTATION_OUTLINE.md): Engineering defense and presentation slide outline with anticipated Q&A.

### Phase Verification Reports
- [`docs/PHASE10_GUI.md`](docs/PHASE10_GUI.md): Phase 10 native graphical desktop specification, visual tests, and architecture report.
- [`docs/FEATURE_TRACEABILITY.md`](docs/FEATURE_TRACEABILITY.md): 54-point complete feature requirement traceability matrix.
- [`docs/SETUP_AND_BUILD.md`](docs/SETUP_AND_BUILD.md): Step-by-step setup, toolchain requirements, build, and run guide.
- [`docs/RELEASE_CHECKLIST.md`](docs/RELEASE_CHECKLIST.md): Pre-release checklist, security audit findings, and remaining boundaries.
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


