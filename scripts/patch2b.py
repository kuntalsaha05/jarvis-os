from pathlib import Path

p = Path("kernel/interrupts/idt.c")
s = p.read_text()

old = """    idt_set_gate(
        32,
        (uint64_t)timer_stub,
        0x08,
        0x8E
    );"""

new = old + """

    idt_set_gate(
        0x81,
        (uint64_t)schedule_stub,
        0x08,
        0x8E
    );"""

assert s.count(old) == 1, "Timer IDT gate not found uniquely; no changes made."
p.write_text(s.replace(old, new, 1))
print("IDT vector 0x81 registered.")
