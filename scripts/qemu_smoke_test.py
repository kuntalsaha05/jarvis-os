#!/usr/bin/env python3
import subprocess
import time
import sys

def main():
    print("==================================================")
    print("      JARVIS-OS AUTOMATED QEMU SMOKE TEST         ")
    print("==================================================")

    qemu_cmd = [
        "qemu-system-x86_64",
        "-cdrom", "build/jarvis.iso",
        "-m", "256M",
        "-display", "none",
        "-serial", "stdio"
    ]

    commands_to_send = (
        "\n"
        "status\n"
        "irq\n"
        "threads\n"
        "memory\n"
        "pftest\n"
        "ipc\n"
        "df\n"
        "touch test_smoke.txt\n"
        "write test_smoke.txt Hello_Jarvis_FS\n"
        "cat test_smoke.txt\n"
        "trunc test_smoke.txt 5\n"
        "cat test_smoke.txt\n"
        "rm test_smoke.txt\n"
        "fstest\n"
        "ls\n"
        "cat kernel.cfg\n"

        "sched\n"
        "paging\n"
        "fit\n"
        "alloc\n"
        "deadlock\n"
        "detect\n"
        "disk\n"
        "sync\n"
        "files\n"
        "syscall\n"
        "ai\n"
    ).encode("utf-8")

    start_time = time.time()
    try:
        proc = subprocess.Popen(
            qemu_cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        stdout_data, stderr_data = proc.communicate(input=commands_to_send, timeout=12)

    except subprocess.TimeoutExpired:
        proc.kill()
        stdout_data, stderr_data = proc.communicate()
    except Exception as e:
        print(f"Error executing QEMU: {e}")
        return 1

    output = stdout_data.decode("utf-8", errors="replace")
    duration = time.time() - start_time

    print(f"Captured {len(output)} bytes of serial output in {duration:.2f}s.\n")

    required_checkpoints = [
        ("Early Serial Debug Output", "[JARVIS-OS] Early serial debug online."),
        ("Multiboot 0.6.96 Hand-off", "Bootloader Hand-off: Multiboot 0.6.96 Verified"),
        ("Physical Memory Discovery", "Physical Memory: Lower"),
        ("BIOS/GRUB Memory Map", "BIOS/GRUB Memory Map:"),
        ("Page Allocator Test", "Page allocator test: PASS"),
        ("Page Allocator Invariants", "Page allocator invariants: PASS"),
        ("Page Fault Diagnostics Boot", "Page fault diagnostics: PASS"),
        ("RAM Disk Test", "RAM disk test: PASS"),
        ("Block Device Invariants Boot", "Block device invariants: PASS"),
        ("Filesystem Invariants Boot", "Filesystem invariants: PASS"),
        ("Cooperative Switching", "Task A iteration"),
        ("Cooperative Return", "Context-switch test: returned to kernel"),
        ("Register/Stack Integrity Test", "Register/stack integrity test: PASS"),
        ("Timer Preemption Execution", "PIT timer preemption test starting"),
        ("Timer Preemption Quanta Switch", "[PREEMPT 1] Preempted across timer quanta, completed!"),
        ("Timer Preemption Verified", "PIT timer preemption test: PASS (involuntary timer preemption verified)"),
        ("Kernel Online", "Status: ONLINE"),
        ("Shell Ready", "jarvis>"),
        ("Interrupt Subsystem IRQ0/1", "Interrupt Subsystem Status:"),
        ("Timer IRQ0 Vector", "Vector 32: timer_stub (IDT gate 0x20)"),
        ("Keyboard IRQ1 Vector", "Vector 33: keyboard_stub (IDT gate 0x21)"),
        ("Process & Thread Metrics", "Process & Thread Subsystem Statistics:"),
        ("Memory Subsystem Boundary", "Identity-Mapped Boundary: 0x00000000 - 0x001FFFFF"),
        ("Page Fault Decoder Self-Test", "Diagnostic decoder test: PASS"),
        ("Page Fault Diagnosis Output", "Sample Diagnosis 1 (Simulated supervisor read at 0x00205000)"),

        ("Context Switch Counting", "Total Context Switches:"),
        ("Quantum Preemption Counting", "Quantum Preemptions:"),
        ("Directory Listing (ls)", "JARVIS-FS Directory Listing:"),
        ("File Readback (cat)", "--- Content of kernel.cfg ---"),
        ("CPU Scheduling Simulation", "CPU SCHEDULING ALGORITHM BENCHMARK"),
        ("Page Replacement Benchmark", "PAGE REPLACEMENT ALGORITHM BENCHMARK"),
        ("Belady's Anomaly", "ANOMALY CONFIRMED!"),
        ("Memory Allocation Fit Benchmark", "MEMORY ALLOCATION STRATEGIES DEMO"),
        ("Best Fit Allocs", "Summary: 4 allocated, 0 failed"),
        ("File Allocation Methods Benchmark", "FILE ALLOCATION METHODS DEMO"),
        ("File Allocation Lookup", "Logical block 2 of file1 -> Physical Block 2"),
        ("Banker's Deadlock Avoidance", "BANKER'S DEADLOCK AVOIDANCE"),
        ("Banker Safe Sequence", "Safe sequence found"),
        ("Deadlock Detection & Prevention", "DEADLOCK DETECTION & PREVENTION"),
        ("Deadlock Detection Cycle Found", "Deadlock Detected in Processes: P0, P1"),
        ("Deadlock Prevention Havender", "Deadlock Prevention (Havender's Resource Ordering)"),
        ("Disk Scheduling Simulation", "DISK SCHEDULING BENCHMARK"),
        ("Synchronization Invariants", "SYNCHRONIZATION & IPC BENCHMARK"),
        ("Producer-Consumer Invariant", "Bounded buffer, FIFO order"),
        ("Reader-Writer Invariant", "Concurrent read allowed; exclusive write enforced"),
        ("Mutex Ownership Protection", "Owner PID tracked; non-owner unlock rejected"),
        ("Counting Semaphore Invariants", "Atomic counting, bounded limits, underflow/overflow protection"),
        ("IPC Queue Invariants Boot", "IPC queue invariants: PASS"),
        ("Synchronization Invariants Boot", "Synchronization invariants: PASS"),
        ("IPC Message Passing Benchmark", "MESSAGE PASSING IPC BENCHMARK"),
        ("IPC Queue Underflow", "Queue underflow check: PASS"),
        ("IPC Queue Telemetry", "Queue Metrics Telemetry:"),
        ("IPC Subsystem Invariants Shell", "IPC Subsystem Invariants Verification: PASS"),
        ("JARVIS-FS Directory", "JARVIS-FS Directory Listing"),
        ("Software Interrupt Syscall", "SYSTEM CALL DISPATCH BENCHMARK"),
        ("Syscall int $0x80 Execution", "SYS_GET_TICKS via int $0x80"),
        ("Storage Volume Metrics (df)", "JARVIS-FS Storage & Volume Metrics:"),
        ("Touch File Creation", "touch: created file 'test_smoke.txt'"),
        ("Write File Content", "write: successfully wrote 15 bytes to 'test_smoke.txt'"),
        ("Truncate File Size", "trunc: truncated 'test_smoke.txt' to 5 bytes"),
        ("Remove File", "rm: deleted file 'test_smoke.txt'"),
        ("Filesystem Invariants Shell", "JARVIS-FS Invariants:    PASS"),
        ("Block Device Invariants Shell", "Block Device Invariants: PASS"),
        ("AI Status Shell Command", "JARVIS AI ARCHITECTURE STATUS"),
        ("AI Security 4-Tier Model", "Enforced Security & Permission Model:")
    ]


    all_passed = True
    for label, pattern in required_checkpoints:
        if pattern in output:
            print(f"  [PASS] {label}")
        else:
            print(f"  [FAIL] {label} (missing pattern: '{pattern}')")
            all_passed = False

    print("\n==================================================")
    if all_passed:
        print(f"   ALL {len(required_checkpoints)} VERIFICATION CHECKPOINTS PASSED!       ")
        print("==================================================")
        return 0
    else:
        print("   SOME CHECKPOINTS FAILED. SEE OUTPUT ABOVE.    ")
        print("==================================================")
        return 1

if __name__ == "__main__":
    sys.exit(main())

