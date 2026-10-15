# JARVIS-OS — Known Limitations & Architectural Boundaries

In accordance with the project's educational mission, this document explicitly records the design boundaries, simulated vs native implementations, and current hardware limitations of JARVIS-OS.

---

## 1. Uniprocessor Hardware Assumptions

- **Interrupt Controller**: JARVIS-OS uses the legacy Intel 8259 PIC (Programmable Interrupt Controller) rather than the modern APIC/x2APIC.
- **Multiprocessing**: The kernel operates on a single CPU core. Concurrency primitives (mutexes, semaphores) use atomic test-and-set instructions and spin-pauses suitable for uniprocessor preemption. SMP (Symmetric Multiprocessing) is not implemented.

---

## 2. Privilege Levels & System Call Mechanism

- **Kernel Mode Execution**: All created processes (`task_a`, `task_b`, `preempt_task_1`, `preempt_task_2`, and the interactive shell) execute in CPU Ring 0 (Supervisor mode) sharing the kernel address space.
- **System Call Entry Mechanism**: Hardware/software interrupt entry is fully implemented via `int $0x80`. The interrupt descriptor table configures IDT gate `0x80` with DPL 3 (`0xEE`), assembly stub `syscall_stub` preserves all general-purpose registers (`rax..r15`), extracts arguments (`rax`=number, `rdi`=arg1, `rsi`=arg2, `rdx`=arg3), calls `syscall_dispatch`, and returns the result in `rax` via `iretq`. A C wrapper `syscall()` is provided in `kernel/syscall/syscall.c`.
- **True Ring-3 Hardware Boundary Requirements**: A full hardware user-mode boundary requires the following components which are outside the scope of this monolithic educational kernel:
  1. **Task State Segment (TSS)**: A 64-bit TSS descriptor loaded into the CPU Task Register (`ltr`) specifying `RSP0` so that hardware transitions from Ring 3 to Ring 0 have a clean, trusted kernel stack.
  2. **GDT Selectors for Ring 3**: User Code Descriptor (DPL 3, selector with RPL=3) and User Data Descriptor (DPL 3, selector with RPL=3).
  3. **Paging Permissions**: User pages must have the User/Supervisor bit set (`U/S = 1`), while kernel pages remain `U/S = 0` to prevent ring-3 code from tampering with kernel page tables and I/O ports.
  4. **Executable Loading**: An ELF64 parser and user process loader with separate virtual memory address spaces (`CR3` switching per process).
- **Educational Scope**: JARVIS-OS consciously operates as a single-privilege kernel monolith. All system call benchmarks execute honestly within this architectural reality without fabricating false user-space isolation.

---

## 3. Real Kernel Subsystems vs Educational Simulators

To maximize clarity for students and evaluators, JARVIS-OS cleanly distinguishes between real hardware mechanisms and algorithm simulators:

| Subsystem | Real Kernel Implementation | Educational Algorithm Simulator |
|---|---|---|
| **CPU Scheduling** | Round Robin PIT preemptive scheduler (5-tick quantum) + cooperative `int $0x81` in `kernel/process/scheduler.c` | Deterministic FCFS, SJF, and Round Robin benchmark with Gantt turnaround/wait calculations in `kernel/algorithms/sched_sim.c` |
| **Physical Memory** | Bitmap allocator for 64 physical 4096-byte frames (`kernel/memory/page.c`) + heap allocator (`kernel/memory/heap.c`) | First Fit, Best Fit, and Worst Fit dynamic partition simulators with external fragmentation metrics in `kernel/algorithms/mem_fit.c` |
| **Virtual Memory** | 4-level x86-64 paging identity mapping in `kernel/arch/x86_64/boot.S` and `#PF` vector 14 handler in `kernel/interrupts/idt.c` | FIFO, LRU, and Optimal page replacement simulators with Belady anomaly confirmation in `kernel/algorithms/page_replace.c` |
| **Storage & Files** | In-memory block device (`kernel/storage/ramdisk.c`) and JARVIS-FS inode filesystem (`kernel/storage/fs.c`) | Contiguous, Linked, and Indexed block allocation simulators with logical-to-physical lookups in `kernel/algorithms/file_alloc.c` |
| **Deadlock & Concurrency**| Atomic mutex locks, counting semaphores, bounded buffer, and reader-writer demo in `kernel/ipc/` | Banker's safety algorithm (`banker.c`) and Wait-For Graph / Matrix Deadlock Detection & Havender Prevention in `kernel/algorithms/deadlock.c` |

---

## 4. Storage & Persistence

- **RAM Disk**: The block device (`kernel/storage/ramdisk.c`) is allocated in RAM (128 sectors of 512 bytes = 64 KiB).
- **Volatile Storage**: Data written to JARVIS-FS files is retained in memory for the duration of the QEMU session, but is not persisted to physical host storage after the emulator shuts down. Real physical disk drivers (IDE, AHCI, NVMe) are not implemented.

---

## 5. Host-Side AI Inference Boundary & Telemetry Truthfulness

- **Host Service**: As designed, deep-learning models (AirLLM, llama.cpp) run as companion processes on the host workstation, not inside the bare-metal kernel.
- **Telemetry Labeling**: When the companion is operating offline or without an active serial transport, tool registry results explicitly specify `"telemetry_source": "OFFLINE_SIMULATION"` to avoid misleading claims of real live hardware data.
- **Offline / Mock Mode**: A mock backend is provided so the entire AI companion and tool dispatch pipeline can be tested without downloading multi-gigabyte neural network weights.

---

## 6. Recovery Procedures

If an experimental modification causes a kernel boot regression:
1. Verify the 20-quadword x86-64 `iretq` frame layout in `kernel/process/process.c`:
   - `frame[0..14] = GPRs`
   - `frame[15] = RIP`
   - `frame[16] = CS (0x08)`
   - `frame[17] = RFLAGS (0x202)`
   - `frame[18] = RSP (stack top)`
   - `frame[19] = SS (0x10)`
2. Ensure IDT vectors 0-31 are hooked to `generic_exception_stub` so unexpected CPU exceptions print diagnostics instead of triple-faulting.
3. Use the serial console or debug port `0xE9` (`make run-headless` or `make test-qemu`) to capture boot diagnostic messages.
4. Checkpoints and backups are maintained in `backups/pre-repair-20261015/`.
