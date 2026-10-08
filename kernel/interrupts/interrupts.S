.code64

.global interrupt_stub

.extern interrupt_handler

interrupt_stub:
    cld
    call interrupt_handler

    iretq
