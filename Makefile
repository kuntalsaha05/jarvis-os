
TARGET = x86_64-elf
CC = $(TARGET)-gcc
LD = $(TARGET)-ld

CFLAGS = -ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -O2 -Wall -Wextra
LDFLAGS = -T kernel/linker.ld

KERNEL = build/jarvis.bin
ISO = build/jarvis.iso
ISO_DIR = build/isodir

OBJECTS = \
	build/boot.o \
	build/context.o \
	build/kernel.o \
	build/idt.o \
	build/interrupts.o \
	build/pic.o \
	build/pit.o \
	build/process.o \
	build/scheduler.o \
	build/heap.o \
	build/page.o \
	build/syscall.o \
	build/ipc.o \
	build/semaphore.o \
	build/sync_demo.o \
	build/block.o \
	build/ramdisk.o \
	build/fs.o \
	build/sched_sim.o \
	build/page_replace.o \
	build/banker.o \
	build/deadlock.o \
	build/mem_fit.o \
	build/file_alloc.o \
	build/disk_sched.o \
	build/debug.o \
	build/multiboot.o \
	build/terminal.o \
	build/keyboard.o \
	build/shell.o \
	build/framebuffer.o \
	build/mouse.o \
	build/font.o \
	build/graphics.o \
	build/event.o \
	build/wm.o \
	build/desktop.o

.PHONY: all clean iso run run-headless test test-ai test-all

all: $(KERNEL)

build:
	mkdir -p build

build/context.o: kernel/arch/x86_64/context.S | build
	$(CC) $(CFLAGS) -c $< -o $@

build/boot.o: kernel/arch/x86_64/boot.S | build
	$(CC) $(CFLAGS) -c $< -o $@

build/kernel.o: kernel/kernel.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/idt.o: kernel/interrupts/idt.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/interrupts.o: kernel/interrupts/interrupts.S | build
	$(CC) $(CFLAGS) -c $< -o $@

build/pic.o: kernel/interrupts/pic.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/pit.o: kernel/interrupts/pit.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/process.o: kernel/process/process.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/scheduler.o: kernel/process/scheduler.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/heap.o: kernel/memory/heap.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/page.o: kernel/memory/page.c kernel/memory/page.h | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/syscall.o: kernel/syscall/syscall.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/ipc.o: kernel/ipc/ipc.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/semaphore.o: kernel/ipc/semaphore.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/sync_demo.o: kernel/ipc/sync_demo.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/terminal.o: kernel/drivers/terminal.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/keyboard.o: kernel/drivers/keyboard.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/shell.o: kernel/shell/shell.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/block.o: kernel/storage/block.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/ramdisk.o: kernel/storage/ramdisk.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/fs.o: kernel/storage/fs.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/sched_sim.o: kernel/algorithms/sched_sim.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/page_replace.o: kernel/algorithms/page_replace.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/banker.o: kernel/algorithms/banker.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/deadlock.o: kernel/algorithms/deadlock.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/mem_fit.o: kernel/algorithms/mem_fit.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/file_alloc.o: kernel/algorithms/file_alloc.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/disk_sched.o: kernel/algorithms/disk_sched.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/debug.o: kernel/drivers/debug.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/multiboot.o: kernel/memory/multiboot.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/framebuffer.o: kernel/drivers/framebuffer.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/mouse.o: kernel/drivers/mouse.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/font.o: kernel/gui/font.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/graphics.o: kernel/gui/graphics.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/event.o: kernel/gui/event.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/wm.o: kernel/gui/wm.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/desktop.o: kernel/gui/desktop.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(KERNEL): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

iso: $(ISO)

$(ISO): $(KERNEL) boot/grub/grub.cfg
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/jarvis.bin
	cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR)

run: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO) -m 256M -serial stdio

run-headless: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO) -m 256M -display none -serial stdio

test:
	mkdir -p build
	gcc -O2 -Wall -Wextra -Ikernel tests/host_tests.c kernel/algorithms/sched_sim.c kernel/algorithms/page_replace.c kernel/algorithms/banker.c kernel/algorithms/deadlock.c kernel/algorithms/mem_fit.c kernel/algorithms/file_alloc.c kernel/algorithms/disk_sched.c kernel/ipc/ipc.c kernel/ipc/semaphore.c kernel/ipc/sync_demo.c kernel/storage/block.c kernel/storage/ramdisk.c kernel/storage/fs.c kernel/drivers/framebuffer.c kernel/drivers/mouse.c kernel/gui/font.c kernel/gui/graphics.c kernel/gui/event.c kernel/gui/wm.c kernel/gui/desktop.c -o build/host_tests
	./build/host_tests


test-ai:
	python3 -m unittest discover -s tests -p "test_*.py"

test-qemu: $(ISO)
	python3 scripts/qemu_smoke_test.py

test-gui: $(ISO)
	python3 scripts/qemu_gui_test.py

test-release: $(ISO)
	python3 scripts/validate_release_workflow.py

test-all: test test-ai test-qemu test-gui test-release

clean:
	rm -rf build
