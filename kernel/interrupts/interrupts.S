.code64

.global interrupt_stub
.global timer_stub

.extern interrupt_handler
.extern timer_handler

interrupt_stub:
    cld
    call interrupt_handler
    iretq

timer_stub:
    cld
    call timer_handler
    iretq
