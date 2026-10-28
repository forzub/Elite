#!/usr/bin/env bash
# Newtonian manoeuvre-program pilot unit tests.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
cd "$ELITE_ROOT"
EXE="$ELITE_DIAG_DIR/elite_newtonian_unit.exe"
rm -f "$EXE"
g++ -std=c++17 -O1 -Wall -Wextra -DGLM_ENABLE_EXPERIMENTAL -I. -Isrc \
  tests/navigation_runtime/NewtonianOptimalControllerTests.cpp -o "$EXE" 2>&1 \
  | grep -E 'error|warning' | head -20
"$EXE"; rc=$?
rm -f "$EXE"
exit $rc
