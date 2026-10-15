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
        "sched\n"
        "paging\n"
        "deadlock\n"
        "disk\n"
        "sync\n"
        "files\n"
    ).encode("utf-8")

    start_time = time.time()
    try:
        proc = subprocess.Popen(
            qemu_cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        stdout_data, stderr_data = proc.communicate(input=commands_to_send, timeout=7)
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
        ("Page Allocator Test", "Page allocator test: PASS"),
        ("RAM Disk Test", "RAM disk test: PASS"),
        ("Kernel Online", "Status: ONLINE"),
        ("Cooperative/Preemption Switching", "Task A iteration"),
        ("Task A/B Return", "Context-switch test: returned to kernel"),
        ("Shell Ready", "jarvis>"),
        ("CPU Scheduling Simulation", "CPU SCHEDULING ALGORITHM BENCHMARK"),
        ("Page Replacement Benchmark", "PAGE REPLACEMENT ALGORITHM BENCHMARK"),
        ("Belady's Anomaly", "ANOMALY CONFIRMED!"),
        ("Banker's Deadlock Avoidance", "BANKER'S DEADLOCK AVOIDANCE"),
        ("Banker Safe Sequence", "Safe sequence found"),
        ("Disk Scheduling Simulation", "DISK SCHEDULING BENCHMARK"),
        ("Synchronization Invariants", "SYNCHRONIZATION & IPC BENCHMARK"),
        ("Producer-Consumer Invariant", "Bounded buffer, FIFO order"),
        ("Reader-Writer Invariant", "Concurrent read allowed; exclusive write enforced"),
        ("JARVIS-FS Directory", "JARVIS-FS Directory Listing")
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
        print("   ALL 16 VERIFICATION CHECKPOINTS PASSED!       ")
        print("==================================================")
        return 0
    else:
        print("   SOME CHECKPOINTS FAILED. SEE OUTPUT ABOVE.    ")
        print("==================================================")
        return 1

if __name__ == "__main__":
    sys.exit(main())

