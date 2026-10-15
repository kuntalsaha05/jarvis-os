# JARVIS-OS — Testing & Verification Guide

This document specifies the verification methodology, test harness structure, and validation evidence for JARVIS-OS.

---

## 1. Testing Philosophy

In accordance with the project rules:
- **No fake passes**: A test is only marked complete if actual source code executes and produces correct output.
- **Decoupled execution**: Hardware-independent algorithmic logic (scheduling metrics, page replacement, deadlock avoidance, disk seek calculations) is testable on the host without booting QEMU.
- **End-to-End integration**: Bare-metal kernel boot, preemption context switching, RAM disk I/O, and shell interactivity are tested automatically via an automated QEMU smoke runner.

---

## 2. Test Suites Summary

| Test Suite | Implementation File | Runner Target | Scope |
|------------|---------------------|---------------|-------|
| **Host Algorithm Tests** | `tests/host_tests.c` | `make test` | Mathematical verification of FCFS/SJF/RR metrics, FIFO/LRU/Optimal page faults, Belady's Anomaly, Banker's safety/request matrices, and disk seek distance. |
| **AI Companion Tests** | `tests/test_ai.py` | `make test-ai` | Python unit tests for `MockLLMBackend`, `LlamaCppBackend` fallback, `ToolRegistry` allowlisting, input validation, audit logs, and JSON serialization. |
| **QEMU Smoke Test** | `scripts/qemu_smoke_test.py` | `make test-qemu` | Boots `build/jarvis.iso` headlessly, verifies 16 checkpoints (allocator, RAM disk, ONLINE state, task A/B preemption, shell prompt, and in-kernel command execution). |
| **All Test Suites** | All of the above | `make test-all` | Runs all three test suites in sequence and reports aggregate status. |

---

## 3. Running the Tests

### 3.1 Host Algorithm Tests (`make test`)
Compiles the pure C algorithm modules (`sched_sim.c`, `page_replace.c`, `banker.c`, `disk_sched.c`) with host `gcc`:
```bash
make test
```
**Expected Output**:
```
==================================================
   JARVIS-OS HOST ALGORITHM VERIFICATION SUITE    
==================================================
[TEST] CPU Scheduling Algorithms... PASS
[TEST] Page Replacement & Belady Anomaly... PASS
[TEST] Banker's Deadlock Avoidance... PASS
[TEST] Disk Head Scheduling (FCFS, SSTF, SCAN, C-SCAN)... PASS
==================================================
   ALL HOST UNIT TESTS COMPLETED SUCCESSFULLY!    
==================================================
```

### 3.2 AI Companion Tests (`make test-ai`)
Executes the Python test suite:
```bash
make test-ai
```
**Expected Output**:
```
python3 -m unittest discover -s tests -p "test_*.py"
........
----------------------------------------------------------------------
Ran 8 tests in 0.001s

OK
```

### 3.3 Automated QEMU Smoke Test (`make test-qemu`)
Spawns QEMU in headless mode, connects to standard I/O, feeds shell commands, and verifies output against 16 required checkpoints:
```bash
make test-qemu
```
**Expected Output**:
```
==================================================
      JARVIS-OS AUTOMATED QEMU SMOKE TEST         
==================================================
Captured 5421 bytes of serial output in 7.00s.

  [PASS] Page Allocator Test
  [PASS] RAM Disk Test
  [PASS] Kernel Online
  [PASS] Cooperative/Preemption Switching
  [PASS] Task A/B Return
  [PASS] Shell Ready
  [PASS] CPU Scheduling Simulation
  [PASS] Page Replacement Benchmark
  [PASS] Belady's Anomaly
  [PASS] Banker's Deadlock Avoidance
  [PASS] Banker Safe Sequence
  [PASS] Disk Scheduling Simulation
  [PASS] Synchronization Invariants
  [PASS] Producer-Consumer Invariant
  [PASS] Reader-Writer Invariant
  [PASS] JARVIS-FS Directory

==================================================
   ALL 16 VERIFICATION CHECKPOINTS PASSED!       
==================================================
```

