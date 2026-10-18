# JARVIS-OS — Phase 4 IPC, Synchronization & Deadlocks Verification Document

This document records the architectural design, implementation details, invariant checks, and verified execution evidence for **Phase 4 (Inter-Process Communication, Synchronization Primitives, and Deadlock Handling)** of JARVIS-OS.

---

## 1. Phase 4 Completion Checklist (6/6 Completed)

| Requirement | Priority | Status | Implementation Details | Verified Evidence |
|---|---|:---:|---|---|
| **IPC Message Structure & Bounded Queues** | Critical | ✅ Complete | In `kernel/ipc/ipc.h` & `kernel/ipc/ipc.c`, enhanced `ipc_message_t` with `sender_pid`, `receiver_pid`, `type`, `length`, and `data[64]`. Implemented multi-queue support (`IPC_MAX_QUEUES = 4`) with queue capacity (16 messages), explicit bounds enforcement, and queue ownership rules. | Boot output: `IPC queue invariants: PASS`. Shell `ipc` benchmark: message exchange, underflow check, and telemetry display. |
| **Scheduler-Integrated Blocking IPC** | Critical | ✅ Complete | In `kernel/ipc/ipc.c`, implemented `ipc_receive_blocking_from_queue()` which marks the task as waiting on the queue, sets `wake_tick` timeout, transitions the task to `PROCESS_BLOCKED` via `process_block()`, and yields to the scheduler. `ipc_send_to_queue()` unblocks waiting tasks via `process_unblock()`. | Task blocking and wake-up verified without busy-waiting. Nonblocking operations return `IPC_ERR_EMPTY`. |
| **Counting Semaphores & Wait Queue** | Critical | ✅ Complete | In `kernel/ipc/semaphore.h` & `kernel/ipc/semaphore.c`, implemented counting semaphores with `value`, `max_value`, and wait queue (`wait_queue[8]`). Tasks block on empty resource counts via `process_block()`; `semaphore_post()` atomically unblocks waiting tasks. | Boot output: `Synchronization invariants: PASS`. Shell `sync` benchmark reports atomic counting and bounded limits. |
| **Mutexes with Strict Ownership Checks** | Critical | ✅ Complete | In `kernel/ipc/sync_demo.h` & `kernel/ipc/sync_demo.c`, implemented `mutex_t` with `owner_pid` and `lock_count`. `mutex_lock()` records the current task PID; recursive locking attempts return `MUTEX_ERR_DEADLOCK`. `mutex_unlock()` strictly verifies that only the owner PID can unlock, returning `MUTEX_ERR_NOT_OWNER` otherwise. | Host test and boot test verify unauthorized unlock attempts are rejected while keeping the mutex locked. |
| **Concurrency Demonstrations (PC & RW)** | High | ✅ Complete | Implemented Producer-Consumer bounded buffer (capacity 4, `sem_empty`, `sem_full`, `buffer_mutex`) and Reader-Writer synchronization (`rw_mutex`, `count_mutex`, concurrent readers, exclusive writer). | Invariants verified in `test_sync_invariants()`: FIFO order, overflow/underflow rejection, reader concurrency, exclusive writer locking. |
| **Deadlock Handling & Scenarios** | High | ✅ Complete | Preserved educational implementations: Banker's safety algorithm (`kernel/algorithms/banker.c`) and Wait-For Graph (WFG) / Matrix deadlock detection & Havender linear ordering prevention (`kernel/algorithms/deadlock.c`). Added tests for safe states, unsafe request rejection, detected cycles, and prevention. | Verified in host tests and shell commands `deadlock` and `detect`. |

---

## 2. IPC Subsystem Architecture

### 2.1 Message Layout and Protocol
Each message transmitted through the IPC subsystem conforms to `ipc_message_t`:

```
+-------------------------------------------------------------+
| sender_pid   (uint32_t, 4 bytes)                            |
| receiver_pid (uint32_t, 4 bytes, 0 = broadcast)             |
| type         (uint32_t, 4 bytes, application / message tag) |
| length       (uint32_t, 4 bytes, 0 <= length <= 64)         |
| data         (char[64], 64 bytes payload)                   |
+-------------------------------------------------------------+
Total Message Size: 80 bytes
```

