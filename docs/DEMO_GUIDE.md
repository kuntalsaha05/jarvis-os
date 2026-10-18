# JARVIS-OS — Live Demonstration & Evaluation Guide

**Milestone:** Phase 14 Release Candidate (v1.0.0-RC1)  
**Target:** Evaluators, Reviewers & Operating System Students  
**Architecture:** x86-64 Bare-Metal Kernel + Host AI Companion  

---

## 1. Prerequisites & Environment Setup

Ensure the following packages are installed on your Linux / WSL2 host:

```bash
# Ubuntu / Debian prerequisites
sudo apt update
sudo apt install -y build-essential gcc gdb qemu-system-x86 nasm xorriso grub-pc-bin grub-common python3 python3-pip

# Cross-compiler (x86_64-elf-gcc)
# If not already installed, ensure x86_64-elf-gcc is in your PATH.
x86_64-elf-gcc --version
```

---

## 2. Building the Release Candidate

Execute the automated build targets from the root repository directory:

```bash
cd ~/jarvis-os

# 1. Clean previous build artifacts
make clean

# 2. Compile kernel binary (build/jarvis.bin)
make

# 3. Package bootable GRUB ISO (build/jarvis.iso)
make iso
```

Verified artifact output:
- **Location**: `build/jarvis.iso`
- **Size**: ~5.1 MB
- **Format**: El Torito / Multiboot 1 bootable hybrid ISO image

---

## 3. Automated Verification Suites

Before interactive evaluation, run the comprehensive verification targets:

```bash
# 1. Run all host algorithm & stress unit tests (17 test suites)
make test

# 2. Run Python AI companion tests (22 unit tests)
make test-ai

# 3. Run bare-metal QEMU smoke test (65 checkpoints)
make test-qemu

# 4. Run native GUI visual smoke tests (captures 6 PPM screendumps)
make test-gui

# 5. Run end-to-end release workflow validation (13 steps)
make test-release

# 6. Run all test suites in sequence
make test-all

# 7. Run empirical performance benchmarks
make bench-full
```

All commands must exit with code `0`.

---

## 4. Interactive Kernel Shell Walkthrough

Launch the OS in QEMU:

```bash
make run
```
*(For headless/terminal-only testing without a display window, run `make run-headless`)*.

### 4.1 System & Hardware Telemetry
Type the following commands into the `jarvis>` shell prompt:

```text
status
```
*Expected Output*: Kernel status (`ONLINE`), uptime ticks, terminal outputs, and memory summary.

```text
irq
```
*Expected Output*: IDT gate configuration, PIC remapping (Master 0x20, Slave 0x28), PIT frequency (100 Hz), and keyboard IRQ1 status.

```text
memory
```
*Expected Output*: Physical page allocator bitmap telemetry, active frames, and heap allocation metrics.

```text
threads
```
*Expected Output*: Task Control Block (TCB) table showing PID 0 (Idle task), PID 1 (Shell), priorities, states (`RUNNING`, `READY`), and stack pointers.

### 4.2 Storage & Resilient Filesystem (JARVIS-FS)
```text
ls
```
*Expected Output*: Directory listing showing default `kernel.cfg` file and storage statistics.

```text
touch test.txt
write test.txt Operating_System_Demonstration
cat test.txt
```
*Expected Output*: Displays `Operating_System_Demonstration`.

```text
trunc test.txt 16
cat test.txt
```
*Expected Output*: Displays truncated content `Operating_System_`.

```text
rm test.txt
ls
```
*Expected Output*: `test.txt` removed cleanly, blocks returned to free pool.

### 4.3 Educational Algorithm Demonstrations
```text
sched
```
*Expected Output*: Runs CPU scheduling simulation comparing FCFS, SJF, and Round Robin timelines, completion times, and wait times.

```text
paging
```
*Expected Output*: Compares FIFO, LRU, and Optimal page replacement algorithms and executes the Belady anomaly confirmation (faults increase from 9 to 10 when frames increase from 3 to 4).

```text
banker
```
*Expected Output*: Evaluates Banker's Deadlock Avoidance algorithm with Allocation, Max, Need, and Available matrices; computes the safe sequence `<P1 -> P3 -> P4 -> P0 -> P2>` and demonstrates unsafe allocation rejection.

```text
sync
```
*Expected Output*: Runs Producer-Consumer bounded buffer demonstration with semaphores and Reader-Writer concurrency with mutexes.

```text
disk
```
*Expected Output*: Simulates disk head scheduling comparing FCFS (640), SSTF (236), SCAN (331), and C-SCAN (382) cylinder movements.

---

## 5. Native Graphical Desktop Walkthrough

From the interactive text shell, start the graphical environment:

