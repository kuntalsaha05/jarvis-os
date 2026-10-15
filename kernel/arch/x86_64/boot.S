.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .text
.code32

.global _start
.type _start, @function

_start:
    cli
    movl %eax, multiboot_magic
    movl %ebx, multiboot_info_ptr

    # PML4[0] -> PDPT
    mov $pml4_table, %eax
    mov $pdpt_table, %ebx
    or $0x3, %ebx
    mov %ebx, (%eax)

    # PDPT[0] -> Page Directory
    mov $pdpt_table, %eax
    mov $pd_table, %ebx
    or $0x3, %ebx
    mov %ebx, (%eax)

    # PD[0] -> 2 MiB identity-mapped page
    # Present + Writable + Huge Page
    mov $pd_table, %eax
    mov $0x83, %ebx
    mov %ebx, (%eax)

    # Load PML4
    mov $pml4_table, %eax
    mov %eax, %cr3

    # Enable PAE
    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4

    # Enable Long Mode through EFER
    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr

    # Enable paging
    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0

    # Load our 64-bit GDT
    lgdt gdt_descriptor

    # Enter 64-bit mode
    ljmp $0x08, $long_mode_start

.code64

long_mode_start:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss

    mov $stack_top, %rsp

    movl multiboot_magic(%rip), %edi
    movl multiboot_info_ptr(%rip), %esi

    call kernel_main

hang:
    cli
    hlt
    jmp hang

.section .rodata

.align 8

gdt:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF
    .quad 0x00CF92000000FFFF

gdt_descriptor:
    .word gdt_descriptor - gdt - 1
    .long gdt

.section .bss

.align 16

stack_bottom:
.skip 16384

stack_top:

.align 4096

pml4_table:
.skip 4096

.align 4096

pdpt_table:
.skip 4096

.align 4096

pd_table:
.skip 4096

.align 4
.global multiboot_magic
.global multiboot_info_ptr
multiboot_magic:
.long 0

multiboot_info_ptr:
.long 0