### 2.2 Multi-Queue Management and Ownership Rules
1. **Queue Isolation**: The kernel supports up to 4 isolated queues (`IPC_MAX_QUEUES = 4`). Queue 0 (`IPC_DEFAULT_QUEUE`) is the permanent system mailbox owned by the kernel (PID 0).
2. **Dynamic Queues**: Dynamic queues (`1 .. IPC_MAX_QUEUES - 1`) are allocated via `ipc_queue_create(owner_pid)`.
3. **Queue Destruction Permissions**:
   - `ipc_queue_destroy(queue_id, requester_pid)` enforces that only the assigned `owner_pid` or the kernel (PID 0) can destroy a queue. Attempts by unauthorized PIDs return `IPC_ERR_NOT_OWNER` (-4).
   - Queue 0 is permanent and cannot be destroyed.
   - Any task waiting on a queue prior to destruction is awakened cleanly.
4. **Queue Capacity and Dropped Message Accounting**:
   - Each queue holds up to 16 messages (`IPC_QUEUE_CAPACITY`).
   - If a queue is full, `ipc_send()` rejects the message with `IPC_ERR_FULL` (-2) and increments `total_dropped_full`.
   - Nonblocking `ipc_receive()` on an empty queue immediately returns `IPC_ERR_EMPTY` (-2).

### 2.3 Scheduler-Integrated Blocking Receive
When a task calls `ipc_receive_blocking_from_queue()`:
1. If a matching message is already in the queue, it is dequeued immediately with `IPC_SUCCESS` (0).
2. If the queue is empty, the task registers its PID in `queue.waiting_pid`.
3. If `timeout_ticks > 0`, it sets `current->wake_tick = timer_ticks + timeout_ticks`.
4. It calls `process_block(current->pid)` to enter `PROCESS_BLOCKED` and yields via `scheduler_yield()`.
5. When another task calls `ipc_send_to_queue()`, the sender detects `waiting_pid > 0` and calls `process_unblock(waiting_pid)`.
6. Alternatively, if the timeout expires before a message arrives, the PIT timer ISR in `scheduler_interrupt()` awakens the task upon `timer_ticks >= wake_tick`. The task wakes up, discovers the queue is empty, and returns `IPC_ERR_TIMEOUT` (-3).

---

## 3. Synchronization Primitives

### 3.1 Mutex with Strict Ownership Verification
The kernel mutex (`mutex_t`) guarantees mutual exclusion with caller identity validation:
- **Lock Acquisition (`mutex_lock` / `mutex_try_lock`)**:
  - Obtains the caller's PID via `scheduler_current_process()->pid`.
  - Checks if the mutex is already locked by the calling PID; if so, returns `MUTEX_ERR_DEADLOCK` (-2) to prevent recursive self-deadlocks.
  - Atomically acquires the lock via GCC `__sync_lock_test_and_set(&mutex->locked, 1)`.
  - Records `mutex->owner_pid = caller_pid` and increments `mutex->lock_count`.
- **Lock Release (`mutex_unlock`)**:
  - If the mutex is not locked, returns `MUTEX_ERR_NOT_LOCKED` (-1).
  - Checks `if (mutex->owner_pid != caller_pid)`: if another task attempts to release the lock, the operation is **strictly rejected** with `MUTEX_ERR_NOT_OWNER` (-2), leaving the lock held.
  - If the caller is the legitimate owner, clears `owner_pid = 0` and releases the lock via `__sync_lock_release(&mutex->locked)`.

### 3.2 Counting Semaphores with Task Wait Queues
The counting semaphore (`semaphore_t`) supports bounded capacity and unblocking:
- `semaphore_init_bounded(sem, initial_value, max_value)` configures the resource counter and upper clamp limit.
- `semaphore_wait(sem)`:
  - If `value > 0`, atomically decrements `value` via `__sync_sub_and_fetch`.
  - If `value <= 0`, records the calling PID in `wait_queue[8]`, transitions the task to `PROCESS_BLOCKED`, and yields.
- `semaphore_post(sem)`:
  - Checks that `value < max_value`, returning -1 on overflow attempt.
  - Atomically increments `value`.
  - If tasks are waiting in `wait_queue`, dequeues the first waiting PID and invokes `process_unblock(pid)`.

