# JARVIS-OS — Known Limitations & Architectural Boundaries

**Milestone:** Phase 14 Release Candidate (v1.0.0-RC1)  
**Target:** Evaluators, Students & Academic Reviewers  
**Commitment:** Absolute technical honesty regarding implemented capabilities versus simulated or bounded features.

---

## 1. Memory & Address Space Boundaries

- **Single Identity-Mapped Region (First 2 MiB)**: Bootloader page tables (`kernel/arch/x86_64/boot.S`) identity-map the first 2 MiB of physical memory using a single 2 MiB huge page (`0x00000000 - 0x001FFFFF`). Any access to virtual addresses $\ge$ 2 MiB (`0x00200000`) triggers a Page Fault (`#PF`, vector 14).
- **Physical Page Allocator Pool**: While Multiboot discovers the full system memory map (e.g. 256 MiB in QEMU), the current bitmap allocator (`kernel/memory/page.c`) manages a static pool of 64 frames (256 KiB) starting after the kernel binary for deterministic educational predictability.
- **Kernel Heap Bounds**: The dynamic heap (`kernel/memory/heap.c`) operates in a preallocated static buffer of 64 KiB (`KERNEL_HEAP_SIZE = 64 * 1024`). It enforces 16-byte System V AMD64 alignment and coalescing of adjacent free blocks, but does not dynamically grow beyond 64 KiB via `sbrk` or paging.
- **Single Kernel Privilege Level (Ring 0 Monolithic)**: All kernel threads, tasks, and shell routines execute in CPU Ring 0 (Supervisor Mode) sharing the single kernel address space. The system call interface (`int $0x80`) verifies software interrupt gate dispatch, argument passing, and System V register frame restoration honestly within Ring 0 without claiming hardware multi-ring user-space isolation.

---

## 2. Storage Volatility & Persistence Boundaries

- **RAM Disk Volatility**: The block device (`kernel/storage/ramdisk.c`) is allocated as an in-memory buffer (`128 sectors \times 512 bytes = 64 KiB`).
  - **Session Persistence**: Files created in JARVIS-FS and desktop settings saved to `/desktop.cfg` **persist across the running OS session**, surviving shell commands, background thread switches, and repeated toggles between the text shell and the GUI.
  - **Cold Reboot Reality**: Because there is no physical IDE, AHCI, SATA, or NVMe block driver writing back to host disk sectors, **all filesystem changes and desktop configurations reset to defaults upon a cold hardware reboot / power cycle**.
- **Flat Namespace & Capacity Limits**: JARVIS-FS supports a single flat directory with up to 16 files (`FS_MAX_FILES = 16`) and filenames up to 27 characters. Hierarchical subdirectories (`/dir/file`) are not supported.
- **Direct Block Pointers**: Inode structures feature 8 direct block pointers (`FS_DIRECT_BLOCKS = 8`), imposing a hard file size limit of 4,096 bytes (4 KiB). Indirect and indexed block allocations are evaluated in educational simulations (`kernel/algorithms/file_alloc.c`).

---

## 3. Processor & Concurrency Execution Model

- **Uniprocessor PIC Architecture**: JARVIS-OS targets a single CPU core using legacy Intel 8259 PIC controllers and the 8254 PIT timer. Modern APIC, x2APIC, and Symmetric Multiprocessing (SMP) across multiple cores are not implemented.
- **Synchronization Primitives**: Concurrency primitives (mutexes, counting semaphores) utilize atomic compiler built-ins (`__atomic_compare_exchange_n`, `__sync_fetch_and_add`) and uniprocessor preemption controls suitable for single-core multitasking.
- **Semaphore Wait Queue Bounds**: Semaphores support up to 8 waiting task PIDs (`MAX_WAITING_TASKS = 8`). Attempting to block more than 8 tasks returns `SEMAPHORE_ERR_QUEUE_FULL` (-2).
- **IPC Multi-Queue Bounds**: The IPC subsystem supports 4 queues (`IPC_MAX_QUEUES = 4`), each with a capacity of 16 messages of 64 bytes. Senders to full queues receive `IPC_ERR_FULL` (-1).

---

## 4. Graphics & Window Management Scope

- **Software Rasterization (No GPU Acceleration)**: The native GUI renders directly to a VBE/Bochs BGA $1024 \times 768 \times 32\text{ bpp}$ linear framebuffer via software blitting routines. Hardware 3D acceleration (OpenGL/Vulkan) is not implemented.
- **Bounded Window Pool**: The Window Manager manages a static pool of 8 windows (`WM_MAX_WINDOWS = 8`) to guarantee strictly bounded memory usage and eliminate dynamic heap fragmentation during window management.
- **Event Queue Bounded Overflow**: The GUI event queue holds up to 64 events (`GUI_EVENT_QUEUE_SIZE = 64`). Under input bursts exceeding 64 unprocessed events, excess events are safely dropped with an incremented `dropped_count` to protect kernel memory from corruption.

---

## 5. Real Kernel Subsystems vs Educational Simulators

To ensure complete academic transparency, the following table distinguishes real bare-metal runtime mechanisms from educational algorithm models:

| Subsystem | Real Kernel Implementation | Educational Algorithm Simulation |
|---|---|---|
| **CPU Scheduling** | Round-Robin PIT preemption (5-tick quantum) + cooperative `int $0x81` in `scheduler.c` | FCFS, SJF, and Round Robin simulator computing Gantt charts, turnaround time, and wait times in `sched_sim.c` |
| **Physical Memory** | Bitmap allocator managing 64 physical 4096-byte frames in `page.c` + heap allocator in `heap.c` | First Fit, Best Fit, and Worst Fit dynamic partition simulators computing external fragmentation in `mem_fit.c` |
| **Virtual Memory** | Identity-mapped 2 MiB huge page in `boot.S` + `#PF` exception handler in `idt.c` | FIFO, LRU, and Optimal page replacement simulators with Belady anomaly confirmation in `page_replace.c` |
| **Storage & Files** | RAM disk block device in `ramdisk.c` + JARVIS-FS inode filesystem with rollback in `fs.c` | Contiguous, Linked, and Indexed block allocation simulators with lookup overhead comparisons in `file_alloc.c` |
| **Deadlocks** | Atomic mutex locks with owner PID validation and recursive deadlock rejection in `sync_demo.c` | Banker's safety algorithm in `banker.c` + Wait-For Graph cycle detection and Havender ordering in `deadlock.c` |
| **Disk Scheduling** | In-memory RAM disk sector addressing in `ramdisk.c` | FCFS, SSTF, SCAN, and C-SCAN seek calculations on 200-track geometry in `disk_sched.c` |

---

## 6. AI Companion Host Architecture & Security Boundaries

- **Host-Side Execution**: The AI companion service (`jarvis/`) runs on the host workstation communicating over COM1 serial / virtual PTY. Large Language Model weights (e.g. Ollama daemon, GGUF models) are not bundled inside the kernel binary or bootable ISO.
- **Offline Fallback**: When Ollama or local LLM weights are unavailable, the companion automatically falls back to deterministic local mock responses without failing.
- **Tool Allowlist & Permissions**: The companion cannot execute arbitrary shell scripts or bypass registered interfaces. All operations are strictly constrained to 8 allowlisted tools categorized across 4 permission tiers, with privileged and destructive actions requiring user confirmation.
- **Truthful Provenance**: Model outputs explicitly declare whether telemetry originates from `LIVE_KERNEL` (via active serial transport) or `OFFLINE_SIMULATION` (synthetic test fixtures).

