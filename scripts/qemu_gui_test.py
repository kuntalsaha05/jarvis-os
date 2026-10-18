#!/usr/bin/env python3
import subprocess
import time
import socket
import os
import sys

def main():
    print("==================================================")
    print("      JARVIS-OS NATIVE GUI VISUAL SMOKE TEST      ")
    print("==================================================")

    mon_sock_path = "/tmp/qemu_gui_mon.sock"
    ppm_term = "build/desktop_term.ppm"
    ppm_sysmon = "build/desktop_sysmon.ppm"
    ppm_ai = "build/desktop_ai.ppm"

    for path in (mon_sock_path, ppm_term, ppm_sysmon, ppm_ai):
        if os.path.exists(path):
            try:
                os.remove(path)
            except OSError:
                pass

    qemu_cmd = [
        "qemu-system-x86_64",
        "-cdrom", "build/jarvis.iso",
        "-m", "256M",
        "-display", "none",
        "-serial", "stdio",
        "-chardev", f"socket,id=mon,path={mon_sock_path},server=on,wait=off",
        "-mon", "chardev=mon,mode=readline"
    ]

    print("[1/5] Launching QEMU with VBE/BGA Framebuffer and Monitor Socket...")
    proc = subprocess.Popen(
        qemu_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

    # Wait for QEMU socket and kernel boot
    time.sleep(2.5)

    if not os.path.exists(mon_sock_path):
        print("FAIL: QEMU monitor socket was not created.")
        proc.kill()
        return 1

    try:
        mon_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        mon_sock.connect(mon_sock_path)
        time.sleep(0.5)
        # Absorb monitor greeting
        mon_sock.recv(1024)

        print("[2/5] Entering Native Graphical Desktop (sending 'gui' command)...")
        proc.stdin.write(b"\ngui\n")
        proc.stdin.flush()
        time.sleep(1.0)

        # Dump Terminal Window Screenshot
        print("  Capturing Terminal Window screendump...")
        mon_sock.sendall(f"screendump {ppm_term}\n".encode("utf-8"))
        time.sleep(0.5)

        print("[3/5] Navigating to System Monitor Window (sending TAB key)...")
        proc.stdin.write(b"\t")
        proc.stdin.flush()
        time.sleep(0.5)

        # Dump SysMon Screenshot
        print("  Capturing System Monitor screendump...")
        mon_sock.sendall(f"screendump {ppm_sysmon}\n".encode("utf-8"))
        time.sleep(0.5)

        print("[4/5] Navigating to AI Assistant Window (sending TAB key)...")
        proc.stdin.write(b"\t")
        proc.stdin.flush()
        time.sleep(0.5)

        # Dump AI Chat Screenshot
        print("  Capturing AI Assistant screendump...")
        mon_sock.sendall(f"screendump {ppm_ai}\n".encode("utf-8"))
        time.sleep(0.5)

        print("[5/5] Returning to Text Console Shell (sending ESC key)...")
        proc.stdin.write(b"\x1b")
        proc.stdin.flush()
        time.sleep(0.5)

        # Send status command to verify text shell is active and responsive
        proc.stdin.write(b"status\n")
        proc.stdin.flush()
        time.sleep(1.0)

        mon_sock.close()
    except Exception as e:
        print(f"Error during GUI interaction: {e}")
        proc.kill()
        return 1

    proc.kill()
    stdout_data, stderr_data = proc.communicate()
    serial_log = stdout_data.decode("utf-8", errors="replace")

    # Verify visual screenshots
    for name, ppm_file in [("Terminal Window", ppm_term),
                           ("System Monitor Window", ppm_sysmon),
                           ("AI Assistant Window", ppm_ai)]:
        if not os.path.exists(ppm_file):
            print(f"FAIL: Expected screenshot '{ppm_file}' for {name} does not exist.")
            return 1

        size = os.path.getsize(ppm_file)
        if size < 500000:
            print(f"FAIL: Screenshot '{ppm_file}' too small ({size} bytes).")
            return 1

        with open(ppm_file, "rb") as f:
            header = f.readline().decode("latin1").strip()
            dim_line = f.readline().decode("latin1").strip()
            max_val = f.readline().decode("latin1").strip()
            all_pixels = f.read()

        if header != "P6":
            print(f"FAIL: Invalid PPM header '{header}' in {ppm_file}.")
            return 1

        unique_colors = len(set(all_pixels))
        print(f"  [PASS] {name} Screenshot: {ppm_file} ({size} bytes, dims={dim_line}, unique_bytes={unique_colors})")
        if unique_colors < 10:
            print(f"FAIL: Screenshot {ppm_file} lacks visual diversity (blank or frozen screen).")
            return 1

    # Verify serial log transitions
    if "[GUI] Starting Native JARVIS Graphical Desktop" not in serial_log:
        print("FAIL: Desktop startup banner missing in serial log.")
        return 1

    if "[GUI] Exited Graphical Desktop. Text shell console restored." not in serial_log:
        print("FAIL: Desktop exit / shell restoration banner missing in serial log.")
        return 1

    print("\n==================================================")
    print("   ALL NATIVE GUI VISUAL SMOKE TESTS PASSED!      ")
    print("==================================================")
    return 0

if __name__ == "__main__":
    sys.exit(main())
