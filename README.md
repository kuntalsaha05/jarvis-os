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
   - Identity-mapped PML4, PDPT, and Page Directory (2 MiB huge pages).
   - Global Descriptor Table (GDT), Interrupt Descriptor Table (IDT, 256 gates), 8259 PIC remap, and 100 Hz PIT timer.
   - Dual-output console: VGA text buffer (`0xB8000`), COM1 serial port (`0x3F8`), and QEMU/Bochs debug port (`0xE9`).
   - Dual keyboard input: PS/2 controller (`0x60/0x64`) and COM1 serial input with VT100 terminal handling.

2. **Preemptive & Cooperative Scheduling**:
   - Process Control Blocks (TCB), process creation, termination, and state tracking (NEW, READY, RUNNING, BLOCKED, TERMINATED).
   - Interrupt-driven Round-Robin preemption via PIT timer (5 ticks quantum).
   - Cooperative yielding via software interrupt `int $0x81`.
   - Exact 20-quadword x86-64 `iretq` frame restoration (15 GPRs + RIP + CS + RFLAGS + RSP + SS).

3. **Classic OS Algorithm Implementations & Demonstrations**:
   - **CPU Scheduling**: FCFS, SJF, and Round Robin with arrival time, burst time, completion time, turnaround time, and waiting time calculations.
   - **Page Replacement**: FIFO, LRU, and Optimal replacement simulation.
   - **Belady's Anomaly**: Deterministic demonstration showing FIFO page faults increase from 9 to 10 when frames increase from 3 to 4.
   - **Deadlock Avoidance**: Full Banker's Algorithm with Allocation, Max, Need ($Need = Max - Alloc$), and Available matrices; safety algorithm; and safe-sequence generation.
   - **Disk Head Scheduling**: FCFS, SSTF, SCAN (Elevator), and C-SCAN seek calculations and total head movement.
   - **Concurrency & Sync**: Counting semaphores, atomic spinlocks/mutexes, Producer-Consumer bounded buffer, and Reader-Writer problem.

4. **Storage & Minimal Filesystem (JARVIS-FS)**:
   - Block device abstraction with RAM disk backing (128 sectors of 512 bytes = 64 KiB).
   - Lightweight filesystem with superblock (magic `0x4A415256`), inode table, direct data blocks, creation, opening, reading, writing, and directory listing.

5. **AI-First Modular Host Companion**:
   - Host-side Python companion service (`jarvis/`) communicating over serial/CLI bridge.
   - Abstract `LLMInterface` supporting **MockLLMBackend** (offline verification), **LlamaCppBackend** (local quantized GGUF), and **AirLLMBackend** (layer-wise VRAM streaming).
   - Secure `ToolRegistry` with explicit security allowlists, parameter validation, and timestamped audit logs.

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

# Run all test suites
make test-all
```

---

## Interactive Shell Reference

Launch `make run` or `make run-headless` to interact with the `jarvis>` shell:

| Command | Description |
|---------|-------------|
| `help` | Display list of available commands |
| `status` | Show kernel state, uptime ticks, and free pages |
| `sched` | Run CPU Scheduling Benchmark (FCFS, SJF, Round Robin) |
| `paging` | Run Page Replacement Benchmark and Belady's Anomaly demo |
| `deadlock` | Run Banker's Deadlock Avoidance and safe sequence search |
| `disk` | Run Disk Head Scheduling (FCFS, SSTF, SCAN, C-SCAN) |
| `sync` | Run Producer-Consumer and Reader-Writer synchronization tests |
| `files` | Run JARVIS-FS filesystem formatting, file creation, and `ls` |
| `ipc` | Run Bounded Message Queue IPC test |
| `ai` | Display AI companion protocol and active allowlisted tools |
| `tests` | Run the complete in-kernel benchmark suite |
| `clear` | Clear the screen |

---

## Documentation

Detailed technical architecture and design docs are available in `docs/`:
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): Comprehensive system architecture and interrupt flow.
- [`docs/IMPLEMENTATION_STATUS.md`](docs/IMPLEMENTATION_STATUS.md): 28-point feature status tracker with verified evidence.
- [`docs/BUILD_AND_RUN.md`](docs/BUILD_AND_RUN.md): Build targets, toolchains, and QEMU options.
- [`docs/OS_CONCEPTS.md`](docs/OS_CONCEPTS.md): Academic formulations of scheduling, paging, Banker's algorithm, and disk seek.
- [`docs/AI_ARCHITECTURE.md`](docs/AI_ARCHITECTURE.md): Host companion design, security allowlists, and local LLM backends.
- [`docs/TESTING.md`](docs/TESTING.md): Verification methodology across host tests, Python tests, and QEMU smoke tests.
- [`docs/LIMITATIONS.md`](docs/LIMITATIONS.md): Architectural boundaries, uniprocessor assumptions, and educational scope.
