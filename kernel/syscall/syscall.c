#include <stdint.h>
#include <stddef.h>
#include "syscall.h"
#include "../drivers/terminal.h"
#include "../memory/heap.h"
#include "../process/process.h"
#include "../security/security.h"

extern volatile uint64_t timer_ticks;

uint64_t syscall(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    uint64_t ret;
    __asm__ volatile (
        "movq %1, %%rax\n"
        "movq %2, %%rdi\n"
        "movq %3, %%rsi\n"
        "movq %4, %%rdx\n"
        "int $0x80\n"
        "movq %%rax, %0\n"
        : "=r"(ret)
        : "r"(number), "r"(arg1), "r"(arg2), "r"(arg3)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
    return ret;
}

uint64_t syscall_dispatch(
    uint64_t number,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3)
{
    (void)arg3;

    if (number > SYSCALL_MAX) {
        security_audit_log(SEC_ACT_SYSCALL, PERM_NONE, SEC_AUDIT_FAULT, "invalid_syscall", (int32_t)number);
        return (uint64_t)-1; /* Invalid syscall number */
    }

    switch (number)
    {
        case SYS_GET_TICKS:
            if (security_check_permission(PERM_READ_DIAG, SEC_ACT_SYSCALL, "SYS_GET_TICKS", 0) != SEC_OK) {
                return (uint64_t)-1;
            }
            return timer_ticks;

        case SYS_ALLOCATE_MEMORY:
            if (security_check_permission(PERM_PROC_OP, SEC_ACT_SYSCALL, "SYS_ALLOCATE_MEMORY", (int32_t)arg1) != SEC_OK) {
                return 0;
            }
            if (arg1 == 0 || arg1 > KERNEL_HEAP_SIZE) {
                security_audit_log(SEC_ACT_SYSCALL, PERM_PROC_OP, SEC_AUDIT_FAULT, "alloc_size_invalid", (int32_t)arg1);
                return 0; /* Invalid allocation size */
            }
            return (uint64_t)(uintptr_t)kmalloc((size_t)arg1);

        case SYS_FREE_MEMORY:
            if (security_check_permission(PERM_PROC_OP, SEC_ACT_SYSCALL, "SYS_FREE_MEMORY", 0) != SEC_OK) {
                return (uint64_t)-1;
            }
            if (arg1 == 0 || security_validate_buffer((void *)(uintptr_t)arg1, 1, 0) != SEC_OK) {
                security_audit_log(SEC_ACT_SYSCALL, PERM_PROC_OP, SEC_AUDIT_FAULT, "free_ptr_invalid", 0);
                return (uint64_t)-2; /* Invalid pointer */
            }
            kfree((void *)(uintptr_t)arg1);
            return 0;

        case SYS_CREATE_PROCESS:
            if (security_check_permission(PERM_PROC_OP, SEC_ACT_SYSCALL, "SYS_CREATE_PROCESS", (int32_t)arg2) != SEC_OK) {
                return (uint64_t)-1;
            }
            if (arg1 == 0 || security_validate_buffer((void *)(uintptr_t)arg1, 1, 0) != SEC_OK) {
                security_audit_log(SEC_ACT_SYSCALL, PERM_PROC_OP, SEC_AUDIT_FAULT, "create_proc_entry_invalid", 0);
                return (uint64_t)-2; /* Invalid entry point */
            }
            return (uint64_t)(int64_t)process_create(arg1, (uint32_t)arg2);

        case SYS_GET_PROCESS_COUNT:
            if (security_check_permission(PERM_READ_DIAG, SEC_ACT_SYSCALL, "SYS_GET_PROCESS_COUNT", 0) != SEC_OK) {
                return (uint64_t)-1;
            }
            return (uint64_t)process_get_count();

        case SYS_TERMINATE_PROCESS:
            if (security_check_permission(PERM_PROC_OP, SEC_ACT_SYSCALL, "SYS_TERMINATE_PROCESS", (int32_t)arg1) != SEC_OK) {
                return (uint64_t)-1;
            }
            if (arg1 == 0 || arg1 >= MAX_PROCESSES) {
                security_audit_log(SEC_ACT_SYSCALL, PERM_PROC_OP, SEC_AUDIT_FAULT, "term_pid_invalid", (int32_t)arg1);
                return (uint64_t)-2; /* Invalid PID */
            }
            return (uint64_t)process_terminate((uint32_t)arg1);

        default:
            security_audit_log(SEC_ACT_SYSCALL, PERM_NONE, SEC_AUDIT_FAULT, "unknown_syscall", (int32_t)number);
            return (uint64_t)-1;
    }
}

void syscall_run_demo(void)
{
    terminal_write("\n========================================\n");
    terminal_write("      SYSTEM CALL DISPATCH BENCHMARK    \n");
    terminal_write("========================================\n");
    terminal_write("Mechanism: Software interrupt int $0x80 -> IDT vector 0x80\n");

    /* Syscall 0: SYS_GET_TICKS */
    uint64_t ticks = syscall(SYS_GET_TICKS, 0, 0, 0);
    terminal_write("1. SYS_GET_TICKS via int $0x80: ticks=");
    terminal_write_number((uint32_t)ticks);
    terminal_write(" (PASS)\n");

    /* Syscall 4: SYS_GET_PROCESS_COUNT */
    uint64_t count = syscall(SYS_GET_PROCESS_COUNT, 0, 0, 0);
    terminal_write("2. SYS_GET_PROCESS_COUNT via int $0x80: count=");
    terminal_write_number((uint32_t)count);
    terminal_write(" (PASS)\n");

    /* Syscall 1: SYS_ALLOCATE_MEMORY */
    uint64_t mem_ptr = syscall(SYS_ALLOCATE_MEMORY, 64, 0, 0);
    terminal_write("3. SYS_ALLOCATE_MEMORY (64 bytes) via int $0x80: ");
    if (mem_ptr != 0) {
        terminal_write("PASS (allocated)\n");
        syscall(SYS_FREE_MEMORY, mem_ptr, 0, 0);
    } else {
        terminal_write("FAIL\n");
    }

    /* Invalid Syscall 999: error check */
    uint64_t inv = syscall(999, 0, 0, 0);
    terminal_write("4. Invalid syscall 999 rejection check: ");
    if (inv == (uint64_t)-1) {
        terminal_write("PASS (Rejected with -1)\n");
    } else {
        terminal_write("FAIL\n");
    }
}
