from pathlib import Path
p = Path("kernel/interrupts/idt.c")
s = p.read_text()
old = "extern void timer_stub(void);"
new = old + "\nextern void schedule_stub(void);"
assert s.count(old) == 1, "Unexpected IDT source; aborting safely."
p.write_text(s.replace(old, new, 1))
print("IDT declaration added.")
