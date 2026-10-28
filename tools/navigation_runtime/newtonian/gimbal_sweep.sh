#!/usr/bin/env bash
# Step 4: N1 main-nozzle travel sweep on the replay cases (delay 3).
# usage: gimbal_sweep.sh TAG [build] [degrees...]
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
T=$1; shift
if [ "$1" = build ]; then
  cmake --build "$ELITE_ROOT/build/headless_server" --target EliteNewtonianReplay -j 8 \
    > "$ELITE_DIAG_DIR/elite_${T}_build.log" 2>&1 || { echo BUILD FAILED; exit 1; }
  shift
fi
ANGLES=${*:-0 2 4 7 10 15 20}
for G in $ANGLES; do
  for C in newtonian-I:Newtonian-I newtonian-Y:Newtonian-Y newtonian-X:Newtonian-X \
           newtonian-I-narrow:Newtonian-I newtonian-X-narrow:Newtonian-X; do
    CASE=${C%%:*}; M=${C##*:}; TAG=${T}_g${G}_${CASE}
    ELITE_DEV_N1_GIMBAL_DEG=$G bash "$NEWTONIAN_TOOLS/replay.sh" "$CASE" "$TAG" "$M" 330 3 > /dev/null
    echo -e "g=$G\t$(python "$NEWTONIAN_TOOLS/sweep_summary.py" "$TAG")"
  done
done
