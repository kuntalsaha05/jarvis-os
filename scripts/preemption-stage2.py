from pathlib import Path

p = Path("kernel/process/process.c")
s = p.read_text()

changes = [
    (
        "        p->context = (cpu_context_t){0};\n",
        "        p->context = (cpu_context_t){0};\n"
        "        p->interrupt_frame = NULL;\n"
    ),
    (
        "    p->saved_rsp = 0;\n",
        "    p->saved_rsp = 0;\n"
        "    p->interrupt_frame = NULL;\n"
    ),
]

for old, new in changes:
    count = s.count(old)
    if count != 1:
        raise SystemExit(
            f"Expected one match in process.c, found {count}; no further changes made."
        )
    s = s.replace(old, new, 1)

p.write_text(s)
print("Stage 2 applied: interrupt-frame pointers initialized.")
