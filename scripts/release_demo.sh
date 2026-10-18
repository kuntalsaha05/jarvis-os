#!/usr/bin/env bash
# ==============================================================================
# JARVIS-OS — Phase 7: Reproducible Interactive & Automated Release Demo
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

echo "=================================================="
echo "          JARVIS-OS RELEASE DEMONSTRATION         "
echo "=================================================="
echo "Architecture: x86-64 Long Mode Bare-Metal Kernel"
echo "Host Service: Local AI Assistant Architecture"
echo "=================================================="

# 1. Build Verification
echo ""
echo "[Step 1/5] Verifying Kernel Binary & Bootable ISO..."
make iso

# 2. Host Unit Tests
echo ""
echo "[Step 2/5] Running Host Algorithm Unit Tests..."
make test

# 3. AI Companion Test Suite
echo ""
echo "[Step 3/5] Running Python AI Companion Tests..."
make test-ai

# 4. Host AI Companion Demonstrations
echo ""
echo "[Step 4/5] Demonstrating Host AI Companion Capabilities..."
python3 jarvis/main.py --status
python3 jarvis/main.py --fs-diagnostics
python3 jarvis/main.py --explain "banker"
python3 jarvis/main.py --query "What is the memory usage?"

# 5. Full End-to-End Validation
echo ""
echo "[Step 5/5] Running 13-Step Automated Release Workflow..."
python3 scripts/validate_release_workflow.py

echo ""
echo "=================================================="
echo "       DEMONSTRATION COMPLETED SUCCESSFULLY       "
echo "=================================================="
echo "To interact directly with the live bare-metal kernel:"
echo "  - Interactive VGA + Serial:  make run"
echo "  - Headless Serial Shell:     make run-headless"
echo "To interact with the AI assistant:"
echo "  - Interactive AI REPL:       python3 jarvis/main.py --interactive"
echo "=================================================="

