
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

.global generic_exception_stub
.extern generic_exception_handler

generic_exception_stub:
    cld
    SAVE_REGISTERS
    movq %rsp, %rdi
    andq $-16, %rsp
    call generic_exception_handler
    RESTORE_REGISTERS
    iretq

.global page_fault_stub
.extern page_fault_handler

page_fault_stub:
    cld
    SAVE_REGISTERS

    # Preserve the original saved-register stack pointer.
    movq %rsp, %r12

    # Arguments:
    # rdi = faulting virtual address (CR2)
    # rsi = CPU-generated page-fault error code
    movq %cr2, %rdi
    movq 120(%rsp), %rsi

    andq $-16, %rsp
    call page_fault_handler

    movq %r12, %rsp

    RESTORE_REGISTERS

    # Remove the CPU-generated error code before IRETQ.
    addq $8, %rsp
    iretq
