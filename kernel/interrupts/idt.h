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

void idt_init(void);
void page_fault_decode(uint64_t address, uint64_t error_code, page_fault_info_t *info);
void page_fault_diagnose(uint64_t address, uint64_t error_code);
int test_page_fault_diagnostics(void);

#endif
