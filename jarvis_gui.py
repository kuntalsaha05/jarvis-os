#!/usr/bin/env python3
"""
JARVIS-OS Modern Web GUI Launcher.
Starts the FastAPI backend and automatically launches the HUD web dashboard in the default browser.
"""

import sys
import os
import time
import threading
import webbrowser
import argparse
import uvicorn

# Ensure repository root is on sys.path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def open_browser_delayed(url: str, delay: float = 1.2):
    def _open():
        time.sleep(delay)
        print(f"[JARVIS-OS GUI] Opening browser at {url} ...")
        webbrowser.open(url)
    threading.Thread(target=_open, daemon=True).start()


def main():
    parser = argparse.ArgumentParser(description="JARVIS-OS Modern Web GUI & Telemetry Dashboard")
    parser.add_argument("--host", type=str, default="127.0.0.1", help="Host interface to bind to (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=8000, help="Port to listen on (default: 8000)")
    parser.add_argument("--no-browser", action="store_true", help="Do not automatically launch web browser")
    parser.add_argument("--reload", action="store_true", help="Enable automatic code reloading")

    args = parser.parse_args()

    url = f"http://{args.host}:{args.port}"

    print("==================================================")
    print("         JARVIS-OS MODERN WEB DASHBOARD           ")
    print("==================================================")
    print(f" URL:            {url}")
    print(f" Architecture:   x86-64 Long Mode (Simulated Kernel)")
    print(f" Telemetry:      PIT Timer, Paging, Processes, IRQ")
    print(f" Storage:        JARVIS-FS RAM Disk (64 KiB)")
    print(f" AI Companion:   Allowlisted Tools & Audit Logger")
    print(f" Visualizers:    CPU Gantt, Belady, Banker, Disk, Fit")
    print("==================================================")

    if not args.no_browser:
        open_browser_delayed(url)

    from jarvis.gui.server import app
    uvicorn.run(app, host=args.host, port=args.port, reload=args.reload)


if __name__ == "__main__":
    main()

