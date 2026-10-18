# JARVIS-OS — Phase 10: Native Graphical Desktop Specification & Verification

**Document Version:** 1.0  
**Target Architecture:** x86-64 Bare-Metal Operating System  
**Implementation Files:**  
- `kernel/drivers/framebuffer.h`, `kernel/drivers/framebuffer.c` (Linear framebuffer discovery and primitives)
- `kernel/drivers/mouse.h`, `kernel/drivers/mouse.c` (PS/2 auxiliary device and packet decoder)
- `kernel/gui/font.h`, `kernel/gui/font.c` (8x16 Terminus bitmap font)
- `kernel/gui/graphics.h`, `kernel/gui/graphics.c` (2D drawing library, strings, windows, progress bars)
- `kernel/gui/desktop.h`, `kernel/gui/desktop.c` (Native dark JARVIS desktop manager)
- `scripts/qemu_gui_test.py` (Visual smoke test and automated screendump verifier)
- `boot/grub/grub.cfg` (Video mode configuration and keep gfxpayload)

---

## 1. Architectural Overview & Design Decisions

Phase 10 equips JARVIS-OS with a native, bare-metal graphical desktop while preserving the existing VGA text console (`0xB8000`) and serial communications (`0x3F8` and `0xE9`) as zero-regression fallbacks.

```
+---------------------------------------------------------------------------------------+
|                                    JARVIS-OS DESKTOP                                  |
+---------------------------------------------------------------------------------------+
| TOP STATUS BAR: Uptime | Arch (x86-64 Long Mode) | Free Pages (X/64) | Mode: GUI      |
+---------------------------------------------------+-----------------------------------+
| ACTIVE WINDOW (Terminal / AI Assistant)           | SYSTEM MONITOR PANEL              |
|                                                   |                                   |
| - Graphical Terminal Shell                        | - CPU & Preemptive RR Scheduler   |
|   Prompt: jarvis>                                 |   Context Switches, Preemptions   |
|   Interactive Command Execution                   | - Physical & Virtual Memory       |
|                                                   |   Page Frame Pool, Identity Map   |
| - JARVIS AI Companion Chat                        | - Storage & JARVIS-FS             |
|   Protocol v1.0 JSON over COM1                    |   Capacity, Inodes, Rollback      |
|   Data Provenance ([LIVE] vs [SIM])               | - Provenance Transparency Legend  |
|   4-Tier Security Sandbox                         |                                   |
+---------------------------------------------------+-----------------------------------+
| BOTTOM TASKBAR: [JARVIS OS] | [1: Terminal] | [2: SysMon] | [3: AI Chat] | [ESC: Shell] |
+---------------------------------------------------------------------------------------+
| Hardware Input: PS/2 Keyboard IRQ1 + PS/2 Mouse IRQ12 (Hardware Arrow Cursor)         |
+---------------------------------------------------------------------------------------+
```

### 1.1 Robust Framebuffer Discovery Subsystem
The kernel does not hardcode framebuffer addresses or assume resolution. `fb_init()` implements a three-tier discovery hierarchy:
1. **Multiboot 1.0 Framebuffer Table (Bit 12):** When booted via GRUB 2 with `gfxpayload=keep`, `mbi->flags & (1 << 12)` provides `framebuffer_addr`, `width`, `height`, `pitch`, and `bpp`.
2. **Multiboot VBE Mode Info (Bit 11):** If bit 12 is absent but bit 11 is set, the kernel parses `vbe_mode_info_t` at `mbi->vbe_mode_info`, extracting `phys_base_ptr` and geometry.
3. **BGA / QEMU PCI VGA Detection:** If GRUB runs in text mode or headless, the kernel queries Bochs DISPI I/O port `0x01CE` for BGA ID (`0xB0C0`–`0xB0C6`) and reads PCI BAR 0 (Bus 0, Dev 2, Func 0, Reg `0x10`). It dynamically programs 1024x768x32 linear framebuffer mode directly from kernel code.
4. **Graceful Fallback:** If no hardware framebuffer is present, the kernel logs the fallback status and remains in standard VGA text mode (`0xB8000`) without halting or crashing.

### 1.2 4 GiB Long Mode Identity Paging
In QEMU, the PCI video RAM aperture is mapped at high physical addresses (e.g. `0xFD000000`). To allow the 64-bit kernel to write directly to the linear framebuffer without causing Page Faults (`#PF`), `kernel/arch/x86_64/boot.S` was expanded to initialize 4 Page Directories (`pd_tables`, 2048 entries of 2 MiB each), identity-mapping the entire `0x00000000` to `0xFFFFFFFF` (4 GiB) address space.

