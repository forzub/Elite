#!/usr/bin/env bash
# Offline regression matrix: I at control delay 1/3/6 ticks, Y and X at 3.
# usage: replay_matrix.sh TAG [build]
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
T=$1
if [ "$2" = build ]; then
  cmake --build "$ELITE_ROOT/build/headless_server" --target EliteNewtonianReplay -j 8 \
    > "$ELITE_DIAG_DIR/elite_${T}_replay_build.log" 2>&1 || {
    echo BUILD FAILED; grep -m15 -E "error|Error" "$ELITE_DIAG_DIR/elite_${T}_replay_build.log"; exit 1; }
  echo replay build ok
fi
run() { # case model delay tag
  bash "$NEWTONIAN_TOOLS/replay.sh" "$1" "$4" "$2" 260 "$3"
  python "$NEWTONIAN_TOOLS/decision.py" "$4" | sed -n '3,4p;6p;9p;11,12p'
  python "$NEWTONIAN_TOOLS/terminal.py" "$4" | grep -E 'max omega|DONE at'
}
run newtonian-I Newtonian-I 3 "${T}I3"
run newtonian-I Newtonian-I 1 "${T}I1"
run newtonian-I Newtonian-I 6 "${T}I6"
run newtonian-Y Newtonian-Y 3 "${T}Y3"
run newtonian-X Newtonian-X 3 "${T}X3"
