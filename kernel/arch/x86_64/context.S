.code64
.text

.global context_switch
.type context_switch, @function

/*
 * void context_switch(uint64_t *old_rsp, uint64_t new_rsp)
 *
 * Save callee-saved registers on the current stack,
 * switch stacks, restore the next task's registers,
 * and return into its saved execution point.
 */
context_switch:
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, (%rdi)
    movq %rsi, %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    ret

.size context_switch, .-context_switch
