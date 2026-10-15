# JARVIS-OS — Known Limitations & Architectural Boundaries

In accordance with the project's educational mission, this document explicitly records the design boundaries, simulated vs native implementations, and current hardware limitations of JARVIS-OS.

---

## 1. Uniprocessor Hardware Assumptions

- **Interrupt Controller**: JARVIS-OS uses the legacy Intel 8259 PIC (Programmable Interrupt Controller) rather than the modern APIC/x2APIC.
- **Multiprocessing**: The kernel operates on a single CPU core. Concurrency primitives (mutexes, semaphores) use atomic test-and-set instructions and spin-pauses suitable for uniprocessor preemption. SMP (Symmetric Multiprocessing) is not implemented.

---

## 2. Privilege Levels & Execution Rings

- **Kernel Mode Execution**: All created kernel processes (`task_a`, `task_b`, shell) currently execute in CPU Ring 0 (Supervisor mode) sharing the kernel address space.
- **Ring 3 User Mode**: While the system call dispatcher table exists (`kernel/syscall/syscall.c`), a true hardware user/kernel privilege boundary with separate user stacks, `TSS`, and `sysret`/`iretq` ring 3 transitions is not yet implemented.

---

## 3. Paging vs Page Replacement Simulation

- **Paging**: The kernel configures 4-level paging (PML4, PDPT, PD) in `boot.S` using 2 MiB identity-mapped pages to cover physical memory. The page allocator manages a bitmap pool of 4 KiB frames.
- **Page Replacement**: The FIFO, LRU, and Optimal page replacement demonstrations (`kernel/algorithms/page_replace.c`) are educational algorithm simulations operating on deterministic reference strings. Hardware demand paging with disk-swapping is not implemented on the bare-metal MMU.

---

## 4. Storage & Persistence

- **RAM Disk**: The block device (`kernel/storage/ramdisk.c`) is allocated in RAM (128 sectors of 512 bytes = 64 KiB).
- **Volatile Storage**: Data written to JARVIS-FS files is retained in memory for the duration of the QEMU session, but is not persisted to physical host storage after the emulator shuts down. Real physical disk drivers (IDE, AHCI, NVMe) are not implemented.

---

## 5. Host-Side AI Inference Boundary

- **Host Service**: As designed, deep-learning models (AirLLM, llama.cpp) run as companion processes on the host workstation, not inside the bare-metal kernel.
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
4. Git recovery branch `checkpoint-preemption-dev` and directory `backups/pre-repair-20261015/` contain baseline recovery copies.

