#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct {
    uint64_t fault_address;
    uint64_t error_code;
    int is_present;          /* bit 0: 0 = not present, 1 = protection violation */
    int is_write;            /* bit 1: 0 = read, 1 = write */
    int is_user;             /* bit 2: 0 = supervisor, 1 = user */
    int is_rsvd_violation;   /* bit 3: 1 = reserved bits violated */
    int is_instruction_fetch;/* bit 4: 1 = instruction fetch */
    int is_mapped_region;    /* 1 = within identity mapped 2 MiB (0x0 - 0x1FFFFF), 0 = unmapped */
} page_fault_info_t;

typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) exception_frame_t;

void idt_init(void);
void c_exception_handler(exception_frame_t *frame, uint64_t cr2);
const char *exception_name(uint64_t vector);
void page_fault_decode(uint64_t address, uint64_t error_code, page_fault_info_t *info);
void page_fault_diagnose(uint64_t address, uint64_t error_code);
int test_page_fault_diagnostics(void);
int test_exception_invariants(void);

#endif