```text
gui
```

### 5.1 Desktop Interface Overview
The screen automatically transitions into high-resolution $1024 \times 768 \times 32\text{ bpp}$ mode.
- **Top Status Bar**: Displays system title, active window name, live uptime ticks, and long-mode architecture status.
- **Bottom Taskbar**: Shortcut buttons for applications (`1: Term`, `2: SysMon`, `3: Files`, `4: Proc`, `5: AI Chat`), theme switcher (`6: Theme`), and shell exit (`ESC: Shell`).
- **Hardware Mouse**: Smooth hardware arrow cursor driven by PS/2 mouse driver with packet decoding.

### 5.2 Window Interactions
1. **Focus Cycling**: Press `TAB` to cycle keyboard focus through open windows. The active window promotes to the top of the z-order with an accent border.
2. **Window Dragging**: Click and hold the mouse on any window's title bar ($y \in [0, 24]$ px relative to window), move the mouse, and release. The window moves smoothly and clamps to work area boundaries.
3. **Window Controls**:
   - Click `_` (top right): Minimizes the window to the taskbar.
   - Click `[]` or `^`: Maximizes the window to fill the entire desktop work area. Clicking again restores original geometry.
   - Click `X`: Closes the window and frees all resources.
4. **Desktop Applications**:
   - Click `1: Term` on the taskbar to focus the Graphical Terminal. Type shell commands directly inside the window (`echo hello`, `ticks`, etc.).
   - Click `2: SysMon` to view live CPU usage, uptime, and physical page allocation gauges.
   - Click `3: Files` to browse JARVIS-FS files and inode allocations.
   - Click `4: Proc` to monitor active process states and Round-Robin scheduler metrics.
   - Click `5: AI Chat` to view AI companion connection status, protocol version, and security indicators.
5. **Theme Switching**:
   - Press the `T` key or click `6: Theme` on the taskbar. The desktop cycles through **JARVIS Dark**, **Cyberpunk**, and **Classic Slate**.
   - Theme settings are automatically saved to `/desktop.cfg` with checksum integrity.
6. **Clean Shell Restoration**:
   - Press the `ESC` key or click `ESC: Shell` on the taskbar.
   - The desktop loop terminates cleanly, and the interactive text shell (`jarvis>`) is fully restored without rebooting.

---

## 6. Host AI Companion Walkthrough

Open a separate terminal window on your host workstation to exercise the AI companion:

```bash
# 1. Run AI companion interactive console
python3 -m jarvis.main --interactive
```

### 6.1 Querying Kernel Telemetry
```text
jarvis-ai> What is the current memory status?
```
*Expected Output*: Dispatches `memory.get_stats`, retrieves page pool metrics, and synthesizes a response explicitly disclosing `[LIVE_KERNEL]` (if connected via COM1) or `[OFFLINE_SIMULATION]` (if running standalone).

### 6.2 OS Concept Explanations
```text
jarvis-ai> Explain the Banker's algorithm and Belady's anomaly
```
*Expected Output*: Provides structured academic explanations backed by local LLM inference or deterministic mock fixture.

### 6.3 Security Authorization & Gating
Attempt to invoke a privileged tool:
```text
jarvis-ai> trigger a kernel panic
```
*Behavior*:
- The agent detects `system.trigger_panic` has permission level `PRIVILEGED`.
- Prompts user for explicit confirmation: `[Security Prompt: Approve execution of privileged tool 'system.trigger_panic'? (y/n)]`.
- Entering `n` rejects execution with `Permission Denied` and logs the denial to the audit trail.
- Entering `y` approves and executes the action.

### 6.4 Inspecting Audit Logs
```bash
python3 -m jarvis.main --audit
```
*Expected Output*: Prints timestamped audit records with tool name, arguments, status (`SUCCESS`, `DENIED`, `APPROVAL_REQUIRED`), and runtime in milliseconds.

---

## 7. Troubleshooting & Common Questions

- **Q: Ollama model is not responding or reports offline.**  
  **A**: If Ollama is running in Windows while you are in WSL, ensure Ollama is listening on `0.0.0.0:11434` (`OLLAMA_HOST=0.0.0.0`) and reach it via your host IP. When Ollama is offline, JARVIS-OS automatically falls back to deterministic local responses without failing.
- **Q: Does desktop configuration survive a cold reboot?**  
  **A**: Settings saved to `/desktop.cfg` persist across the running session and multiple GUI/shell switches, but reset on a cold hardware reboot because JARVIS-FS uses an in-memory RAM disk.
- **Q: How to run tests without launching graphics?**  
  **A**: `make test-all` runs completely headlessly. All visual smoke tests use QEMU screendump sockets without requiring an X11/Wayland display.

