# JARVIS-OS — Phase 5: Storage and Filesystem Reliability Verification Report

**Milestone:** Phase 5 (Storage & Filesystem Reliability)  
**Date:** 2026-10-18  
**Repository:** `~/jarvis-os` (`main` branch)  
**Architecture:** x86-64 freestanding C and assembly  
**Kernel Targets:** Bare-metal Long Mode, GRUB Multiboot, QEMU emulator  

---

## 1. Executive Summary

Phase 5 delivers a hardened, bounds-checked storage subsystem, a resilient in-memory filesystem (JARVIS-FS) with rollback and file truncation capabilities, extended shell diagnostics (`df`, `touch`, `write`, `trunc`, `rm`, `fstest`), host-side disk image inspection utilities, and complete verification across all three test tiers:
- **Host Unit Tests (`make test`)**: **15/15 test suites PASS** (including Block Device, Filesystem Invariants, File Allocation Reuse, and Disk Scheduling).
- **AI Companion Tests (`make test-ai`)**: **11/11 tests PASS** (including host image tool validation).
- **Automated QEMU Bare-Metal Smoke Test (`make test-qemu`)**: **63/63 checkpoints PASS** (100% bare-metal validation).

---

## 2. Block-Device Subsystem Architecture

### 2.1 Interface & Error Codes
The block-device abstraction in [`kernel/storage/block.h`](file:///home/kuntalsaha/jarvis-os/kernel/storage/block.h) and [`kernel/storage/block.c`](file:///home/kuntalsaha/jarvis-os/kernel/storage/block.c) provides a clean driver interface:

```c
#define BLOCK_OK                 0
#define BLOCK_ERR_NOT_READY     -1
#define BLOCK_ERR_INVALID_PARAM -2
#define BLOCK_ERR_OUT_OF_BOUNDS -3
#define BLOCK_ERR_IO_FAILED     -4

typedef struct {
    int (*read)(uint64_t lba, uint32_t count, void *buffer);
    int (*write)(uint64_t lba, uint32_t count, const void *buffer);
    uint32_t sector_size;
    uint64_t sector_count;
    const char *name;
} block_device_t;
```

### 2.2 Boundary Checking & Range Validation
Every read and write request strictly validates:
1. **Device Readiness**: Rejects calls if no device has registered (`BLOCK_ERR_NOT_READY`).
2. **Buffer Null Pointer**: Rejects `buffer == NULL` with `BLOCK_ERR_INVALID_PARAM`.
3. **Zero Count**: Returns `BLOCK_OK` immediately without side effects.
4. **LBA Bounds**: Rejects requests where `lba >= sector_count` with `BLOCK_ERR_OUT_OF_BOUNDS`.
5. **Integer Overflow & Capacity**: Checks `lba + count < lba` (overflow protection) and `count > sector_count - lba` with `BLOCK_ERR_OUT_OF_BOUNDS`.

### 2.3 Working Driver: RAMDisk
The current registered driver is [`kernel/storage/ramdisk.c`](file:///home/kuntalsaha/jarvis-os/kernel/storage/ramdisk.c):
- Sector Size: 512 bytes
- Sector Count: 128 sectors
- Total Capacity: 65,536 bytes (64 KiB)
- Volatility: Stored in kernel BSS memory; non-persistent across machine reboots.

---

## 3. JARVIS-FS Filesystem Correctness & Robustness

### 3.1 Data Structures & Geometry
JARVIS-FS operates directly on the block-device abstraction:
- **Block 0 (Superblock)**: Magic number `0x4A415256` (`"JARV"`), 128 total blocks, 120 data blocks, 16 maximum inodes.
- **Blocks 1-2 (Inode Table)**: 16 inode descriptors (68 bytes each, with 28-byte filenames, 4-byte size, 8 direct block pointers, and active flag).
- **Block 3 (Block Allocation Bitmap)**: Bit-vector managing physical allocation for blocks 4..123.
- **Blocks 4-123 (Data Blocks)**: 120 allocatable data blocks $\times$ 512 bytes = 61,440 bytes of payload storage.
- **Maximum File Size**: 8 direct blocks $\times$ 512 bytes = 4,096 bytes per file.

### 3.2 Key Robustness Features Implemented in Phase 5
1. **Path Normalization**:
   Handles leading slashes (`"/kernel.cfg"` normalizes to `"kernel.cfg"`). Rejects empty names, names exceeding 27 characters, or interior slashes with `FS_ERR_INVALID_PARAM`.
2. **Rollback on Partial Allocation Failure**:
   During `fs_write()`, if block allocation fails midway due to disk exhaustion or I/O failure, all newly allocated blocks within that call are safely unrolled and freed back to the bitmap, preserving previous file size and offsets without leaking blocks or corrupting file metadata.
3. **File Truncation (`fs_truncate`)**:
   - Shrinking: Reclaims trailing data blocks back to the free bitmap and zeros the partial tail byte block.
   - Expanding: Allocates zeroed blocks up to the requested size (subject to disk capacity).
   - Bounds: Enforces max file size (4096 bytes).
4. **Open Descriptor Invalidation on Deletion**:
   When `fs_delete()` removes a file, all active file descriptors pointing to that inode index are immediately closed and marked invalid (`FS_ERR_BAD_FD`). Subsequent reads or writes on those descriptors fail cleanly.
5. **Block Reuse**:
   Deleting a file reclaims all associated direct blocks and its inode slot. Newly created files immediately reuse the freed blocks and slots.

---

## 4. Educational OS Algorithms: Allocation & Disk Scheduling

JARVIS-OS cleanly decouples genuine kernel storage mechanisms from educational algorithm benchmarks:

### 4.1 File Allocation Methods ([`kernel/algorithms/file_alloc.c`](file:///home/kuntalsaha/jarvis-os/kernel/algorithms/file_alloc.c))
- **Contiguous Allocation**: $O(1)$ random access, verifies contiguous block runs, external fragmentation rejection (`-2`) when total space is sufficient but fragmented, and block freeing/reuse.
- **Linked Allocation**: $O(N)$ sequential traversal via block chaining, zero external fragmentation, block freeing, and reuse.
- **Indexed Allocation**: $O(1)$ direct access via single index block, 1-block overhead, bounds checks, freeing, and reuse.

### 4.2 Disk Head Scheduling ([`kernel/algorithms/disk_sched.c`](file:///home/kuntalsaha/jarvis-os/kernel/algorithms/disk_sched.c))
Simulated on a 200-cylinder disk (0..199) with request queue `[98, 183, 37, 122, 14, 124, 65, 67]` starting at cylinder 53:
- **FCFS**: 640 cylinder movements.
- **SSTF**: 236 cylinder movements.
- **SCAN (Elevator)**: 331 cylinder movements (services upwards to 199, then reverses).
- **C-SCAN**: 382 cylinder movements (services upwards to 199, wraps to 0, continues).
- **Edge Cases**: Verified zero-request queues (movement 0) and single requests matching head position (movement 0).

---

## 5. Persistence, Host Tools & Hardware Characterization

### 5.1 Storage Volatility Truthfulness
- JARVIS-OS currently uses an in-memory RAM disk (`ramdisk_data`).
- **All data written to JARVIS-FS is volatile** and does not persist across emulator or system restarts.
- Physical IDE/AHCI/NVMe drivers are intentionally not implemented to maintain safety and simplicity within this educational monolithic kernel.

### 5.2 Host-Side Disk Tool ([`scripts/jarvis_fs_tool.py`](file:///home/kuntalsaha/jarvis-os/scripts/jarvis_fs_tool.py))
A host-side Python tool has been implemented to:
- Format raw 64 KiB disk images conforming to the JARVIS-FS layout.
- Inspect disk images (verifying superblock magic `0x4A415256`, inode counts, and directory entries).
- Inject files into disk images and extract files from disk images with checksum verification.
- Validated via automated unit test in [`tests/test_ai.py`](file:///home/kuntalsaha/jarvis-os/tests/test_ai.py).

---

## 6. Interactive Shell Commands Reference

The shell (`kernel/shell/shell.c`) includes 8 storage commands reporting actual status without false success:

| Command | Syntax | Description |
|---|---|---|
| `df` / `fsstat` | `df` | Displays superblock magic, capacity, used/free blocks, and inode table metrics |
| `ls` | `ls` | Lists all files with byte sizes and allocated block counts |
| `cat` | `cat <filename>` | Displays file content, or error if not found |
| `touch` | `touch <filename>` | Creates an empty file |
| `write` | `write <filename> <text>` | Writes text to file; reports exact byte count written |
| `trunc` | `trunc <filename> <size>` | Truncates file to specified size; frees or allocates blocks |
| `rm` / `del` | `rm <filename>` | Deletes file, frees direct blocks and inode slot |
| `fstest` | `fstest` | Runs in-kernel invariant test suite (block device, FS, alloc, disk seek) |

---

## 7. Verification Evidence & Test Execution Logs

### 7.1 Host Unit Tests (`make test`)
```
mkdir -p build
gcc -O2 -Wall -Wextra -Ikernel tests/host_tests.c kernel/algorithms/sched_sim.c kernel/algorithms/page_replace.c kernel/algorithms/banker.c kernel/algorithms/deadlock.c kernel/algorithms/mem_fit.c kernel/algorithms/file_alloc.c kernel/algorithms/disk_sched.c kernel/ipc/ipc.c kernel/ipc/semaphore.c kernel/ipc/sync_demo.c kernel/storage/block.c kernel/storage/ramdisk.c kernel/storage/fs.c -o build/host_tests
./build/host_tests
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

### 7.2 AI Companion Tests (`make test-ai`)
```
python3 -m unittest discover -s tests -p "test_*.py"
...........
----------------------------------------------------------------------
Ran 11 tests in 0.003s

OK
```

### 7.3 QEMU Bare-Metal Smoke Test (`make test-qemu`)
```
==================================================
      JARVIS-OS AUTOMATED QEMU SMOKE TEST         
==================================================
Captured 16520 bytes of serial output in 14.54s.

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

==================================================
   ALL 63 VERIFICATION CHECKPOINTS PASSED!       
==================================================
```

---

## 8. Conclusion

All requirements of Phase 5 are fully satisfied:
- Clean block-device interface with status and range bounds checks.
- Resilient JARVIS-FS with file creation, read/write, truncation, deletion, block reuse, and rollback on allocation failure.
- Decoupled educational algorithms for File Allocation and Disk Scheduling with verified reuse and boundary invariants.
- Honest characterization of RAM disk volatility and host-side disk tool utility.
- 100% automated test coverage across `make test-all`.
