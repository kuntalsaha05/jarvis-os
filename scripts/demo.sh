#!/usr/bin/env bash
# ==============================================================================
# JARVIS-OS — Unified Demonstration Script (Automated & Interactive)
# ==============================================================================

set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}==================================================${NC}"
echo -e "${BLUE}         JARVIS-OS UNIFIED DEMONSTRATION          ${NC}"
echo -e "${BLUE}==================================================${NC}"
echo "System: Bare-Metal x86-64 Operating System + Host Local AI"
echo "Root:   ${ROOT_DIR}"
echo ""

# ------------------------------------------------------------------------------
# 1. Dependency & Toolchain Check
# ------------------------------------------------------------------------------
echo -e "${YELLOW}[1/6] Checking Required Dependencies & Toolchains...${NC}"
REQUIRED_TOOLS=("gcc" "make" "python3" "qemu-system-x86_64" "xorriso" "grub-mkrescue")
MISSING_TOOLS=()

for tool in "${REQUIRED_TOOLS[@]}"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        MISSING_TOOLS+=("$tool")
    fi
done

if [ ${#MISSING_TOOLS[@]} -ne 0 ]; then
    echo -e "${RED}[ERROR] Missing required tools:${NC} ${MISSING_TOOLS[*]}"
    echo "Install dependencies on Debian/Ubuntu with:"
    echo "  sudo apt install build-essential gcc make python3 qemu-system-x86 xorriso grub-pc-bin grub-common"
    exit 1
fi
echo -e "${GREEN}  All core tools detected.${NC}"

# ------------------------------------------------------------------------------
# 2. Build Kernel Binary & Bootable ISO
# ------------------------------------------------------------------------------
echo -e "${YELLOW}[2/6] Building Clean Kernel Binary & Bootable ISO...${NC}"
make clean >/dev/null 2>&1
if ! make > build_demo.log 2>&1; then
    echo -e "${RED}[ERROR] Kernel build failed! See build_demo.log${NC}"
    exit 1
fi

if ! make iso >> build_demo.log 2>&1; then
    echo -e "${RED}[ERROR] ISO generation failed! See build_demo.log${NC}"
    exit 1
fi

ISO_PATH="${ROOT_DIR}/build/jarvis.iso"
if [ ! -f "${ISO_PATH}" ]; then
    echo -e "${RED}[ERROR] ISO file not created at ${ISO_PATH}${NC}"
    exit 1
fi
ISO_SIZE=$(du -h "${ISO_PATH}" | cut -f1)
echo -e "${GREEN}  Kernel build & ISO generation successful (${ISO_SIZE} at ${ISO_PATH})${NC}"
rm -f build_demo.log

# ------------------------------------------------------------------------------
# 3. Host C Algorithm Tests
# ------------------------------------------------------------------------------
echo -e "${YELLOW}[3/7] Running 16 Host C Algorithm & GUI Unit Test Suites...${NC}"
if ! make test > host_tests_demo.log 2>&1; then
    echo -e "${RED}[ERROR] Host unit tests failed!${NC}"
    cat host_tests_demo.log
    rm -f host_tests_demo.log
    exit 1
fi
echo -e "${GREEN}  All 16 Host C algorithm & GUI suites passed successfully.${NC}"
rm -f host_tests_demo.log

# ------------------------------------------------------------------------------
# 4. Python AI Companion Tests
# ------------------------------------------------------------------------------
echo -e "${YELLOW}[4/7] Running 18 Python AI Companion & Agent Unit Tests...${NC}"
if ! make test-ai > ai_tests_demo.log 2>&1; then
    echo -e "${RED}[ERROR] AI companion unit tests failed!${NC}"
    cat ai_tests_demo.log
    rm -f ai_tests_demo.log
    exit 1
fi
echo -e "${GREEN}  All 18 AI unit tests passed successfully.${NC}"
rm -f ai_tests_demo.log

# ------------------------------------------------------------------------------
# 5. QEMU Bare-Metal Verification (Bounded Runtime)
# ------------------------------------------------------------------------------
echo -e "${YELLOW}[5/7] Booting QEMU with Bounded Runtime & Capturing Serial Output...${NC}"
if ! make test-qemu > qemu_demo.log 2>&1; then
    echo -e "${RED}[ERROR] QEMU bare-metal verification failed!${NC}"
    cat qemu_demo.log
    rm -f qemu_demo.log
    exit 1
fi
echo -e "${GREEN}  All 65 QEMU bare-metal checkpoints passed in ~12 seconds.${NC}"
rm -f qemu_demo.log

# ------------------------------------------------------------------------------
# 6. QEMU Native GUI Visual Smoke Test (Screendumps & Input Navigation)
# ------------------------------------------------------------------------------
echo -e "${YELLOW}[6/7] Running Native Graphical Desktop Visual Smoke Test...${NC}"
if ! make test-gui > gui_demo.log 2>&1; then
    echo -e "${RED}[ERROR] GUI visual smoke test failed!${NC}"
    cat gui_demo.log
    rm -f gui_demo.log
    exit 1
fi
echo -e "${GREEN}  All 3 Native GUI window screendumps and input navigation passed.${NC}"
rm -f gui_demo.log

# ------------------------------------------------------------------------------
# 7. Interactive Mode vs Automated 13-Step Release Validation
# ------------------------------------------------------------------------------
if [ "$1" == "--interactive" ] || [ "$1" == "-i" ]; then
    echo -e "${BLUE}==================================================${NC}"
    echo -e "${BLUE}        ENTERING INTERACTIVE DEMO MODE            ${NC}"
    echo -e "${BLUE}==================================================${NC}"
    echo "Recommended commands to test in the 'jarvis>' shell:"
    echo "  1. status       - Inspect kernel state, uptime ticks, free pages"
    echo "  2. irq          - Verify timer IRQ0 and keyboard IRQ1 vectors"
    echo "  3. threads      - Inspect Round-Robin quantum and context switches"
    echo "  4. memory       - View physical allocator statistics"
    echo "  5. pftest       - Safe page-fault decoder test without crash"
    echo "  6. df           - View JARVIS-FS volume and inode allocation"
    echo "  7. touch / write / cat / trunc / rm - Test filesystem operations"
    echo "  8. sched / paging / deadlock / disk - Run algorithm benchmarks"
    echo "  9. ai           - Display AI assistant protocol and security model"
    echo " 10. gui          - Launch native dark graphical desktop"
    echo "Press Ctrl+A, X to exit QEMU."
    echo ""
    make run-headless
else
    echo -e "${YELLOW}[7/7] Executing Complete 13-Step Automated Release Validation...${NC}"
    if ! python3 scripts/validate_release_workflow.py; then
        echo -e "${RED}[ERROR] Release workflow validation failed!${NC}"
        exit 1
    fi
    echo ""
    echo -e "${GREEN}==================================================${NC}"
    echo -e "${GREEN}   DEMONSTRATION & VERIFICATION COMPLETED: PASS   ${NC}"
    echo -e "${GREEN}==================================================${NC}"
    echo "To run the interactive shell directly in your terminal:"
    echo "  ./scripts/demo.sh --interactive"
    echo "  (or: make run-headless)"
    echo "To interact with the host AI assistant:"
    echo "  python3 jarvis/main.py --interactive"
    echo "=================================================="
fi

