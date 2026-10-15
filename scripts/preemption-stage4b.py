from pathlib import Path
import shutil
import sys

ROOT = Path(".")
BACKUP = ROOT / "backups/preemption-stage4b-rollback"

FILES = [
    Path("kernel/process/process.c"),
    Path("kernel/process/scheduler.c"),
    Path("kernel/interrupts/interrupts.S"),
    Path("kernel/interrupts/idt.c"),
]

# Read all files first. Nothing is written until every precondition passes.
original = {}
for path in FILES:
    if not path.is_file():
        raise SystemExit(f"Missing source file: {path}")
    original[path] = path.read_text()

checks = [
    ("process stack",
     Path("kernel/process/process.c"),
     "/* Initial stack layout consumed by context_switch(). */"),
    ("assembly timer declaration",
     Path("kernel/interrupts/interrupts.S"),
     ".extern timer_handler"),
    ("assembly timer call",
     Path("kernel/interrupts/interrupts.S"),
     "call timer_handler"),
    ("timer handler",
     Path("kernel/interrupts/idt.c"),
     "void timer_handler(void)"),
    ("timer IDT gate",
     Path("kernel/interrupts/idt.c"),
     "(uint64_t)timer_stub"),
]

for label, path, marker in checks:
    count = original[path].count(marker)
    if count != 1:
        raise SystemExit(
            f"Preflight failed: {label} in {path}: {count} matches. "
            "No source files changed."
        )

# Confirm the current process metadata is the expected checkpoint.
header = ROOT / "kernel/process/process.h"
if "uint64_t *interrupt_frame;" not in header.read_text():
    raise SystemExit("interrupt_frame field missing; no source files changed.")

print("Stage 4B preflight passed.")
print("All expected markers are unique.")
print("No kernel source files were modified.")
print("Next step: generate and validate the coordinated dispatcher patch.")
