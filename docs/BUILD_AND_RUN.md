# JARVIS-OS — Build and Run Guide

This guide details all prerequisites, compilation targets, test commands, and runtime options for JARVIS-OS.

---

## 1. Prerequisites and Toolchain

JARVIS-OS requires an x86-64 freestanding cross-compiler and Multiboot ISO generation tools:

| Component | Minimum Version | Tested Environment | Role |
|-----------|-----------------|--------------------|------|
| `x86_64-elf-gcc` | 14.2.0 | Ubuntu 24.04 / WSL2 | Compiles freestanding x86-64 C files |
| `x86_64-elf-ld` | 2.44 | GNU Binutils | Links binary against `kernel/linker.ld` |
| `make` | 4.3 | GNU Make | Builds dependency targets |
| `qemu-system-x86_64` | 8.2.2 | QEMU Emulator | Emulates target bare-metal machine |
| `grub-mkrescue` | 2.12 | GNU GRUB | Packages kernel ELF into bootable ISO |
| `xorriso` | 1.5.6 | libburnia | Required backend for `grub-mkrescue` |
| `python3` | 3.12+ | Python Software Foundation | Host AI companion and test orchestrator |

---

## 2. Make Targets Reference

All build rules are defined in the project root [`Makefile`](../Makefile):

```bash
# 1. Compile all kernel objects and link build/jarvis.bin
make

# 2. Build bootable CD-ROM ISO image at build/jarvis.iso
make iso

# 3. Launch interactive QEMU session with VGA window + Serial console
make run

# 4. Launch headless QEMU session (no graphical window; pure serial stdio)
make run-headless

# 5. Run host C unit tests for scheduling, paging, banker, and disk algorithms
make test

# 6. Run Python unit tests for the AI companion and tool registry
make test-ai

# 7. Run automated end-to-end QEMU boot and shell smoke test
make test-qemu

# 8. Run all test suites in sequence
make test-all

# 9. Clean build artifacts and generated ISOs
make clean
```

---

## 3. Running JARVIS-OS in QEMU

### 3.1 Graphical Mode (`make run`)
Launches QEMU with a graphical VGA window and redirects serial output to the host terminal:
```bash
qemu-system-x86_64 -cdrom build/jarvis.iso -m 256M -serial stdio
```
- In this mode, you can type commands directly in the graphical window (using the PS/2 keyboard driver) or in your host terminal (using the COM1 serial driver).

### 3.2 Headless Mode (`make run-headless`)
Ideal for remote SSH sessions, WSL2 environments without an X11 server, or automated scripts:
```bash
qemu-system-x86_64 -cdrom build/jarvis.iso -m 256M -display none -serial stdio
```
- Output appears directly in your terminal.
- To exit QEMU in headless mode, press `Ctrl+A` then `X`, or use `killall qemu-system-x86_64`.

---

## 4. Running the Host-Side AI Companion

The companion service lives in `jarvis/` and operates on the host:

```bash
# Run the interactive AI companion demonstration
python3 jarvis/main.py

# Query the companion with a specific prompt
python3 jarvis/main.py --query "What is the current kernel status?"

# Query with full audit log output
python3 jarvis/main.py --query "Show me memory stats" --audit

# Select local LLM backend
python3 jarvis/main.py --backend mock   # Default deterministic backend
python3 jarvis/main.py --backend llama  # llama.cpp local quantized model
python3 jarvis/main.py --backend airllm # AirLLM layer-wise model
```

---

## 5. Troubleshooting & Common Issues

### 5.1 "No display specified" or X11 connection failure
If running on WSL2 without WSLg or an active X11 server, `make run` may attempt to open a window and pause.
**Solution**: Use `make run-headless` or `make test-qemu`, which pass `-display none`.

### 5.2 GRUB consuming the first piped character
When piping commands directly into QEMU (`printf "status\n" | make run-headless`), GRUB's menu reads from the serial port during the initial 0-second menu display and may consume the first byte.
**Solution**: Prepend a leading newline to your input sequence:
```bash
printf "\nstatus\nsched\n" | make run-headless
```
This ensures the newline satisfies GRUB's prompt, and all subsequent commands arrive cleanly at the `jarvis>` shell.

