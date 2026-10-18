# JARVIS-OS — Setup, Build, and Run Guide

This guide describes how to install prerequisites, build the x86-64 bare-metal kernel, run the system in QEMU, execute the test suites, and configure the host-side AI assistant.

---

## 1. Prerequisites & Toolchains

### 1.1 Linux Packages (Debian / Ubuntu)
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    gcc \
    make \
    python3 \
    python3-pip \
    qemu-system-x86 \
    xorriso \
    grub-pc-bin \
    grub-common \
    mtools
```

### 1.2 Cross-Compiler Toolchain
JARVIS-OS builds freestanding 64-bit binaries using `x86_64-elf-gcc` and `x86_64-elf-ld`.
Verify they are available in your path:
```bash
x86_64-elf-gcc --version
x86_64-elf-ld --version
```
*(On systems where `x86_64-elf-gcc` is not installed system-wide, a standard `gcc` with `-m64 -ffreestanding -mno-red-zone` can be configured in the Makefile).*

---

## 2. Building the Kernel and Bootable ISO

### 2.1 Compile Kernel Binary
```bash
make
```
Compiles all C and assembly source files into `build/jarvis.bin` via the linker script `kernel/linker.ld`.

### 2.2 Generate Bootable ISO
```bash
make iso
```
Packages `build/jarvis.bin` with `boot/grub/grub.cfg` into `build/jarvis.iso` using `grub-mkrescue` and `xorriso`.

### 2.3 Clean Build Artifacts
```bash
make clean
```
Deletes the `build/` directory.

---

## 3. Running JARVIS-OS in QEMU

### 3.1 Interactive Mode (Dual VGA Window + Serial Console)
```bash
make run
```
Spawns QEMU with a graphical VGA window (80x25 text mode) and directs COM1 serial output to your terminal. Keyboard input is accepted from both the graphical window and the terminal.

### 3.2 Headless Mode (Terminal Only)
```bash
make run-headless
```
Runs QEMU without a graphical window (`-display none`), directing all display and shell interactions through the serial console. Press `Ctrl+A, X` to exit QEMU.

---

## 4. Verification and Testing

JARVIS-OS includes a four-tier automated testing pyramid:

| Target | Command | Description |
| :--- | :--- | :--- |
| **Host Unit Tests** | `make test` | Pure C tests for all 15 algorithm and kernel data-structure invariant suites (scheduling, paging, banker, disk, memory fit, file allocation, IPC queues, semaphores, mutexes, block bounds, and filesystem). |
| **AI Unit Tests** | `make test-ai` | Python test suite verifying 18 test cases across backends, 8-step agent cycle, tool allowlist, permission gating, timeouts, and serial transport framing. |
| **QEMU Smoke Test** | `make test-qemu` | Automated headless QEMU test asserting 65 bare-metal boot and shell checkpoints in ~12 seconds. |
| **Release Validation** | `make test-release` | End-to-end 13-step release workflow covering clean build, ISO creation, QEMU interaction, and AI assistant security. |
| **Complete Verification**| `make test-all` | Runs all four test suites in sequence. |

---

## 5. Host AI Companion Configuration

The host-side AI assistant operates independently of the kernel binary in `jarvis/`.

### 5.1 CLI Modes
```bash
# Display system, health check, and tool permissions
python3 jarvis/main.py --status

# Inspect JARVIS-FS disk image geometry and files
python3 jarvis/main.py --fs-diagnostics

# Query the AI assistant about an operating system concept
python3 jarvis/main.py --explain "banker"

# Single prompt query with autonomous tool dispatch
python3 jarvis/main.py --query "What is the memory usage?"

# Interactive REPL session
python3 jarvis/main.py --interactive
```

### 5.2 Selecting Backends
- **Mock Backend (Default)**:
  ```bash
  python3 jarvis/main.py --backend mock --interactive
  ```
  Fast, deterministic, zero-dependency mode used for automated testing.

- **llama.cpp (Quantized Local Inference)**:
  ```bash
  pip install llama-cpp-python
  export JARVIS_LLAMA_MODEL="/path/to/model.gguf"
  python3 jarvis/main.py --backend llama --interactive
  ```

- **AirLLM (Layer-Streaming Low-VRAM Inference)**:
  ```bash
  pip install airllm torch
  export JARVIS_AIRLLM_MODEL="garage-bAInd/Platypus2-70B-instruct"
  python3 jarvis/main.py --backend airllm --interactive
  ```

