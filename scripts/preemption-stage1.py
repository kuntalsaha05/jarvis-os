from pathlib import Path

p = Path("kernel/process/process.h")
s = p.read_text()

old = "    uint64_t saved_rsp;\n} process_t;"
new = (
    "    uint64_t saved_rsp;\n"
    "    uint64_t *interrupt_frame;\n"
    "} process_t;"
)

if "uint64_t *interrupt_frame;" in s:
    print("Interrupt-frame field already exists; no change needed.")
else:
    if s.count(old) != 1:
        raise SystemExit("Unexpected process_t layout; no changes made.")
    p.write_text(s.replace(old, new, 1))
    print("Stage 1 applied: process interrupt-frame storage added.")
