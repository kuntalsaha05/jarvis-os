
.code64

.global interrupt_stub
.global timer_stub

.extern interrupt_handler
.extern timer_handler

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

    movq %rsp, %r12
    andq $-16, %rsp
    call timer_handler
    movq %r12, %rsp

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
