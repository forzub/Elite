#!/usr/bin/env bash
# Offline behaviour matrix: every built-in scenario of EliteNewtonianScenarios
# x models (MATRIX_MODELS, default "I X") x control delay 1/3/6 ticks, production route autopilot on
# the production ShipCore step, judged by matrix.py.
# usage: scenario_matrix.sh TAG [build] [scenario...]
# output: $ELITE_DIAG_DIR/elite_TAG_matrix.tsv (results) and the table on
# stdout. Traces of passing runs are removed; failing runs keep theirs.
# Healthy Y and X fly identically (same aggregate main thrust, same RCS
# nodes), so Y is left out of the healthy default.
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
T=$1; shift
if [ "$1" = build ]; then
  shift
  cmake --build "$ELITE_ROOT/build/headless_server" \
    --target EliteNewtonianScenarios EliteNewtonianReplay -j 8 \
    > "$ELITE_DIAG_DIR/elite_${T}_build.log" 2>&1 || {
    echo BUILD FAILED; grep -m15 -E "error" "$ELITE_DIAG_DIR/elite_${T}_build.log"; exit 1; }
fi
SCEN_EXE="$ELITE_ROOT/build/headless_server/EliteNewtonianScenarios.exe"
CD="$ELITE_DIAG_DIR/elite_${T}_cases"
rm -rf "$CD"; mkdir -p "$CD"
"$SCEN_EXE" "$CD" "$CASES_DIR/newtonian-I.case" "$CASES_DIR/newtonian-Y.case" \
  "$CASES_DIR/newtonian-X.case" "$@" > "$ELITE_DIAG_DIR/elite_${T}_plans.txt" || {
  echo SCENARIO GENERATION FAILED; exit 1; }
export T CD REPLAY_EXE NEWTONIAN_TOOLS
one() { # <case basename> <delay>
  local N=$1 D=$2 M=Newtonian-${1##*-} TAG=${T}_${1}_d${2}
  local O="$ELITE_DIAG_DIR/elite_${TAG}"
  if [ -f "$CD/$N.case" ]; then
    ELITE_NEWTONIAN_POLICY_TRACE="${O}_decision.tsv" \
    ELITE_NEWTONIAN_EVENT_TRACE="${O}_events.tsv" \
    ELITE_REPLAY_STATE_TRACE="${O}_state.tsv" \
    ELITE_REPLAY_STOP_STILL_S=15 \
      "$REPLAY_EXE" "$CD/$N.case" "$M" 900 "$D" > /dev/null 2> "${O}_replay.stderr"
  fi
  local R; R=$(python "$NEWTONIAN_TOOLS/matrix.py" run "$TAG" "$CD/$N.geom")
  echo "$R"
  # Keep the traces of a failing run only.
  python -c "import sys; sys.path.insert(0, sys.argv[1]); import matrix as m; \
r = dict(zip(m.COLUMNS, sys.argv[2].split('\t'))); sys.exit(1 if m.failures(r) else 0)" \
    "$NEWTONIAN_TOOLS" "$R" && rm -f "${O}_decision.tsv" "${O}_state.tsv" "${O}_replay.stderr"

}
export -f one
RES="$ELITE_DIAG_DIR/elite_${T}_matrix.tsv"
for G in "$CD"/*.geom; do
  N=$(basename "$G" .geom)
  [[ " ${MATRIX_MODELS:-I X} " == *" ${N##*-} "* ]] || continue
  for D in 1 3 6; do echo "$N $D"; done
done | xargs -P 8 -L 1 bash -c 'one "$0" "$1"' > "$RES"
python "$NEWTONIAN_TOOLS/matrix.py" table "$RES"