### 1.3 2D Graphics Engine & Typography
- **Pixel Operations:** Fully bounds-checked `fb_put_pixel()` and `fb_get_pixel()` supporting 32-bit (ARGB/XRGB), 24-bit, and 16-bit color depths.
- **Geometric Primitives:** Filled rectangles (`fb_fill_rect`), outlined rectangles (`fb_draw_rect_outline`), and Bresenham lines (`fb_draw_line`).
- **Typography:** Embedded 8x16 Terminus bitmap font (`kernel/gui/font.c`) providing high-contrast readability across 96 ASCII glyphs.
- **UI Components:** Window frames with title bars, taskbar buttons, and progress bars.

### 1.4 PS/2 Mouse Driver (IRQ12)
- **Controller Setup:** `mouse_init()` enables the PS/2 auxiliary device (`0xA8`), unmasks IRQ12 on the slave PIC, configures default mouse sampling, and enables streaming mode (`0xF4`).
- **Packet Decoder:** The IRQ12 handler (`mouse_handler`) validates bit 3 of byte 0, decodes 3-byte packets, sign-extends 9-bit delta values, inverts the Y axis, and clamps coordinates to screen bounds `(0..width-1, 0..height-1)`.
- **Cursor Rendering:** Draws an 11x11 hardware arrow cursor atop the desktop layer.

---

## 2. Desktop Components & User Experience

### 2.1 Dark JARVIS Aesthetic
- Background Wallpaper: Deep dark slate (`#0D1117`)
- Panel Background: Dark navy panel (`#161B22`)
- Border Accent: Subtle slate border (`#30363D`)
- Brand / Highlight Accent: Glowing sci-fi cyan (`#00E5FF`)
- Text Hierarchy: Primary white (`#FFFFFF`), secondary gray (`#8B949E`), success green (`#2EA043`), warning yellow (`#D29922`)

### 2.2 Top Status Bar & Bottom Taskbar
- **Status Bar:** Real-time uptime ticks, architecture confirmation (`x86-64 Long Mode`), physical page pool availability (`Pages Free: X / 64`), and desktop mode indicator.
- **Taskbar:** Interactive buttons for window selection: `[1: Terminal]`, `[2: System Mon]`, `[3: AI Assistant]`, and `[ESC: Text Shell]`.

### 2.3 Graphical Terminal Window
- Fully interactive in-kernel terminal emulator executing the `jarvis>` shell.
- Scrollable history buffer (16 lines).
- Commands executed within the graphical terminal mirror output to COM1 serial for remote diagnostics and test capture.

### 2.4 Live System Monitor Panel
- **CPU & Preemptive Scheduler:** Displays Round-Robin quantum (5 ticks = 50 ms), total context switches, involuntary timer preemptions, and voluntary yields.
- **Physical & Virtual Memory:** Frame allocator utilization bar, free frame count, and identity-mapped boundary definition.
- **Storage & Filesystem:** JARVIS-FS superblock magic (`0x4A415256`), total blocks (128 blocks = 64 KiB), block allocation progress bar, and active inode metrics.
- **Provenance Transparency:** Explicitly displays the legend:
  - `[LIVE KERNEL]: PIT Preemption, Page Allocator, JARVIS-FS Inodes`
  - `[EDUCATIONAL SIMULATORS]: Banker's Matrix, Belady 3v4 Anomaly, SSTF Seek`

### 2.5 JARVIS AI Companion Interface
- Connects to the host-side companion service over COM1 serial line (Protocol v1.0).
- Formats queries as versioned JSON messages (`{"version":"1.0","type":"query","payload":{"prompt":"..."}}`).
- Enforces strict security policy: **No unrestricted shell execution** of model output.
- All synthesized responses display data provenance tags (`[LIVE_KERNEL]` or `[OFFLINE_SIMULATION]`).

---

## 3. Desktop Controls & Navigation

| Input Action | Target Component | Behavior |
|---|---|---|
| `TAB` | Desktop Manager | Cycle active window (`Terminal -> System Monitor -> AI Assistant`) |
| `ESC` | Desktop Manager | Exit graphical desktop and restore VGA text console shell |
| `F1` / `F2` / `F3` | Desktop Manager | Directly focus Terminal, System Monitor, or AI Assistant window |
| `Mouse Move` | Mouse Cursor | Updates 11x11 hardware cursor position on screen |
| `Mouse Left Click` | Taskbar Tabs | Switches active window or exits to text console |
| Alphanumeric Keys | Active Input Field | Types characters into Terminal command line or AI prompt |
| `Backspace` | Active Input Field | Deletes last character in command buffer |
| `Enter` (`\n`) | Active Window | Dispatches command in Terminal or transmits query in AI Chat |

