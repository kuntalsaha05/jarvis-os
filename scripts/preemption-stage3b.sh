#!/usr/bin/env bash
set -eu

{
    echo "===== interrupts.S ====="
    sed -n '1,150p' kernel/interrupts/interrupts.S

    echo "===== idt.c ====="
    sed -n '1,180p' kernel/interrupts/idt.c

    echo "===== idt.h ====="
    sed -n '1,160p' kernel/interrupts/idt.h

    echo "===== PIC interface ====="
    sed -n '1,140p' kernel/interrupts/pic.h

    echo "===== Makefile ====="
    sed -n '1,180p' Makefile
} > preemption-stage3b-inspection.txt

cat preemption-stage3b-inspection.txt
