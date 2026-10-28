#!/usr/bin/env bash
# Offline replay of one case on production ShipCore physics.
# usage: replay.sh CASE TAG [MODEL] [SECONDS] [DELAY_TICKS]
#   CASE: a name in cases/ (newtonian-I) or a path to a dumped case file
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
C=$1; T=$2; M=${3:-Newtonian-I}; S=${4:-260}; D=${5:-3}
[ -f "$C" ] || C="$CASES_DIR/$C.case"
O=$ELITE_DIAG_DIR
rm -f "$O/elite_${T}_decision.tsv" "$O/elite_${T}_events.tsv"
ELITE_NEWTONIAN_POLICY_TRACE="$O/elite_${T}_decision.tsv" \
ELITE_NEWTONIAN_EVENT_TRACE="$O/elite_${T}_events.tsv" \
  "$REPLAY_EXE" "$C" "$M" "$S" "$D" 2> "$O/elite_${T}_replay.stderr"
grep -h REPLAY "$O/elite_${T}_replay.stderr"