---

## 4. QEMU Configurations & Commands

### 4.1 Interactive Graphical Session (GTK / SDL Window)
```bash
# Launch interactive QEMU session with graphical display and serial stdio
make run

# Inside the 'jarvis>' shell, type:
gui
```

### 4.2 Headless Serial Session (Regression Safe)
```bash
# Launch headless QEMU session (display none, serial stdio)
make run-headless
```

### 4.3 Automated Visual Smoke Test (Screendumps)
```bash
# Execute automated visual smoke test
make test-gui
# (or: python3 scripts/qemu_gui_test.py)
```

---

## 5. Verification Results

### 5.1 Host Unit Tests (`make test`)
```
==================================================
   JARVIS-OS HOST ALGORITHM VERIFICATION SUITE    
==================================================
[TEST] CPU Scheduling Algorithms... PASS
[TEST] Page Replacement & Belady Anomaly... PASS
[TEST] Banker's Deadlock Avoidance... PASS
[TEST] Disk Head Scheduling (FCFS, SSTF, SCAN, C-SCAN)... PASS
[TEST] Memory Allocation Fit (First, Best, Worst)... PASS
[TEST] File Allocation Methods (Contiguous, Linked, Indexed)... PASS
[TEST] Deadlock Detection (WFG/Matrix) & Prevention... PASS
[TEST] Process Lifecycle State Transitions... PASS
[TEST] Memory Boundary & Frame Invariants... PASS
[TEST] IPC Subsystem & Bounded Queues... PASS
[TEST] Mutex Ownership & Semaphore Invariants... PASS
[TEST] Deadlock Scenarios & Havender Prevention... PASS
[TEST] Block Device Interface & Bounds Checks... PASS
[TEST] JARVIS-FS Filesystem Correctness & Rollback... PASS
[TEST] File Allocation (Reuse) & Disk Scheduling Invariants... PASS
[TEST] Framebuffer & Native GUI Invariants... PASS
==================================================
   ALL HOST UNIT TESTS COMPLETED SUCCESSFULLY! (16/16)
==================================================
```

### 5.2 Native GUI Visual Smoke Test (`make test-gui`)
```
==================================================
      JARVIS-OS NATIVE GUI VISUAL SMOKE TEST      
==================================================
[1/5] Launching QEMU with VBE/BGA Framebuffer and Monitor Socket...
[2/5] Entering Native Graphical Desktop (sending 'gui' command)...
  Capturing Terminal Window screendump...
[3/5] Navigating to System Monitor Window (sending TAB key)...
  Capturing System Monitor screendump...
[4/5] Navigating to AI Assistant Window (sending TAB key)...
  Capturing AI Assistant screendump...
[5/5] Returning to Text Console Shell (sending ESC key)...
  [PASS] Terminal Window Screenshot: build/desktop_term.ppm (2359312 bytes, dims=1024 768, unique_bytes=25)
  [PASS] System Monitor Window Screenshot: build/desktop_sysmon.ppm (2359312 bytes, dims=1024 768, unique_bytes=25)
  [PASS] AI Assistant Window Screenshot: build/desktop_ai.ppm (2359312 bytes, dims=1024 768, unique_bytes=25)
==================================================
   ALL NATIVE GUI VISUAL SMOKE TESTS PASSED!      
==================================================
```

### 5.3 Automated Regression Suite Summary
- `make test`: **16 / 16 PASS** (100%)
- `make test-ai`: **18 / 18 PASS** (100%)
- `make test-qemu`: **65 / 65 PASS** (100%)
- `make test-gui`: **3 / 3 PASS** (100%)
- `make test-release`: **13 / 13 PASS** (100%)
- `./scripts/demo.sh`: **7 / 7 Stages PASS** (100%)

---

## 6. Conscious Limitations & Boundary Definitions

In accordance with strict technical transparency:
1. **Window Management:** The desktop employs a fixed panel/tiled layout with focused active panels rather than free-floating overlapping windows with dynamic z-ordering.
2. **Software Rasterization:** All 2D drawing primitives execute directly in software without 3D GPU acceleration.
3. **Mouse Protocol:** Supports standard 3-byte PS/2 protocol (IntelliMouse 4-byte scroll wheel packets are omitted for hardware simplicity).
4. **Font Set:** Embedded 8x16 Terminus bitmap font covers printable ASCII characters (0x20 to 0x7E); proportional TrueType/OpenType rendering is intentionally out of scope.

