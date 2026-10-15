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
        "threads\n"
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
    ).encode("utf-8")

    start_time = time.time()
    try:
        proc = subprocess.Popen(
            qemu_cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        stdout_data, stderr_data = proc.communicate(input=commands_to_send, timeout=10)
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
        ("RAM Disk Test", "RAM disk test: PASS"),
        ("Kernel Online", "Status: ONLINE"),
        ("Cooperative Switching", "Task A iteration"),
        ("Cooperative Return", "Context-switch test: returned to kernel"),
        ("Timer Preemption Execution", "PIT timer preemption test starting"),
        ("Timer Preemption Quanta Switch", "[PREEMPT 1] Preempted across timer quanta, completed!"),
        ("Timer Preemption Verified", "PIT timer preemption test: PASS (involuntary timer preemption verified)"),
        ("Shell Ready", "jarvis>"),
        ("Process & Thread Metrics", "Process & Thread Subsystem Statistics:"),
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
        ("JARVIS-FS Directory", "JARVIS-FS Directory Listing"),
        ("Software Interrupt Syscall", "SYSTEM CALL DISPATCH BENCHMARK"),
        ("Syscall int $0x80 Execution", "SYS_GET_TICKS via int $0x80")
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

