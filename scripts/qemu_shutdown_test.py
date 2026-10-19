#!/usr/bin/env python3
import subprocess
import time
import sys

def main():
    print("==================================================")
    print("   JARVIS-OS CONTROLLED SHUTDOWN VERIFICATION     ")
    print("==================================================")

    qemu_cmd = [
        "qemu-system-x86_64",
        "-cdrom", "build/jarvis.iso",
        "-m", "256M",
        "-display", "none",
        "-serial", "stdio"
    ]

    print("[1/3] Booting JARVIS-OS in QEMU...")
    proc = subprocess.Popen(
        qemu_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

    # Wait for kernel initialization and shell prompt
    time.sleep(2.5)

    print("[2/3] Issuing 'shutdown' command to kernel shell...")
    try:
        proc.stdin.write(b"shutdown\n")
        proc.stdin.flush()
    except (BrokenPipeError, OSError) as e:
        print(f"Error sending shutdown: {e}")

    print("[3/3] Waiting for ACPI power-off and clean QEMU exit...")
    try:
        stdout_data, stderr_data = proc.communicate(timeout=6.0)
    except subprocess.TimeoutExpired:
        print("FAIL: QEMU did not exit within 6.0 seconds after shutdown command.")
        proc.kill()
        stdout_data, stderr_data = proc.communicate()
        return 1

    output = stdout_data.decode("utf-8", errors="replace")
    print(f"Captured {len(output)} bytes of serial output during shutdown.\n")

    # Verification checkpoints
    checkpoints = [
        ("Shutdown Banner", "JARVIS-OS SYSTEM SHUTDOWN"),
        ("Volatile RAMDisk Disclosure", "Storage is backed by volatile RAMDisk (/dev/ram0). Data is not retained across power cycles."),
        ("ACPI Power-off Invocation", "Powering off system via ACPI / PM interface"),
    ]

    all_passed = True
    for name, marker in checkpoints:
        if marker in output:
            print(f"  [PASS] {name}")
        else:
            print(f"  [FAIL] {name} (marker '{marker}' not found)")
            all_passed = False

    # Check exit code
    if proc.returncode == 0:
        print(f"  [PASS] QEMU Exit Code: {proc.returncode} (Clean Power-Off)")
    else:
        print(f"  [FAIL] QEMU Exit Code: {proc.returncode}")
        all_passed = False

    print("\n==================================================")
    if all_passed:
        print("   CONTROLLED SHUTDOWN VERIFICATION: PASS        ")
        print("==================================================")
        return 0
    else:
        print("   CONTROLLED SHUTDOWN VERIFICATION: FAIL        ")
        print("==================================================")
        return 1

if __name__ == "__main__":
    sys.exit(main())

