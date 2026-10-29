#!/usr/bin/env bash
# One real game flight with the Newtonian decision trace.
# usage: fly.sh MODEL SECONDS TAG      (MODEL: Newtonian-I|Y|X, Assisted-I ...)
# outputs: $ELITE_DIAG_DIR/elite_TAG{.log,_decision.tsv,_events.tsv,_case.txt,
#          _route.txt,_runtime.csv}
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
M=$1; S=$2; T=$3
O=$ELITE_DIAG_DIR
cd "$ELITE_ROOT"
tasklist | grep -qi EliteGame && { echo "EliteGame already running - abort"; exit 1; }
rm -f "$O/elite_navigation_stress.log" "$O/elite_newtonian_runtime.csv" \
      "$O/elite_${T}_decision.tsv" "$O/elite_${T}_events.tsv"
ELITE_AUTOPILOT_CASE_DUMP="$O/elite_${T}_case.txt" \
  ELITE_ROUTE_DUMP="$O/elite_${T}_route.txt" \
  ELITE_NEWTONIAN_POLICY_TRACE="$O/elite_${T}_decision.tsv" \
  ELITE_NEWTONIAN_EVENT_TRACE="$O/elite_${T}_events.tsv" \
  ELITE_SHIP_PHYSICAL_TEST_MODEL=$M \
 ./build/EliteGame.exe --dev-direct-flight --diagnostic-scenario nav-stress:simple \
   > "$O/elite_$T.stdout" 2> "$O/elite_$T.stderr" &
# Stop exactly the instance started here (never other EliteGame windows).
# No EliteGame ran before the launch (checked above), so the one running
# now is ours.
WINPID=; WAITED=0
while [ -z "$WINPID" ] && [ $WAITED -lt 30 ]; do
  sleep 1; WAITED=$((WAITED + 1))
  WINPID=$(tasklist //FI "IMAGENAME eq EliteGame.exe" //NH //FO CSV | grep -i elitegame | head -1 | cut -d, -f2 | tr -d '"')
done
[ -n "$WINPID" ] || echo "WARNING: game did not start"
sleep "$((S - WAITED))"
taskkill //PID "$WINPID" //F > /dev/null 2>&1; sleep 3
cp "$O/elite_navigation_stress.log" "$O/elite_$T.log"
[ -f "$O/elite_newtonian_runtime.csv" ] && \
  cp "$O/elite_newtonian_runtime.csv" "$O/elite_${T}_runtime.csv"
echo "== $T ($M) done"
