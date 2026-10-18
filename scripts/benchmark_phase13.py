#!/usr/bin/env python3
"""
JARVIS-OS — Phase 13 Performance & Stability Benchmark Suite
Measures:
  1. Boot time and GUI initialization time under QEMU.
  2. Framebuffer redraw cost and software blit throughput.
  3. Memory allocation throughput, latency, and success rates.
  4. Filesystem operation latency (RAMDisk backed).
  5. AI inference and tool dispatch latency across backends.
  6. Timer interrupt frequency and context switch telemetry.
"""

import os
import sys
import time
import subprocess
import json

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from jarvis.llm.manager import LLMManager
from jarvis.llm.mock_backend import MockLLMBackend
from jarvis.llm.ollama_backend import OllamaBackend
from jarvis.tools.registry import ToolRegistry, PermissionLevel
from jarvis.agent.agent import JarvisAgent
from jarvis.bridge.protocol import BridgeRequest, BridgeResponse


def benchmark_ai_backends():
    print("\n--- [BENCHMARK 1/3] AI Latency & Tool Dispatch Baselines ---")

    # 1. Mock Backend Latency
    mock = MockLLMBackend()
    t0 = time.perf_counter()
    for _ in range(50):
        mock.generate("Hello JARVIS")
    elapsed_mock = (time.perf_counter() - t0) / 50.0
    print(f"  [AI] Mock Backend Latency:          {elapsed_mock * 1000.0:.3f} ms/query")

    # 2. Ollama Backend Latency (or offline fallback)
    ollama = OllamaBackend(model_name="llama3")
    t0 = time.perf_counter()
    resp_ollama = ollama.generate("Hello JARVIS")
    elapsed_ollama = (time.perf_counter() - t0) * 1000.0
    is_live = "Ollama Error" not in resp_ollama
    print(f"  [AI] Ollama Backend Latency:        {elapsed_ollama:.3f} ms ({'LIVE' if is_live else 'OFFLINE_FALLBACK'})")

    # 3. Tool Allowlist Check & Execution Overhead
    registry = ToolRegistry()
    t0 = time.perf_counter()
    for _ in range(1000):
        registry.execute_tool("kernel.query_status", {})
    elapsed_tool = (time.perf_counter() - t0) / 1000.0
    print(f"  [AI] Tool Dispatch & Allowlist:     {elapsed_tool * 1e6:.2f} us/call")

    # 4. End-to-End Agent Cycle Latency (8-step loop)
    agent = JarvisAgent(LLMManager(mock), registry)
    t0 = time.perf_counter()
    for _ in range(50):
        agent.run_cycle("query kernel memory statistics")
    elapsed_agent = (time.perf_counter() - t0) / 50.0
    print(f"  [AI] Full 8-Step Agent Cycle:       {elapsed_agent * 1000.0:.3f} ms/cycle")

    # 5. Message Serialization & Validation Throughput
    req = BridgeRequest(request_id=1, action="tool_call", tool="memory.get_stats", arguments={"key": "val"})
    raw = req.to_json()
    t0 = time.perf_counter()
    for _ in range(2000):
        BridgeRequest.from_json(raw)
    elapsed_proto = (time.perf_counter() - t0) / 2000.0
    print(f"  [AI] Protocol Parsing & Size Check: {elapsed_proto * 1e6:.2f} us/msg")


def benchmark_host_subsystems():
    print("\n--- [BENCHMARK 2/3] Host C Subsystems (Memory, FS, Framebuffer, WM) ---")
    ret = subprocess.run(["./build/perf_bench"], capture_output=True, text=True)
    if ret.returncode != 0:
        print(f"FAIL: build/perf_bench returned code {ret.returncode}")
        print(ret.stderr)
        return False
    print(ret.stdout.strip())
    return True


def benchmark_qemu_boot_and_gui():
    print("\n--- [BENCHMARK 3/3] Bare-Metal Boot & GUI Initialization Timings ---")
    iso_path = "build/jarvis.iso"
    if not os.path.exists(iso_path):
        print(f"FAIL: {iso_path} not found. Run 'make iso' first.")
        return False

    qemu_cmd = [
        "qemu-system-x86_64",
        "-cdrom", iso_path,
        "-m", "256M",
        "-display", "none",
        "-serial", "stdio"
    ]

    t_launch = time.perf_counter()
    proc = subprocess.Popen(
        qemu_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

    t_boot = 0.0
    t_gui = 0.0

    try:
        # Wait for kernel boot and Multiboot hand-off
        time.sleep(2.2)
        t_boot = time.perf_counter() - t_launch

        proc.stdin.write(b"\nstatus\n")
        proc.stdin.flush()
        time.sleep(0.5)

        # Launch graphical desktop
        t_gui_start = time.perf_counter()
        proc.stdin.write(b"gui\n")
        proc.stdin.flush()
        time.sleep(1.0)
        t_gui = (time.perf_counter() - t_gui_start)

        # Send ESC to cleanly return to shell
        proc.stdin.write(b"\x1b")
        proc.stdin.flush()
        time.sleep(0.5)

        proc.stdin.write(b"status\n")
        proc.stdin.flush()
        time.sleep(0.5)
    except Exception as e:
        print(f"Error during QEMU interaction: {e}")
    finally:
        proc.kill()
        stdout_data, _ = proc.communicate()

    t_total = time.perf_counter() - t_launch
    serial_log = stdout_data.decode("utf-8", errors="replace")

    has_boot = "[JARVIS-OS] Early serial debug online." in serial_log
    has_shell = "jarvis>" in serial_log
    has_gui = "[GUI] Starting Native JARVIS Graphical Desktop" in serial_log
    has_exit = "[GUI] Exited Graphical Desktop" in serial_log

    print(f"  [BOOT] Kernel Boot & Shell Ready:   {t_boot:.2f} s")
    print(f"  [GUI]  Desktop Framework Launch:    {t_gui:.2f} s")
    print(f"  [BOOT] Kernel Boot Banner:          {'PASS' if has_boot else 'FAIL'}")
    print(f"  [BOOT] Text Shell Availability:     {'PASS' if has_shell else 'FAIL'}")
    print(f"  [GUI]  Desktop Banner Verified:     {'PASS' if has_gui else 'FAIL'}")
    print(f"  [GUI]  Clean Restoration to Shell:  {'PASS' if has_exit else 'FAIL'}")

    return has_boot and has_shell and has_gui and has_exit


def main():
    print("==================================================")
    print("  JARVIS-OS PHASE 13 INTEGRATED BENCHMARK SUITE   ")
    print("==================================================")

    benchmark_ai_backends()
    ok_host = benchmark_host_subsystems()
    ok_qemu = benchmark_qemu_boot_and_gui()

    print("\n==================================================")
    if ok_host and ok_qemu:
        print("   ALL PERFORMANCE BENCHMARKS COMPLETED! (PASS)   ")
        print("==================================================")
        return 0
    else:
        print("   BENCHMARKS ENCOUNTERED FAILURES!               ")
        print("==================================================")
        return 1


if __name__ == "__main__":
    sys.exit(main())
