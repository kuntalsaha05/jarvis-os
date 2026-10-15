from pathlib import Path
import shutil
import sys

root = Path(".")
backup = root / "backups/preemption-stage4-rollback"
files = [
    Path("kernel/process/process.c"),
    Path("kernel/process/scheduler.c"),
    Path("kernel/interrupts/interrupts.S"),
    Path("kernel/interrupts/idt.c"),
]
backup.mkdir(parents=True, exist_ok=True)

for p in files:
    if not p.is_file():
        raise SystemExit(f"Missing required file: {p}")
    shutil.copy2(p, backup / p.name)

def replace_once(path, old, new):
    p = root / path
    s = p.read_text()
    n = s.count(old)
    if n != 1:
        raise RuntimeError(f"{path}: expected exactly one match, found {n}")
    p.write_text(s.replace(old, new, 1))

try:
    # Give each process an interrupt-compatible initial frame.
    replace_once(
        "kernel/process/process.c",
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

        /* 15 saved GPRs followed by RIP, CS, and RFLAGS. */
        uint64_t *frame = (uint64_t *)(top - 18 * sizeof(uint64_t));
        for (uint32_t j = 0; j < 18; j++)
            frame[j] = 0;

        frame[15] = (uint64_t)(uintptr_t)process_task_trampoline;
        frame[16] = 0x08;
        frame[17] = 0x202;

        p->interrupt_frame = frame;
        p->saved_rsp = (uint64_t)(uintptr_t)frame;
""",
    )

    # Replace the cooperative scheduler with a unified frame dispatcher.
    Path("kernel/process/scheduler.c").write_text(r'''#include "scheduler.h"
#include <stddef.h>
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
    current_process = NULL;
    bootstrap_frame = NULL;
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
    process_t *selected = NULL;

    if (current_algorithm == SCHEDULER_RR) {
        for (uint32_t offset = 1; offset <= MAX_PROCESSES; offset++) {
            uint32_t i = (rr_cursor + offset) % MAX_PROCESSES;
            process_t *p = process_get_by_index(i);
            if (p != NULL && p->state == PROCESS_READY) {
                rr_cursor = i;
                return p;
            }
        }
        return NULL;
    }

    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        process_t *p = process_get_by_index(i);
        if (p == NULL || p->state != PROCESS_READY)
            continue;

        if (selected == NULL ||
            (current_algorithm == SCHEDULER_SJF &&
             p->cpu_time < selected->cpu_time) ||
            (current_algorithm == SCHEDULER_FCFS &&
             p->creation_tick < selected->creation_tick)) {
            selected = p;
        }
    }
    return selected;
}

process_t *scheduler_current_process(void)
{
    return current_process;
}

/*
 * Called by the assembly stubs with a pointer to the saved interrupt frame.
 * Returns the frame that the assembly stub must restore before IRETQ.
 */
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
        if (first == NULL)
            return frame;

        scheduler_started = 1;
        current_process = first;
        first->state = PROCESS_RUNNING;
        quantum_ticks = 0;
        return first->interrupt_frame;
    }

    process_t *previous = current_process;
    if (previous == NULL)
        return bootstrap_frame != NULL ? bootstrap_frame : frame;

    if (from_timer && previous->state == PROCESS_RUNNING) {
        previous->cpu_time++;

        if (current_algorithm != SCHEDULER_RR)
            return frame;

        quantum_ticks++;
        if (quantum_ticks < SCHEDULER_RR_QUANTUM)
            return frame;
    }

    /* Save the interrupted process unless it has already terminated. */
    if (previous->state != PROCESS_TERMINATED) {
        previous->interrupt_frame = frame;
        previous->saved_rsp = (uint64_t)(uintptr_t)frame;

        if (previous->state == PROCESS_RUNNING)
            previous->state = PROCESS_READY;
    }

    process_t *next = scheduler_select_next();

    if (next == NULL) {
        if (previous->state == PROCESS_READY) {
            previous->state = PROCESS_RUNNING;
            quantum_ticks = 0;
            return frame;
        }

        current_process = NULL;
        scheduler_started = 0;
        quantum_ticks = 0;
        return bootstrap_frame != NULL ? bootstrap_frame : frame;
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
    if (scheduler_started && current_process != NULL)
        __asm__ volatile ("int $0x81" ::: "memory");
}

void scheduler_tick(void)
{
    if (current_process != NULL &&
        current_process->state == PROCESS_RUNNING)
        current_process->cpu_time++;
}

void process_task_trampoline(void)
{
    process_t *p = scheduler_current_process();

    if (p != NULL) {
        void (*entry)(void) = (void (*)(void))(uintptr_t)p->entry_point;
        entry();
        process_terminate(p->pid);
    }

    scheduler_yield();

    for (;;)
        __asm__ volatile ("cli; hlt");
}
''')

    # Route both timer and software scheduling interrupts to the dispatcher.
    replace_once(
        "kernel/interrupts/interrupts.S",
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

    replace_once(
        "kernel/interrupts/interrupts.S",
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

    # Register the software scheduling interrupt.
    replace_once(
        "kernel/interrupts/idt.c",
        "extern void timer_stub(void);\n",
        "extern void timer_stub(void);\nextern void schedule_stub(void);\n",
    )

    replace_once(
        "kernel/interrupts/idt.c",
        """    idt_set_gate(
    \\t32,
    \\t(uint64_t)timer_stub,
    \\t0x08,
    \\t0x8E
    );

""",
        """    idt_set_gate(
    \\t32,
    \\t(uint64_t)timer_stub,
    \\t0x08,
    \\t0x8E
    );

    idt_set_gate(
        0x81,
        (uint64_t)schedule_stub,
        0x08,
        0x8E
    );

""",
    )

    # The timer dispatcher now sends EOI and updates ticks itself.
    replace_once(
        "kernel/interrupts/idt.c",
        """void timer_handler(void)
{
    timer_ticks++;

    scheduler_tick();

    pic_send_eoi(0);
}

""",
        "",
    )

except Exception as exc:
    print(f"PATCH FAILED: {exc}", file=sys.stderr)
    for p in files:
        shutil.copy2(backup / p.name, p)
    print("Original source files restored from stage-4 rollback backup.")
    sys.exit(1)

print("Stage 4 patch applied.")
print(f"Rollback copies: {backup}")
print("Next: run diff checks and a clean build before launching QEMU.")
