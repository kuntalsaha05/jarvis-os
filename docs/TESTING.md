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
| **Host Algorithm Tests** | `tests/host_tests.c` | `make test` | Mathematical verification of CPU scheduling (FCFS, SJF, RR), Page Replacement (FIFO, LRU, Optimal, Belady Anomaly), Banker's algorithm, Disk head scheduling, Memory Allocation Fit (First/Best/Worst Fit), File Allocation (Contiguous, Linked, Indexed), and Deadlock Detection (WFG/Matrix) & Prevention. |
| **AI Companion Tests** | `tests/test_ai.py` | `make test-ai` | Python unit tests for `MockLLMBackend`, `LlamaCppBackend` fallback, `AirLLMBackend` loading check, `ToolRegistry` allowlisting, input validation, audit logs, JSON protocol serialization, and explicit `OFFLINE_SIMULATION` telemetry labeling. |
| **QEMU Smoke Test** | `scripts/qemu_smoke_test.py` | `make test-qemu` | Boots `build/jarvis.iso` headlessly, verifies 28 checkpoints covering boot, memory, preemption without yield, shell commands, and in-kernel benchmarks. |
| **All Test Suites** | All of the above | `make test-all` | Runs all three test suites in sequence and reports aggregate status. |

---

## 3. Running the Tests

### 3.1 Host Algorithm Tests (`make test`)
Compiles the pure C algorithm modules (`sched_sim.c`, `page_replace.c`, `banker.c`, `disk_sched.c`, `mem_fit.c`, `file_alloc.c`, `deadlock.c`) with host `gcc`:
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
..........
----------------------------------------------------------------------
Ran 10 tests in 0.001s

OK
```

### 3.3 Automated QEMU Smoke Test (`make test-qemu`)
Spawns QEMU in headless mode, connects to standard I/O, feeds shell commands, and verifies output against 28 required checkpoints:
```bash
make test-qemu
```
**Verified Output**:
```
==================================================
      JARVIS-OS AUTOMATED QEMU SMOKE TEST         
==================================================
Captured 10532 bytes of serial output in 9.01s.

  [PASS] Page Allocator Test
  [PASS] RAM Disk Test
  [PASS] Kernel Online
  [PASS] Cooperative Switching
  [PASS] Cooperative Return
  [PASS] Timer Preemption Execution
  [PASS] Timer Preemption Quanta Switch
  [PASS] Timer Preemption Verified
  [PASS] Shell Ready
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
   ALL 28 VERIFICATION CHECKPOINTS PASSED!       
==================================================
```

### 3.4 Full Verification Suite (`make test-all`)
Executes all test suites in sequence:
```bash
make test-all
```
