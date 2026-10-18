#!/usr/bin/env bash
# Run from Ubuntu/WSL. Optional --full-grade also checks the team's other labs.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p test-results
python3 tests/test_memdump.py 2>&1 | tee test-results/memdump-guard.log
python3 tests/test_shell.py 2>&1 | tee test-results/shell-after.log
./grade-lab-util memdump 2>&1 | tee test-results/memdump-grade.log
if [[ "${1-}" == "--full-grade" ]]; then
  make grade GRADEFLAGS="--results test-results/final-grades.json" 2>&1 |
    tee test-results/grade-final.log
fi
