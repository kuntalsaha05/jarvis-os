# JARVIS-OS — Phase 2 Hardware & Interrupt Verification Document

This document records the design, implementation, and verified execution evidence for **Phase 2 (Hardware, Interrupts, and Device Drivers)** of JARVIS-OS.

---

## 1. Phase 2 Completion Checklist (6/6 Completed)

| Requirement | Priority | Status | Implementation Details | Verified Evidence |
|---|---|:---:|---|---|
| **Keyboard IRQ1 handling** | Critical | ✅ Complete | `kernel/interrupts/interrupts.S` (`keyboard_stub`), `kernel/interrupts/idt.c` (gate 33), `kernel/interrupts/pic.c` (`pic_unmask_irq(1)`), `kernel/drivers/keyboard.c` (`kbd_queue`) | IRQ1 stub entered, scancode enqueued, EOI acknowledged |
| **Keyboard controller setup** | Critical | ✅ Complete | `kernel/drivers/keyboard.c` (`keyboard_init`): sets controller config byte bit 0 (IRQ1 enable) and bit 6 (translation) via command `0x60` | Controller enables IRQ1 generation upon keystroke |
| **Safe interrupt handling** | Critical | ✅ Complete | ISR `keyboard_handler()` runs in interrupt context: reads port `0x60`, validates device, pushes scancode to circular buffer, sends `pic_send_eoi(1)`. Shell reader `keyboard_read_char()` dequeues and parses ASCII outside ISR | Clean separation between hardware event capture and console processing |
| **Timer and PIC verification** | High | ✅ Complete | PIT programmed to 100 Hz in `kernel/interrupts/pit.c`. `scheduler_interrupt` tracks ticks and preemption. `kernel_main` preemption test verifies `p1_count > 0`, `p2_count > 0`, and `quantum_preemptions > 0` without yields | Confirmed involuntary preemption across timer quanta |
| **Repeatable QEMU tests** | High | ✅ Complete | `scripts/qemu_smoke_test.py` automated harness running QEMU headless over stdio | 40/40 checkpoints passed consistently |
| **Phase 2 evidence** | High | ✅ Complete | Exact commands, terminal outputs, and architectural boundaries recorded herein | Reproducible via `make test-all` |

---

## 2. Hardware & Interrupt Architecture

### 2.1 Programmable Interrupt Controller (8259 PIC)
- **Vector Remapping**:
  - Master PIC: remapped to vectors `32..39` (`0x20..0x27`).
  - Slave PIC: remapped to vectors `40..47` (`0x28..0x2F`).
- **Unmasking**:
  - In `pic_init()`, `master_mask &= ~(0x01 | 0x02);` unmasks both IRQ0 (PIT timer) and IRQ1 (PS/2 keyboard).
  - Helper functions `pic_unmask_irq(irq)` and `pic_mask_irq(irq)` provide dynamic IRQ line control for individual devices.
- **End-of-Interrupt (EOI)**:
  - `pic_send_eoi(irq)` issues `0x20` to `PIC1_COMMAND` (0x20), and if `irq >= 8`, cascades `0x20` to `PIC2_COMMAND` (0xA0).

