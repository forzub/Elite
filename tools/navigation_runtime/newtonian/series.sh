#!/usr/bin/env bash
# Build the game, then fly each model and print the decision-trace summary.
# usage: series.sh TAG SECONDS MODEL...   e.g. series.sh s3 270 Newtonian-I Newtonian-Y
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
T=$1; S=$2; shift 2
cd "$ELITE_ROOT"
bash build_mingw64.sh > "$ELITE_DIAG_DIR/elite_${T}_build.log" 2>&1 || {
  echo BUILD FAILED; grep -m10 error "$ELITE_DIAG_DIR/elite_${T}_build.log"; exit 1; }
echo build ok
for M in "$@"; do
  bash "$NEWTONIAN_TOOLS/fly.sh" "$M" "$S" "${T}_$M" > /dev/null
  echo "== $M"
  python "$NEWTONIAN_TOOLS/decision.py" "${T}_$M" | head -12
  python "$NEWTONIAN_TOOLS/terminal.py" "${T}_$M" | grep -E 'TERMINAL_STEP|max omega|DONE at'
done
