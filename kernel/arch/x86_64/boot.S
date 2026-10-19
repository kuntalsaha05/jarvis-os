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

    # Initialize 4 Page Directories (2048 entries * 2 MiB = 4 GiB identity-mapped)
    mov $pd_tables, %edi
    mov $0x83, %ebx          # Start at physical 0x00000000, Present + Writable + 2MB
    mov $2048, %ecx
.Lmap_loop:
    mov %ebx, (%edi)
    movl $0, 4(%edi)
    add $0x200000, %ebx
    add $8, %edi
    dec %ecx
    jnz .Lmap_loop

    # Link PDPT entries 0..3 to the 4 Page Directories
    mov $pdpt_table, %edi
    mov $pd_tables, %ebx
    or $0x3, %ebx
    mov %ebx, 0(%edi)
    movl $0, 4(%edi)

    add $4096, %ebx
    mov %ebx, 8(%edi)
    movl $0, 12(%edi)

    add $4096, %ebx
    mov %ebx, 16(%edi)
    movl $0, 20(%edi)

    add $4096, %ebx
    mov %ebx, 24(%edi)
    movl $0, 28(%edi)

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
.skip 65536

stack_top:

.align 4096

pml4_table:
.skip 4096

.align 4096

pdpt_table:
.skip 4096

.align 4096

pd_tables:
.skip 4096 * 4

.align 4
.global multiboot_magic
.global multiboot_info_ptr
multiboot_magic:
.long 0

multiboot_info_ptr:
.long 0
