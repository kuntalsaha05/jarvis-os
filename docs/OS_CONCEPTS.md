# JARVIS-OS — Operating System Concepts & Algorithms

This document provides academic descriptions, mathematical formulations, and implementation details for the core OS concepts demonstrated in JARVIS-OS.

---

## 1. Process Scheduling

### 1.1 Metrics Formulation
For each process $i$ with arrival time $A_i$, burst time $B_i$, and completion time $C_i$:
- **Turnaround Time ($TAT_i$)**: Total time elapsed from submission to completion:
  $$TAT_i = C_i - A_i$$
- **Waiting Time ($WT_i$)**: Total time spent waiting in the ready queue:
  $$WT_i = TAT_i - B_i$$
- **Response Time ($RT_i$)**: Time from arrival to first execution:
  $$RT_i = \text{Start}_i - A_i$$

### 1.2 Algorithms Implemented
1. **First-Come, First-Served (FCFS)**:
   - Non-preemptive scheduling in order of arrival. Simple and fair, but prone to the **convoy effect** where long CPU-burst tasks delay short interactive tasks.
2. **Shortest Job First (SJF)**:
   - Selects the task with the smallest CPU burst time from among currently arrived tasks. Mathematically optimal for minimizing average waiting time, but requires knowing or estimating burst times.
3. **Round Robin (RR)**:
   - Preemptive scheduling with a fixed time quantum $q$ ($q = 5$ ticks in kernel scheduler; configurable in benchmark simulator). Ensures bounded response times and prevents task starvation.

---

## 2. Virtual Memory & Page Replacement

### 2.1 Paging Concepts
- Physical memory is organized into fixed-size **page frames** (4096 bytes on x86-64).
- The page allocator manages a bitmap representing available physical frames, ensuring $O(1)$ allocation and deallocation with strict 4 KiB alignment checks.

### 2.2 Page Replacement Algorithms
When a page fault occurs and all memory frames are occupied, a victim page must be evicted:
1. **FIFO (First-In, First-Out)**:
   - Replaces the oldest loaded page. Simple queue-based logic.
2. **LRU (Least Recently Used)**:
   - Replaces the page that has not been accessed for the longest duration of time, approximating future behavior through past locality of reference.