### 2.2 Interrupt Descriptor Table (IDT) Gate Layout
| Vector | Description | Gate Type / DPL | Handler Stub | Handler Function |
|:---:|---|:---:|---|---|
| `0..31` | CPU Exceptions (#DE, #GP, #UD, etc.) | 0x8E (Ring 0) | `generic_exception_stub` | `generic_exception_handler` -> `kernel_panic` |
| `14` | Page Fault (#PF) | 0x8E (Ring 0) | `page_fault_stub` | `page_fault_handler` (CR2, error code) |
| `32` | Master PIC IRQ0 (PIT Timer, 100 Hz) | 0x8E (Ring 0) | `timer_stub` | `scheduler_interrupt(frame, 1)` |
| `33` | Master PIC IRQ1 (PS/2 Keyboard) | 0x8E (Ring 0) | `keyboard_stub` | `keyboard_handler()` |
| `0x80` | System Call Interface (`int $0x80`) | 0xEE (Ring 3/0) | `syscall_stub` | `syscall_dispatch(...)` |
| `0x81` | Cooperative Yield (`int $0x81`) | 0x8E (Ring 0) | `schedule_stub` | `scheduler_interrupt(frame, 0)` |

### 2.3 PS/2 Keyboard Controller & Event Buffer
1. **Controller Configuration**:
   - `keyboard_init()` disables ports (`0xAD`, `0xA7`), drains residual bytes from port `0x60`, reads controller configuration byte (`0x20`), modifies configuration:
     - Bit 0: First PS/2 port interrupt enabled (`1`).
     - Bit 4: First PS/2 port clock enabled (`0`).
     - Bit 6: First PS/2 port translation enabled (`1`).
   - Writes configuration via controller command `0x60`, then enables the first PS/2 port via command `0xAE`.
   - Calls `pic_unmask_irq(1)` to ensure the PIC accepts IRQ1 interrupts.
2. **Circular Event Buffer**:
   - Capacity: 128 scancodes (`KEYBOARD_BUFFER_SIZE`).
   - Thread-safe atomic counters (`head`, `tail`, `count`) using GCC built-ins `__sync_fetch_and_add` and `__sync_fetch_and_sub`.
3. **ISR (`keyboard_handler`)**:
   - Checks status port `0x64` bit 0 (output buffer full).
   - Verifies status bit 5 is 0 (data is from keyboard, not auxiliary PS/2 mouse).
   - Enqueues scancode into `kbd_queue` via `keyboard_enqueue(scancode)`.
   - Sends EOI to master PIC: `pic_send_eoi(1)`.
4. **Console Consumer (`keyboard_read_char`)**:
   - Dequeues scancodes in user/shell context.
   - Filters out key-release events (`scancode & 0x80`).
   - Translates make-codes to ASCII using `scancode_map`.
   - Simultaneously services COM1 serial port (`0x3F8`) for dual-console transparency.
   - Preserves fallback polling to ensure execution remains stable even if interrupts are temporarily disabled (`cli`).

---

## 3. Timer & PIC Preemption Verification

### 3.1 PIT Timer Programming
- Mode: Channel 0, Rate Generator (Mode 2), Square Wave / Periodic.
- Frequency: 100 Hz (10 ms tick period).
- `timer_ticks` incremented on every IRQ0 tick inside `scheduler_interrupt`.

### 3.2 Preemption Proof Without Cooperative Yields
The preemption validation in `kernel/kernel.c` tests genuine involuntary timer preemption:
- Spawns two CPU-bound tasks (`preempt_task_1`, `preempt_task_2`).
- Neither task calls `scheduler_yield()` or software interrupt `int $0x81`.
- Each task spins in a busy loop for 12 timer ticks (`timer_ticks < start + 12`), updating `p1_count` and `p2_count`.
- Preemption validation asserts:
  ```c
  if (p1_count > 0 && p2_count > 0 && stats_post.quantum_preemptions > stats_pre.quantum_preemptions) {
      terminal_write("PIT timer preemption test: PASS (involuntary timer preemption verified)\n");
  }
  ```
- **Result**: Both tasks execute and increment loop iterations, the scheduler forcibly preempts them upon quantum expiration (5 ticks), and returns cleanly to `kernel_main`.

---

## 4. Test Execution Evidence

### 4.1 Host Algorithm Test Suite (`make test`)
```text
$ make test
==================================================
   JARVIS-OS HOST ALGORITHM VERIFICATION SUITE    
==================================================
[TEST] CPU Scheduling Algorithms... PASS
[TEST] Page Replacement & Belady Anomaly... PASS
[TEST] Banker's Deadlock Avoidance... PASS
[TEST] Disk Head Scheduling (FCFS, SSTF, SCAN, C-SCAN)... PASS
[TEST] Memory Allocation Fit (First, Best, Worst)... PASS
[TEST] File Allocation Methods (Contiguous, Linked, Indexed)... PASS
[TEST] Deadlock Detection (WFG/Matrix) & Prevention... PASS
==================================================
   ALL HOST UNIT TESTS COMPLETED SUCCESSFULLY!    
==================================================
```

### 4.2 AI Companion Test Suite (`make test-ai`)
```text
$ make test-ai
python3 -m unittest discover -s tests -p "test_*.py"
..........
----------------------------------------------------------------------
Ran 10 tests in 0.001s

OK
```

### 4.3 Automated QEMU Smoke Test (`make test-qemu`)
```text
$ make test-qemu
==================================================
      JARVIS-OS AUTOMATED QEMU SMOKE TEST         
==================================================
Captured 12290 bytes of serial output in 10.01s.

  [PASS] Early Serial Debug Output
  [PASS] Multiboot 0.6.96 Hand-off
  [PASS] Physical Memory Discovery
  [PASS] BIOS/GRUB Memory Map
  [PASS] Page Allocator Test
  [PASS] RAM Disk Test
  [PASS] Kernel Online
  [PASS] Cooperative Switching
  [PASS] Cooperative Return
  [PASS] Timer Preemption Execution
  [PASS] Timer Preemption Quanta Switch
  [PASS] Timer Preemption Verified
  [PASS] Shell Ready
  [PASS] Interrupt Subsystem IRQ0/1
  [PASS] Timer IRQ0 Vector
  [PASS] Keyboard IRQ1 Vector
  [PASS] Process & Thread Metrics
  [PASS] Context Switch Counting
  [PASS] Quantum Preemption Counting
  [PASS] Directory Listing (ls)
  [PASS] File Readback (cat)
  [PASS] CPU Scheduling Simulation
  [PASS] Page Replacement Benchmark
  [PASS] Belady's Anomaly
  [PASS] Memory Allocation Fit Benchmark
  [PASS] Best Fit Allocs
  [PASS] File Allocation Methods Benchmark
  [PASS] File Allocation Lookup
  [PASS] Banker's Deadlock Avoidance
  [PASS] Banker Safe Sequence
  [PASS] Deadlock Detection & Prevention
  [PASS] Deadlock Detection Cycle Found
  [PASS] Deadlock Prevention Havender
  [PASS] Disk Scheduling Simulation
  [PASS] Synchronization Invariants
  [PASS] Producer-Consumer Invariant
  [PASS] Reader-Writer Invariant
  [PASS] JARVIS-FS Directory
  [PASS] Software Interrupt Syscall
  [PASS] Syscall int $0x80 Execution

==================================================
   ALL 40 VERIFICATION CHECKPOINTS PASSED!       
==================================================
```

---

## 5. Architectural Boundaries & Limitations

1. **Privilege Boundary**:
   - Tasks execute in CPU Ring 0 (Supervisor mode) sharing the kernel address space.
   - User-level isolation (Ring 3) requires TSS `RSP0` configuration, user page table permission bits, and an ELF loader, which is documented in `docs/LIMITATIONS.md`.
2. **Interrupt Controller Model**:
   - The kernel relies on standard legacy Intel 8259 dual PIC chips rather than APIC / IOAPIC.
   - Uniprocessor execution only; SMP multi-core dispatching is not supported.
3. **Dual Input Handling**:
   - Both PS/2 keyboard IRQ1 and COM1 serial line 0x3F8 are polled transparently in `keyboard_read_char()`, enabling seamless operation in graphical QEMU windows and headless automated test harnesses alike.
