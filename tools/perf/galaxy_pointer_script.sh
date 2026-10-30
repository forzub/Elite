#!/usr/bin/env bash
# DEV: Galaxy Map pointer acceptance in the REAL game. Starts the game
# (nav-stress:simple), opens the Galaxy Map at 25 s and runs the scripted
# pointer (ELITE_DEV_GALAXY_POINTER_SCRIPT: repeated star / cube / double
# click / cloud / drag sequences through the production input path) with the
# pointer trace on. Stops only the game PID it started.
# usage: galaxy_pointer_script.sh [MAX_SECONDS=590]   PHASES=035 limits phases
# output: $ELITE_DIAG_DIR/elite_galaxy_pointer_script.txt (PASS/FAIL table),
#         elite_galaxy_pick_trace.csv, elite_galaxy_pointer_stderr.txt
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
O="${ELITE_DIAG_DIR:-/d/tmp}"
cd "$ROOT"
tasklist | grep -qi EliteGame && { echo "EliteGame already running - abort"; exit 1; }
rm -f "$O/elite_galaxy_pick_trace.csv" "$O/elite_galaxy_pointer_script.txt"
ELITE_DEV_GALAXY_POINTER_PHASES=${PHASES:-} ELITE_TRACE_RUNTIME=1 ELITE_DEV_GALAXY_MAP_AT=25 \
  ELITE_DEV_GALAXY_POINTER_SCRIPT=1 ELITE_GALAXY_PICK_TRACE=1 ELITE_SHIP_PHYSICAL_TEST_MODEL=Newtonian-I \
  ./build/EliteGame.exe --dev-direct-flight --diagnostic-scenario nav-stress:simple \
  > /dev/null 2> "$O/elite_galaxy_pointer_stderr.txt" &
# No EliteGame ran before the launch (checked above), so the one running now is ours.
WINPID=; W=0
while [ -z "$WINPID" ] && [ $W -lt 30 ]; do
  sleep 1; W=$((W + 1))
  WINPID=$(tasklist //FI "IMAGENAME eq EliteGame.exe" //NH //FO CSV | grep -i elitegame | head -1 | cut -d, -f2 | tr -d '"')
done
[ -n "$WINPID" ] || { echo "game did not start"; exit 1; }
T=0
while [ $T -lt "${1:-590}" ]; do
  sleep 5; T=$((T + 5))
  grep -q "GalaxyPointerScript\] DONE" "$O/elite_galaxy_pointer_stderr.txt" && break
done
sleep 2
taskkill //PID "$WINPID" //F > /dev/null 2>&1
cat "$O/elite_galaxy_pointer_script.txt" 2>/dev/null || echo "no result (script did not finish in ${1:-590} s)"
