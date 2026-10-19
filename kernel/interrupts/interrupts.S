
.code64

.global interrupt_stub
.global timer_stub
.global schedule_stub

.extern interrupt_handler
.extern scheduler_interrupt

.macro SAVE_REGISTERS
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
.endm

.macro RESTORE_REGISTERS
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
.endm

interrupt_stub:
    cld
    SAVE_REGISTERS

    movq %rsp, %r12
    andq $-16, %rsp
    call interrupt_handler
    movq %r12, %rsp

    RESTORE_REGISTERS
    iretq

timer_stub:
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

.global keyboard_stub
.extern keyboard_handler

keyboard_stub:
    cld
    SAVE_REGISTERS
    movq %rsp, %r12
    andq $-16, %rsp
    call keyboard_handler
    movq %r12, %rsp
    RESTORE_REGISTERS
    iretq

.global mouse_stub
.extern mouse_handler

mouse_stub:
    cld
    SAVE_REGISTERS
    movq %rsp, %r12
    andq $-16, %rsp
    call mouse_handler
    movq %r12, %rsp
    RESTORE_REGISTERS
    iretq

.global syscall_stub
.extern syscall_dispatch

syscall_stub:
    cld

    SAVE_REGISTERS

    movq 112(%rsp), %rdi    # syscall number (saved rax)
    movq 64(%rsp), %rsi     # arg1 (saved rdi)
    movq 72(%rsp), %rdx     # arg2 (saved rsi)
    movq 88(%rsp), %rcx     # arg3 (saved rdx)

    movq %rsp, %r12
    andq $-16, %rsp
    call syscall_dispatch
    movq %r12, %rsp

    movq %rax, 112(%rsp)    # overwrite saved rax with return value

    RESTORE_REGISTERS
    iretq

.extern c_exception_handler

.macro EXCEPTION_NO_ERR vec
.global isr_exc_\vec
isr_exc_\vec:
    cld
    pushq $0
    pushq $\vec
    jmp common_exception_entry
.endm

.macro EXCEPTION_ERR vec
.global isr_exc_\vec
isr_exc_\vec:
    cld
    pushq $\vec
    jmp common_exception_entry
.endm

EXCEPTION_NO_ERR 0   # #DE Divide Error
EXCEPTION_NO_ERR 1   # #DB Debug
EXCEPTION_NO_ERR 2   # NMI
EXCEPTION_NO_ERR 3   # #BP Breakpoint
EXCEPTION_NO_ERR 4   # #OF Overflow
EXCEPTION_NO_ERR 5   # #BR Bound Range Exceeded
EXCEPTION_NO_ERR 6   # #UD Invalid Opcode
EXCEPTION_NO_ERR 7   # #NM Device Not Available
EXCEPTION_ERR    8   # #DF Double Fault
EXCEPTION_NO_ERR 9   # Coprocessor Segment Overrun
EXCEPTION_ERR    10  # #TS Invalid TSS
EXCEPTION_ERR    11  # #NP Segment Not Present
EXCEPTION_ERR    12  # #SS Stack Fault
EXCEPTION_ERR    13  # #GP General Protection
EXCEPTION_ERR    14  # #PF Page Fault
EXCEPTION_NO_ERR 15  # Reserved
EXCEPTION_NO_ERR 16  # #MF x87 FPU Floating-Point
EXCEPTION_ERR    17  # #AC Alignment Check
EXCEPTION_NO_ERR 18  # #MC Machine Check
EXCEPTION_NO_ERR 19  # #XM SIMD Floating-Point
EXCEPTION_NO_ERR 20  # #VE Virtualization
EXCEPTION_ERR    21  # #CP Control Protection
EXCEPTION_NO_ERR 22
EXCEPTION_NO_ERR 23
EXCEPTION_NO_ERR 24
EXCEPTION_NO_ERR 25
EXCEPTION_NO_ERR 26
EXCEPTION_NO_ERR 27
EXCEPTION_NO_ERR 28
EXCEPTION_NO_ERR 29
EXCEPTION_NO_ERR 30
EXCEPTION_NO_ERR 31

common_exception_entry:
    SAVE_REGISTERS
    movq %rsp, %rdi         # 1st arg: exception_frame_t *
    movq %cr2, %rsi         # 2nd arg: cr2 (fault address)
    movq %rsp, %r12
    andq $-16, %rsp         # 16-byte stack alignment
    call c_exception_handler
    movq %r12, %rsp
    RESTORE_REGISTERS
    addq $16, %rsp          # pop vector and error_code
    iretq

.global generic_exception_stub
generic_exception_stub:
    jmp isr_exc_13

.global page_fault_stub
page_fault_stub:
    jmp isr_exc_14