### 3.3 Producer-Consumer and Reader-Writer Invariants
- **Producer-Consumer**:
  - Bounded buffer of capacity 4 guarded by `sem_empty` (initially 4), `sem_full` (initially 0), and `buffer_mutex`.
  - Produces 4 items; 5th produce attempt is rejected as buffer is full.
  - Consumes 4 items in strict FIFO order (`101, 102, 103, 104`).
  - 5th consume attempt is rejected as buffer is empty.
- **Reader-Writer**:
  - Guarded by `rw_mutex` and `count_mutex`.
  - Up to 3 concurrent readers hold shared access simultaneously.
  - Writer is strictly locked out while readers are active (`active_readers > 0`).
  - Upon reader departure, writer enters exclusively; readers are locked out during active write.

---

## 4. Deadlock Algorithms & Scenarios

JARVIS-OS distinguishes between runtime kernel synchronization (where mutexes reject non-owner unlock and self-deadlock) and educational algorithm simulators:

1. **Banker's Algorithm (`kernel/algorithms/banker.c`)**:
   - Safe State: For a 5-process, 3-resource system, calculates safe sequence `<P1, P3, P4, P0, P2>` using matrix relation $Need = Max - Allocation$ and work vector updates.
   - Unsafe Request Rejection:
     - Case A (Request > Need): returns `-1` (exceeds maximum claim).
     - Case B (Request > Available): returns `-2` (insufficient resources, must wait).
     - Case C (Request <= Available, but results in unsafe state): returns `-3` (denied: grant would lead to unsafe state).
2. **Deadlock Detection (`kernel/algorithms/deadlock.c`)**:
   - Constructs resource allocation and request matrices.
   - Cycle Detection: Detects circular wait cycles in Wait-For Graph (WFG) (e.g., P0 holding R0 and waiting for R1, while P1 holds R1 and waits for R0), identifying `{P0, P1}` as deadlocked.
3. **Deadlock Prevention (Havender's Resource Ordering)**:
   - Assigns a global linear hierarchy to resource types ($R_0 < R_1 < \dots$).
   - Mandates that processes request resources only in strictly ascending order. Eliminates the circular wait condition.

---

## 5. Verification Evidence

### 5.1 Host Unit Test Suite (`make test`)
12 verification test suites execute cleanly on the host:
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
==================================================
   ALL HOST UNIT TESTS COMPLETED SUCCESSFULLY!    
==================================================
```

### 5.2 AI Companion Suite (`make test-ai`)
```
python3 -m unittest discover -s tests -p "test_*.py"
..........
----------------------------------------------------------------------
Ran 10 tests in 0.001s

OK
```

### 5.3 Automated Bare-Metal QEMU Smoke Test (`make test-qemu`)
54 verification checkpoints pass on bare-metal QEMU:
```
==================================================
      JARVIS-OS AUTOMATED QEMU SMOKE TEST         
==================================================
Captured 14951 bytes of serial output in 12.02s.

  [PASS] Early Serial Debug Output
  [PASS] Multiboot 0.6.96 Hand-off
  [PASS] Physical Memory Discovery
  [PASS] BIOS/GRUB Memory Map
  [PASS] Page Allocator Test
  [PASS] Page Allocator Invariants
  [PASS] Page Fault Diagnostics Boot
  [PASS] RAM Disk Test
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

==================================================
   ALL 54 VERIFICATION CHECKPOINTS PASSED!       
==================================================
```

---

## 6. Architectural Boundaries and Limitations

1. **Uniprocessor Execution Model**: The kernel operates in uniprocessor mode. Synchronization primitives rely on atomic built-ins (`__sync_lock_test_and_set`, `__sync_sub_and_fetch`, `__sync_add_and_fetch`) and short critical sections. SMP cross-core memory barriers and IPIs are not implemented.
2. **Fixed Waiting Queue Capacity**: Counting semaphore wait queues hold up to 8 waiting PIDs (`SEM_WAIT_QUEUE_CAPACITY`). If all slots are occupied, subsequent waiting tasks fall back to cooperative scheduler yielding.
3. **Algorithm Simulation Separation**: Deadlock avoidance (Banker) and cycle detection algorithms operate on simulated matrix structures to model multi-resource claims, while runtime kernel mutexes enforce strict single-resource ownership checks.
