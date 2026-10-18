# JARVIS-OS — v1.0.0-RC1 Release Notes

**Release**: JARVIS-OS Version 1.0.0-RC1 (Release Candidate 1)  
**Date**: October 2026  
**Target Architecture**: x86-64 Long Mode Bare-Metal Kernel + Host AI Companion  
**Primary Repository**: `https://github.com/kuntalsaha05/jarvis-os` (`main` branch)  
**Release ISO Artifact**: `build/jarvis.iso` (5.1 MB, SHA-256: `0287d268ee73ecef8fbd201786db6178596a2efb40d45566f7b1d63f19d72178`)

---

## 1. Executive Overview

JARVIS-OS v1.0.0-RC1 represents the complete, fully integrated release candidate of an educational operating system written from scratch for x86-64 bare metal, featuring an integrated 32-bpp graphical desktop, a bona fide window manager, and a modular host-side AI companion.

The operating system combines real bare-metal kernel mechanisms (interrupt routing, involuntary timer preemption, physical page allocation, IPC queues, synchronization, and an inode filesystem with atomic rollback) with educational algorithm models (CPU scheduling, Belady anomaly demonstration, Banker's deadlock avoidance, disk seek head simulation) and a modern native desktop environment.

Every feature claimed in this release candidate is verified by automated regression tests, in-kernel bare-metal checkpoints, and screendump visual tests under QEMU.

---

## 2. Key Capabilities & Architectural Highlights

### 2.1 Bare-Metal x86-64 Long Mode Kernel
- **Bootloader & Memory Hand-Off**: Boots via GRUB into 64-bit Long Mode. Discovers physical BIOS/GRUB e820 memory segments and hand-offs Multiboot 1 tables.
- **Interrupt Subsystem**: Complete 256-gate Interrupt Descriptor Table (IDT). Legacy Intel 8259 PIC remapped to vectors 32–47. PIT timer Channel 0 configured at 100 Hz (`timer_ticks` incremented every 10 ms).
- **Dual Console & Diagnostics**: Simultaneous output to VGA text buffer (`0xB8000`), COM1 serial port (`0x3F8`), and QEMU/Bochs debug port (`0xE9`). Safe `#PF` page fault decoder reporting CR2 and error flags. Dedicated `kernel_panic` handler with register dumps and red banner.
- **Preemptive Process Scheduling**: Thread Control Blocks (TCB) managing processes across `NEW`, `READY`, `RUNNING`, `BLOCKED`, and `TERMINATED` states. PIT timer-driven involuntary Round-Robin preemption across 5-tick quanta. Dedicated idle task (PID 0) executing `sti; hlt`. System V AMD64 ABI 16-byte stack frame preservation (`%rbx`, `%r12-%r15`) across all switches.
- **Memory Management**: Physical bitmap page allocator managing 64 frames (256 KiB pool) with bounds checking and double-free rejection. Kernel dynamic heap allocator with 16-byte alignment, splitting, and adjacent block coalescing.
- **Storage & Resilient Filesystem (JARVIS-FS)**: Block device abstraction backed by in-memory RAM disk (128 sectors $\times$ 512 bytes = 64 KiB). Superblock magic `0x4A415256`, inode table, and 8 direct block pointers. Features atomic rollback on allocation failure, file truncation (`trunc`), path normalization, and block reuse.
- **System Call Interface**: Software interrupt `int $0x80` routed through IDT gate 128 (DPL 3) preserving GPRs and dispatching kernel system calls.

### 2.2 Native Graphical Desktop & Window Manager
- **Linear Framebuffer**: VBE / Bochs BGA $1024 \times 768 \times 32\text{ bpp}$ linear framebuffer auto-discovery (`0xFD000000`).
- **Bitmap Font & Primitive Rasterizer**: Embedded 8x16 font rendering ASCII characters, rectangles, outlines, and Bresenham lines with screen clipping.
- **True Window Manager (`wm.c` / `wm.h`)**:
  - Bounded 8-window pool with active z-order promotion.
  - Interactive window title bar dragging via mouse with screen boundary clamping.
  - Window controls: Minimize (`_`), Maximize (`[]`), and Close (`X`).
- **Interrupt-Decoupled GUI Event Queue (`event.c` / `event.h`)**: Lockless circular buffer (64 events) enqueuing keyboard and mouse events; drops overflow safely without corrupting kernel state.
- **Reusable Application Framework (`app.c` / `app.h`)**: Structured application descriptors (`app_desc_t`) managing lifecycle states (`REGISTERED`, `RUNNING`, `MINIMIZED`, `STOPPED`) and decoupled window bindings.
- **Desktop Settings & Persistence (`config.c` / `config.h`)**: Versioned binary configuration (`desktop.cfg`) with `0x4A434647` magic, 32-bit polynomial checksum, safe defaults fallback, and 3 switchable themes:
  - *JARVIS Dark*: Midnight Slate background, Dark Steel panels, JARVIS Cyan accent.
  - *Cyberpunk*: Deep Violet background, Dark Neon Purple panels, Electric Magenta accent.
  - *Classic Slate*: Classic Slate Grey background, Charcoal panels, Terminal Emerald accent.
- **Five Desktop Applications**:
  1. **Terminal Application**: Interactive shell in a GUI window (`jarvis>`).
  2. **System Monitor**: Live CPU frequency, timer ticks, and memory page pool gauges.
  3. **File Browser**: Directory explorer and block inspection on JARVIS-FS.
  4. **Process Monitor**: Live PID process table and Round-Robin scheduler telemetry.
  5. **JARVIS AI Companion**: COM1 protocol bridge with Ollama status and 4-tier security indicator.
- **Console Fallback**: Pressing `ESC` or clicking taskbar exit cleanly restores 80x25 text mode and shell.

### 2.3 Modular AI Companion & Security Architecture
- **Pluggable Backends (`LLMInterface`)**: Ollama backend (default local daemon on `localhost:11434`), llama.cpp (GGUF), AirLLM, and deterministic Mock backend for CI.
- **Eight-Step Autonomous Agent Loop**: Structured pipeline accepting queries, building context, selecting tools, validating schemas, gating privileged actions, executing within 5s deadlines, and synthesizing responses.
- **Four-Tier Security Sandbox**: Tools categorized as `READ_ONLY`, `SAFE_DIAGNOSTIC`, `PRIVILEGED`, or `DESTRUCTIVE`. State-altering actions (`system.trigger_panic`, `storage.format_disk`) require explicit user confirmation.
- **Truthful Provenance**: All responses explicitly tag whether data originates from `LIVE_KERNEL` or `OFFLINE_SIMULATION`.
- **Payload Safety**: Host/guest messages bounded by a strict `MAX_MESSAGE_SIZE = 65536` (64 KiB) limit.

---

## 3. Toolchain & Build Environment

| Tool | Required Version | Host Tested Version |
|---|---|---|
| **Bare-Metal C Compiler** | `x86_64-elf-gcc` $\ge 12.0$ | `x86_64-elf-gcc 14.2.0` |
| **Bare-Metal Linker** | `x86_64-elf-ld` $\ge 2.38$ | `GNU ld 2.43.1` |
| **Host C Compiler** | `gcc` $\ge 11.0$ | `gcc 13.3.0` (Ubuntu 24.04) |
| **Python Runtime** | `python3` $\ge 3.10$ | `Python 3.12.3` |
| **ISO Generation** | `grub-mkrescue` + `xorriso` | `xorriso 1.5.6` |
| **Virtual Machine Emulator** | `qemu-system-x86_64` $\ge 7.0$ | `QEMU emulator 8.2.2` |

---

## 4. Verification & Quality Assurance Summary

```
==================================================
   JARVIS-OS RELEASE & INTEGRATION VALIDATION     
==================================================
Host Unit Tests (17 Suites):                     PASS
AI Companion Test Suite (22 Tests):              PASS
Bare-Metal QEMU Smoke Test (65 Checkpoints):     PASS
Native GUI Visual Smoke Tests (6 Screendumps):   PASS
End-to-End Release Validation (13 Steps):        PASS
Host Performance Benchmark Suite:                PASS
==================================================
   RELEASE STATUS: CANDIDATE VERIFIED & READY     
==================================================
```

### 4.1 Measured Performance Baselines
- **Heap Allocation Latency**: $0.0043\ \mu\text{s}$ per operation (234.9M ops/sec, 100% success rate).
- **Filesystem Write Latency**: $0.904\ \mu\text{s}$ per 512B write ($540.2\text{ MB/s}$ RAMDisk throughput).
- **Filesystem Read Latency**: $0.606\ \mu\text{s}$ per 512B read ($805.2\text{ MB/s}$ RAMDisk throughput).
- **Framebuffer Composition Cost**: $0.914\text{ ms}$ per full frame ($1094.5\text{ FPS}$ software blit).
- **GUI Event Queue Dispatch**: $0.0112\ \mu\text{s}$ per event pair (89.5M events/sec).
- **QEMU Bare-Metal Boot to Shell**: $2.20\text{ seconds}$.
- **Desktop Framework Launch**: $1.00\text{ second}$.
- **AI Tool Dispatch & Validation**: $396.71\ \mu\text{s}$ per call.

---

## 5. Known Limitations & Non-Goals

1. **Storage Persistence**: Storage is backed by an in-memory RAM disk (`kernel/storage/ramdisk.c`). Configuration changes saved to `/desktop.cfg` persist across the running session and between shell/GUI toggles, but reset to defaults upon a cold hardware reboot.
2. **CPU Model**: Operates on a single core using legacy PIC/PIT interrupt controllers. Multi-core SMP, APIC, and hardware virtualization are out of scope.
3. **Privilege Isolation**: Executes as a single-privilege kernel monolith in Ring 0. System call benchmarks (`int $0x80`) demonstrate gate transitions and frame integrity honestly within this architecture without claiming multi-ring TSS isolation.
4. **Graphics**: Software-only 2D rasterizer on VBE/BGA linear framebuffer without hardware GPU 3D acceleration.

---

## 6. How to Build & Run

```bash
# 1. Clean build and generate ISO
make clean && make iso

# 2. Run in QEMU with graphical window and serial console
make run

# 3. Run in headless mode for text-only / CI evaluation
make run-headless

# 4. Run full test and benchmark suite
make test-all
make bench-full
```

