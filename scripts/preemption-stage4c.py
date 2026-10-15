from pathlib import Path
import re
import shutil
import sys

paths = [
    Path("kernel/process/process.c"),
    Path("kernel/process/scheduler.c"),
    Path("kernel/interrupts/interrupts.S"),
    Path("kernel/interrupts/idt.c"),
]
backup = Path("backups/preemption-stage4c-rollback")

original = {p: p.read_text() for p in paths}
updated = dict(original)

def replace_exact(path, old, new):
    count = updated[path].count(old)
    if count != 1:
        raise ValueError(f"{path}: expected one exact match, got {count}")
    updated[path] = updated[path].replace(old, new, 1)

def replace_regex(path, pattern, new, label):
    result, count = re.subn(pattern, new, updated[path], count=1, flags=re.S)
    if count != 1:
        raise ValueError(f"{path}: {label}: expected one match, got {count}")
    updated[path] = result

try:
    # Initial task stack must match SAVE_REGISTERS + IRETQ.
    replace_exact(
        paths[0],
        """        uintptr_t top =
            (uintptr_t)&process_stacks[i][PROCESS_STACK_SIZE];
        top &= ~(uintptr_t)0xF;

        uint64_t *sp = (uint64_t *)top;

        /* Initial stack layout consumed by context_switch(). */
        *--sp = (uint64_t)process_task_trampoline;
        *--sp = 0; /* rbx */
        *--sp = 0; /* rbp */
        *--sp = 0; /* r12 */
        *--sp = 0; /* r13 */
        *--sp = 0; /* r14 */
        *--sp = 0; /* r15 */

        p->saved_rsp = (uint64_t)(uintptr_t)sp;
""",
        """        uintptr_t top =
            (uintptr_t)&process_stacks[i][PROCESS_STACK_SIZE];
        top = (top & ~(uintptr_t)0xF) - 8;

        /* 15 GPRs, then RIP, CS, and RFLAGS. */
        uint64_t *frame =
            (uint64_t *)(top - 18 * sizeof(uint64_t));
        for (uint32_t j = 0; j < 18; j++)
            frame[j] = 0;

        frame[15] = (uint64_t)(uintptr_t)process_task_trampoline;
        frame[16] = 0x08;
        frame[17] = 0x202;

        p->interrupt_frame = frame;
        p->saved_rsp = (uint64_t)(uintptr_t)frame;
""",
    )

    # Unified interrupt-frame scheduler.
    updated[paths[1]] = r'''#include "scheduler.h"
#include <stdint.h>
#include "../interrupts/pic.h"

extern volatile uint64_t timer_ticks;

static scheduler_algorithm_t current_algorithm = SCHEDULER_FCFS;
static process_t *current_process;
static uint64_t *bootstrap_frame;
static int scheduler_started;
static uint32_t rr_cursor;
static uint32_t quantum_ticks;

void scheduler_init(void)
{
    current_algorithm = SCHEDULER_FCFS;
    current_process = 0;
    bootstrap_frame = 0;
    scheduler_started = 0;
    rr_cursor = MAX_PROCESSES - 1;
    quantum_ticks = 0;
}

void scheduler_set_algorithm(scheduler_algorithm_t algorithm)
{
    if (algorithm >= SCHEDULER_FCFS && algorithm <= SCHEDULER_RR)
        current_algorithm = algorithm;
}

scheduler_algorithm_t scheduler_get_algorithm(void)
{
    return current_algorithm;
}

process_t *scheduler_select_next(void)
{
    process_t *selected = 0;

    if (current_algorithm == SCHEDULER_RR) {
        for (uint32_t offset = 1; offset <= MAX_PROCESSES; offset++) {
            uint32_t i = (rr_cursor + offset) % MAX_PROCESSES;
            process_t *p = process_get_by_index(i);
            if (p != 0 && p->state == PROCESS_READY) {
                rr_cursor = i;
                return p;
            }
        }
        return 0;
    }

    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        process_t *p = process_get_by_index(i);
        if (p == 0 || p->state != PROCESS_READY)
            continue;

        if (selected == 0 ||
            (current_algorithm == SCHEDULER_SJF &&
             p->cpu_time < selected->cpu_time) ||
            (current_algorithm == SCHEDULER_FCFS &&
             p->creation_tick < selected->creation_tick))
            selected = p;
    }
    return selected;
}

process_t *scheduler_current_process(void)
{
    return current_process;
}

/* Return the saved frame that the assembly stub must restore with IRETQ. */
uint64_t *scheduler_interrupt(uint64_t *frame, uint64_t from_timer)
{
    if (from_timer) {
        timer_ticks++;
        pic_send_eoi(0);
    }

    if (!scheduler_started) {
        if (from_timer)
            return frame;

        bootstrap_frame = frame;
        process_t *first = scheduler_select_next();
        if (first == 0)
            return frame;

        scheduler_started = 1;
        current_process = first;
        first->state = PROCESS_RUNNING;
        quantum_ticks = 0;
        return first->interrupt_frame;
    }

    process_t *previous = current_process;
    if (previous == 0)
        return bootstrap_frame != 0 ? bootstrap_frame : frame;

    if (from_timer && previous->state == PROCESS_RUNNING) {
        previous->cpu_time++;

        if (current_algorithm != SCHEDULER_RR)
            return frame;

        if (++quantum_ticks < SCHEDULER_RR_QUANTUM)
            return frame;
    }

    if (previous->state != PROCESS_TERMINATED) {
        previous->interrupt_frame = frame;
        previous->saved_rsp = (uint64_t)(uintptr_t)frame;
        if (previous->state == PROCESS_RUNNING)
            previous->state = PROCESS_READY;
    }

    process_t *next = scheduler_select_next();

    if (next == 0) {
        if (previous->state == PROCESS_READY) {
            previous->state = PROCESS_RUNNING;
            quantum_ticks = 0;
            return frame;
        }

        current_process = 0;
        scheduler_started = 0;
        quantum_ticks = 0;
        return bootstrap_frame != 0 ? bootstrap_frame : frame;
    }

    if (next == previous) {
        previous->state = PROCESS_RUNNING;
        quantum_ticks = 0;
        return frame;
    }

    current_process = next;
    next->state = PROCESS_RUNNING;
    quantum_ticks = 0;
    return next->interrupt_frame;
}

void scheduler_start(void)
{
    if (!scheduler_started)
        __asm__ volatile ("int $0x81" ::: "memory");
}

void scheduler_yield(void)
{
    if (scheduler_started && current_process != 0)
        __asm__ volatile ("int $0x81" ::: "memory");
}

void scheduler_tick(void)
{
    if (current_process != 0 &&
        current_process->state == PROCESS_RUNNING)
        current_process->cpu_time++;
}

void process_task_trampoline(void)
{
    process_t *p = scheduler_current_process();
    if (p != 0) {
        void (*entry)(void) = (void (*)(void))(uintptr_t)p->entry_point;
        entry();
        process_terminate(p->pid);
    }

    scheduler_yield();
    for (;;)
        __asm__ volatile ("cli; hlt");
}
'''

    # Timer and software interrupt share the same saved-frame format.
    replace_exact(
        paths[2],
        """.global interrupt_stub
.global timer_stub

.extern interrupt_handler
.extern timer_handler
""",
        """.global interrupt_stub
.global timer_stub
.global schedule_stub

.extern interrupt_handler
.extern scheduler_interrupt
""",
    )

    replace_exact(
        paths[2],
        """timer_stub:
    cld
    SAVE_REGISTERS

    movq %rsp, %r12
    andq $-16, %rsp
    call timer_handler
    movq %r12, %rsp

    RESTORE_REGISTERS
    iretq
""",
        """timer_stub:
    cld
    SAVE_REGISTERS
    movq %rsp, %rdi
    movq $1, %rsi
    andq $-16, %rsp
    call scheduler_interrupt
    movq %rax, %rsp
    RESTORE_REGISTERS
    iretq

schedule_stub:
    cld
    SAVE_REGISTERS
    movq %rsp, %rdi
    xorq %rsi, %rsi
    andq $-16, %rsp
    call scheduler_interrupt
    movq %rax, %rsp
    RESTORE_REGISTERS
    iretq
""",
    )

    replace_exact(
        paths[3],
        "extern void timer_stub(void);\n",
        "extern void timer_stub(void);\nextern void schedule_stub(void);\n",
    )

    # Match the gate independent of spaces or tabs.
    replace_regex(
        paths[3],
        r"    idt_set_gate\(\s*32,\s*\(uint64_t\)timer_stub,\s*0x08,\s*0x8E\s*\);",
        """    idt_set_gate(
        32,
        (uint64_t)timer_stub,
        0x08,
        0x8E
    );

    idt_set_gate(
        0x81,
        (uint64_t)schedule_stub,
        0x08,
        0x8E
    );""",
        "timer IDT gate",
    )

    replace_regex(
        paths[3],
        r"void timer_handler\(void\)\s*\{\s*timer_ticks\+\+;\s*scheduler_tick\(\);\s*pic_send_eoi\(0\);\s*\}\s*",
        "",
        "old timer handler",
    )

    # Validate transformed files before writing any of them.
    if "call scheduler_interrupt" not in updated[paths[2]]:
        raise ValueError("Dispatcher call missing from assembly")
    if "0x81" not in updated[paths[3]]:
        raise ValueError("Software interrupt gate missing from IDT")
    if "frame[17] = 0x202;" not in updated[paths[0]]:
        raise ValueError("Initial RFLAGS missing")
    if "uint64_t *scheduler_interrupt(uint64_t *frame, uint64_t from_timer)" not in updated[paths[1]]:
        raise ValueError("Dispatcher function missing")

except Exception as exc:
    print(f"Preflight failed: {exc}", file=sys.stderr)
    print("No kernel source files were written.", file=sys.stderr)
    sys.exit(1)

# Save rollback copies, then write the fully validated set.
backup.mkdir(parents=True, exist_ok=True)
for path in paths:
    shutil.copy2(path, backup / path.name)

written = []
try:
    for path in paths:
        path.write_text(updated[path])
        written.append(path)
except Exception as exc:
    for path in paths:
        shutil.copy2(backup / path.name, path)
    print(f"Write failed: {exc}", file=sys.stderr)
    print("All source files restored from rollback copies.", file=sys.stderr)
    sys.exit(1)

print("Stage 4C patch applied and validated.")
print(f"Rollback copies: {backup}")
print("Do not run QEMU until the clean build succeeds.")
