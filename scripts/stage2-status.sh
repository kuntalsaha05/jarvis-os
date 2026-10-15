#!/usr/bin/env bash
cd "$HOME/jarvis-os" || exit 1

LOG="logs/preemption-stage2.log"

{
    echo "=== JARVIS-OS PREEMPTION STATUS ==="
    date
    echo
    echo "=== Current build ==="
    make
    echo
    echo "=== Current source status ==="
    git status --short
    echo
    echo "=== Saved backups ==="
    find backups -maxdepth 2 -type d 2>/dev/null | head -30
} > "$LOG" 2>&1

RESULT=$?
cat "$LOG"
echo
echo "Exit code: $RESULT"
echo "Log saved to: $LOG"
exit 0
