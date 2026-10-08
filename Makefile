TARGET = x86_64-elf
CC = $(TARGET)-gcc
LD = $(TARGET)-ld
OBJCOPY = $(TARGET)-objcopy

CFLAGS = -ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -O2 -Wall -Wextra
LDFLAGS = -T kernel/linker.ld

KERNEL = build/jarvis.bin

all: $(KERNEL)

build:
	mkdir -p build

build/boot.o: kernel/arch/x86_64/boot.S | build
	$(CC) $(CFLAGS) -c $< -o $@

build/kernel.o: kernel/kernel.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

build/idt.o: kernel/interrupts/idt.c | build
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(KERNEL): build/boot.o build/kernel.o build/idt.o
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -rf build
