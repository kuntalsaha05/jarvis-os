# JARVIS-OS — Testing & Verification Guide

This document specifies the verification methodology, test harness structure, and validation evidence for JARVIS-OS.

---

## 1. Testing Philosophy

In accordance with the project rules:
- **No fake passes**: A test is only marked complete if actual source code executes and produces correct output.
- **Decoupled execution**: Hardware-independent algorithmic logic (scheduling metrics, page replacement, memory fit, file allocation, deadlock detection/prevention, disk seek calculations) is testable on the host without booting QEMU.
- **End-to-End integration**: Bare-metal kernel boot, involuntary timer preemption without voluntary yields, RAM disk I/O, software interrupt system calls (`int $0x80`), and shell interactivity are tested automatically via an automated QEMU smoke runner.

---

## 2. Test Suites Summary

| Test Suite | Implementation File | Runner Target | Scope |
|---|---|---|---|
| **Host Algorithm Tests** | `tests/host_tests.c` | `make test` | Mathematical verification of CPU scheduling (FCFS, SJF, RR), Page Replacement (FIFO, LRU, Optimal, Belady Anomaly), Banker's algorithm, Disk head scheduling, Memory Allocation Fit (First/Best/Worst Fit), File Allocation (Contiguous, Linked, Indexed), Deadlock Detection (WFG/Matrix) & Prevention, Process lifecycle state transitions, Memory bounds, IPC subsystem/bounded queues, Mutex ownership & Semaphore invariants, Deadlock scenarios (exceed max, insufficient, unsafe rejection), Block device interface & bounds checks, JARVIS-FS filesystem correctness & rollback, and File allocation reuse & disk seek invariants. |
| **AI Companion Tests** | `tests/test_ai.py` | `make test-ai` | Python unit tests for `MockLLMBackend`, `LlamaCppBackend` fallback, `AirLLMBackend` loading check, `ToolRegistry` allowlisting, input validation, audit logs, JSON protocol serialization, explicit `OFFLINE_SIMULATION` telemetry labeling, host `JarvisFSImage` creation/inspection, 8-step agent cycles (`JarvisAgent`), privileged approval callbacks, destructive actions, backend health checks, streaming generation, and line-delimited serial bridge transport framing. |
| **QEMU Smoke Test** | `scripts/qemu_smoke_test.py` | `make test-qemu` | Boots `build/jarvis.iso` headlessly, verifies 65 checkpoints covering Multiboot hand-off, memory mapping, early serial debug, preemption without yield, keyboard IRQ1 / timer IRQ0 interrupt handling, thread metrics, IPC message passing/queues, mutex ownership, counting semaphores, block device bounds, JARVIS-FS rollback/truncation/stat/df/touch/write/rm, in-kernel benchmarks, and bare-metal `ai` status command. |
| **All Test Suites** | All of the above | `make test-all` | Runs all three test suites in sequence and reports aggregate status. |

---

## 3. Running the Tests

### 3.1 Host Algorithm Tests (`make test`)
Compiles the pure C algorithm modules (`sched_sim.c`, `page_replace.c`, `banker.c`, `disk_sched.c`, `mem_fit.c`, `file_alloc.c`, `deadlock.c`, `ipc.c`, `semaphore.c`, `sync_demo.c`, `block.c`, `ramdisk.c`, `fs.c`) with host `gcc`:
```bash
make test
```
**Verified Output**:
```
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
[TEST] Process Lifecycle State Transitions... PASS
[TEST] Memory Boundary & Frame Invariants... PASS
[TEST] IPC Subsystem & Bounded Queues... PASS
[TEST] Mutex Ownership & Semaphore Invariants... PASS
[TEST] Deadlock Scenarios & Havender Prevention... PASS
[TEST] Block Device Interface & Bounds Checks... PASS
[TEST] JARVIS-FS Filesystem Correctness & Rollback... PASS
[TEST] File Allocation (Reuse) & Disk Scheduling Invariants... PASS
==================================================
   ALL HOST UNIT TESTS COMPLETED SUCCESSFULLY!    
==================================================
```

### 3.2 AI Companion Tests (`make test-ai`)
Executes the Python test suite:
```bash
make test-ai
```
**Verified Output**:
```
python3 -m unittest discover -s tests -p "test_*.py"
..................
----------------------------------------------------------------------
Ran 18 tests in 0.022s

OK
```

### 3.3 Automated QEMU Smoke Test (`make test-qemu`)
Spawns QEMU in headless mode, connects to standard I/O, feeds shell commands, and verifies output against 65 required checkpoints:
```bash
make test-qemu
```
**Verified Output**:
```
==================================================
      JARVIS-OS AUTOMATED QEMU SMOKE TEST         
==================================================
Captured 17614 bytes of serial output in 12.01s.

  [PASS] Early Serial Debug Output
  [PASS] Multiboot 0.6.96 Hand-off
  [PASS] Physical Memory Discovery
  [PASS] BIOS/GRUB Memory Map
  [PASS] Page Allocator Test
  [PASS] Page Allocator Invariants
  [PASS] Page Fault Diagnostics Boot
  [PASS] RAM Disk Test
  [PASS] Block Device Invariants Boot
  [PASS] Filesystem Invariants Boot
  [PASS] Cooperative Switching
  [PASS] Cooperative Return
  [PASS] Register/Stack Integrity Test
  [PASS] Timer Preemption Execution
  [PASS] Timer Preemption Quanta Switch
  [PASS] Timer Preemption Verified
  [PASS] Kernel Online
  [PASS] Shell Ready
  [PASS] Interrupt Subsystem IRQ0/1
  [PASS] Timer IRQ0 Vector
  [PASS] Keyboard IRQ1 Vector
  [PASS] Process & Thread Metrics
  [PASS] Memory Subsystem Boundary
  [PASS] Page Fault Decoder Self-Test
  [PASS] Page Fault Diagnosis Output
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
  [PASS] Mutex Ownership Protection
  [PASS] Counting Semaphore Invariants
  [PASS] IPC Queue Invariants Boot
  [PASS] Synchronization Invariants Boot
  [PASS] IPC Message Passing Benchmark
  [PASS] IPC Queue Underflow
  [PASS] IPC Queue Telemetry
  [PASS] IPC Subsystem Invariants Shell
  [PASS] JARVIS-FS Directory
  [PASS] Software Interrupt Syscall
  [PASS] Syscall int $0x80 Execution
  [PASS] Storage Volume Metrics (df)
  [PASS] Touch File Creation
  [PASS] Write File Content
  [PASS] Truncate File Size
  [PASS] Remove File
  [PASS] Filesystem Invariants Shell
  [PASS] Block Device Invariants Shell
  [PASS] AI Status Shell Command
  [PASS] AI Security 4-Tier Model

==================================================
   ALL 65 VERIFICATION CHECKPOINTS PASSED!       
==================================================
```


### 3.4 Full Verification Suite (`make test-all`)
Executes all test suites in sequence:
```bash
make test-all
```