3. **Optimal (Belady's MIN Algorithm)**:
   - Replaces the page that will not be accessed for the longest period in the future. Serves as the theoretical upper bound for page hit performance.

### 2.3 Belady's Anomaly
In FIFO page replacement, increasing the number of physical memory frames can paradoxically **increase** the number of page faults.

**Demonstrated Reference String**:
$$1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5$$

- **With 3 Frames**: Generates **9 page faults**.
- **With 4 Frames**: Generates **10 page faults**.

In JARVIS-OS, typing `paging` or running `make test` verifies this anomaly deterministically.

---

## 3. Concurrency, Synchronization & IPC

### 3.1 Race Conditions and Critical Sections
When multiple processes access shared state, race conditions can corrupt data invariants. Synchronization requires mutual exclusion, progress, and bounded waiting.

### 3.2 Implemented Primitives
- **Counting Semaphores**: Maintains an integer counter and supports atomic `wait` (decrement/block) and `post` (increment/unblock) operations.
- **Mutex Locks**: Binary exclusion primitive implemented with atomic test-and-set operations (`__sync_lock_test_and_set`).

### 3.3 Classic Synchronization Problems
1. **Producer-Consumer (Bounded Buffer)**:
   - Employs three primitives: `empty` semaphore (initialized to buffer capacity $N$), `full` semaphore (initialized to 0), and a mutual exclusion lock.
   - Guaranteed Invariants: buffer never exceeds $N$, never underflows below 0, and items are extracted in FIFO sequence.
2. **Reader-Writer**:
   - Multiple readers may access shared data simultaneously; writers require exclusive access.
   - When a writer is active, all readers and subsequent writers are locked out. When readers are active, writers are blocked until the last reader calls `reader_exit()`.

---

## 4. Deadlock Avoidance: Banker's Algorithm

### 4.1 Deadlock Conditions (Coffman Conditions)
Deadlock occurs when four conditions hold simultaneously: Mutual Exclusion, Hold and Wait, No Preemption, and Circular Wait.

### 4.2 The Banker's Algorithm Formulation
Devised by Edsger Dijkstra, Banker's algorithm tests for safety by simulating the allocation of predetermined maximum possible amounts of all resources:

- **Matrices and Vectors**:
  - Available Vector $V \in \mathbb{R}^m$: $V[j]$ represents instances of resource type $R_j$ currently available.
  - Max Matrix $M \in \mathbb{R}^{n \times m}$: $M[i][j]$ is the maximum claim of process $P_i$.
  - Allocation Matrix $A \in \mathbb{R}^{n \times m}$: $A[i][j]$ is the current allocation to process $P_i$.
  - Need Matrix $N \in \mathbb{R}^{n \times m}$: Calculated as:
    $$N[i][j] = M[i][j] - A[i][j]$$

- **Safety Algorithm**:
  1. Let $\text{Work} = V$ and $\text{Finish}[i] = \text{false}$ for all $i \in \{0, \dots, n-1\}$.
  2. Find an index $i$ such that $\text{Finish}[i] == \text{false}$ and $N[i] \le \text{Work}$.
  3. If no such $i$ exists, go to step 4. Otherwise:
     $$\text{Work} = \text{Work} + A[i]$$
     $$\text{Finish}[i] = \text{true}$$
     Record $P_i$ in the safe sequence and repeat step 2.
  4. If $\text{Finish}[i] == \text{true}$ for all $i$, the system is in a **Safe State**.

- **Resource-Request Validation**:
  When process $P_i$ issues a request $\text{Req}$:
  1. If $\text{Req}[j] > N[i][j]$, raise an error (request exceeds maximum claim).
  2. If $\text{Req}[j] > V[j]$, process must wait (insufficient available resources).
  3. Pretend-allocate:
     $$V = V - \text{Req}, \quad A[i] = A[i] + \text{Req}, \quad N[i] = N[i] - \text{Req}$$
  4. Run Safety Algorithm. If safe $\to$ approve allocation; if unsafe $\to$ rollback and deny.

---

## 5. Disk Head Scheduling

Disk scheduling algorithms order outstanding I/O track requests to minimize mechanical seek time (total cylinder traversal distance):

1. **FCFS (First-Come, First-Served)**:
   - Services requests in the order they arrive.
2. **SSTF (Shortest Seek Time First)**:
   - Greedily selects the pending request closest to the current head position. Prone to starvation of distant tracks.
3. **SCAN (Elevator Algorithm)**:
   - The head sweeps in one direction servicing requests until it reaches the edge of the disk, then reverses direction.
4. **C-SCAN (Circular SCAN)**:
   - The head sweeps in one direction to the end of the disk, then immediately jumps back to track 0 without servicing requests on the return trip, providing a more uniform wait time.

---

## 6. Memory Allocation Strategies (Fit Algorithms)

When satisfying memory requests of variable size from a set of free partitions (holes):

1. **First Fit**:
   - Scans partitions from the beginning and allocates the first hole that is large enough ($\text{size} \ge \text{request}$).
   - Advantage: Fast search time ($O(N)$ worst-case, often terminating early).
2. **Best Fit**:
   - Searches the entire partition list and allocates the smallest hole that is big enough:
     $$\text{hole}^* = \arg\min_{\text{hole} \ge \text{req}} (\text{size}(\text{hole}) - \text{req})$$
   - Advantage: Leaves larger blocks intact for subsequent large requests.
   - Disadvantage: Generates small, unusable leftover slivers.
3. **Worst Fit**:
   - Searches the entire partition list and allocates the largest available hole:
     $$\text{hole}^* = \arg\max_{\text{hole} \ge \text{req}} (\text{size}(\text{hole}))$$
   - Concept: Leaves behind a remaining hole that is still large enough to be useful.
4. **External Fragmentation**:
   - Occurs when total free memory across all unallocated blocks is sufficient to satisfy a request, but no single contiguous partition is large enough:
     $$\text{External Frag} = \sum_{\text{unallocated}} \text{size}(\text{block}) \quad \text{when } \max(\text{size}) < \text{req}$$

---

## 7. File Allocation Methods

In file system architecture, allocating disk sectors to files determines random-access performance and storage efficiency:

1. **Contiguous Allocation**:
   - Each file occupies a contiguous set of physical disk blocks (defined by `start_block` and `length`).
   - **Access**: $O(1)$ direct access via $\text{physical} = \text{start} + \text{logical}$. Fastest sequential and random I/O.
   - **Limitation**: Suffers from external disk fragmentation and file growth constraints.
2. **Linked Allocation**:
   - Each file is a linked list of disk blocks. Each sector contains a pointer to the next sector in the chain.
   - **Access**: $O(N)$ sequential traversal required to reach logical block $k$.
   - **Advantage**: Zero external fragmentation; files can grow dynamically anywhere free blocks exist.
3. **Indexed Allocation**:
   - Brings all block pointers together into an **index block** (or inode direct block array).
   - **Access**: $O(1)$ random access by indexing into the pointer array at $\text{index\_table}[k]$.
   - **Overhead**: Requires 1 dedicated index block per file, which may be underutilized for tiny files.

---

## 8. Deadlock Detection & Prevention

Beyond Dijkstra's avoidance, operating systems manage deadlocks via detection and prevention:

### 8.1 Deadlock Detection (Matrix & Wait-For Graph)
When a system does not employ avoidance, deadlocks may occur and must be detected:
- **Available Vector $W$**, **Allocation Matrix $A$**, and **Request Matrix $Q$**.
- A process $P_i$ can finish if its outstanding request $Q[i] \le W$.
- If $Q[i] \le W$, simulate completion: $W = W + A[i]$, mark $P_i$ as finishable.
- If algorithm terminates with processes whose requests cannot be satisfied, those processes constitute a **deadlocked cycle** (circular wait).

### 8.2 Deadlock Prevention (Havender's Resource Ordering)
Prevention eliminates deadlock mathematically by invalidating at least one of the four Coffman conditions.
- **Havender's Linear Ordering**: Assign a global unique integer priority $F(R_j)$ to every resource type $R_j$.
- **Invariant Rule**: A process holding resource $R_i$ may request resource $R_j$ if and only if $F(R_j) > F(R_i)$.
- **Proof**: If a circular wait $P_0 \to P_1 \to \dots \to P_k \to P_0$ were to exist, then $F(R_0) < F(R_1) < \dots < F(R_k) < F(R_0)$, which implies $F(R_0) < F(R_0)$—a mathematical contradiction. Hence, circular wait is impossible.

