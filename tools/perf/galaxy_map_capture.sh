#!/usr/bin/env bash
# DEV: start the real game (nav-stress:simple), open the Galaxy Map at 25 s and
# write the game's own back buffer at the given times (ELITE_DEV_FRAME_CAPTURE).
# Extra env (e.g. ELITE_DEV_GALAXY_HIERARCHY_SCRIPT=1) is passed through.
# usage: galaxy_map_capture.sh "SECONDS:PATH[;SECONDS:PATH...]" [MAX_SECONDS=60]
# Stops only the game PID it started.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
O="${ELITE_DIAG_DIR:-/d/tmp}"
cd "$ROOT"
tasklist | grep -qi EliteGame && { echo "EliteGame already running - abort"; exit 1; }
ELITE_DEV_FRAME_CAPTURE="$1" ELITE_DEV_GALAXY_MAP_AT=${MAP_AT:-25} ELITE_SHIP_PHYSICAL_TEST_MODEL=Newtonian-I \
  ./build/EliteGame.exe --dev-direct-flight --diagnostic-scenario nav-stress:simple \
  > /dev/null 2> "$O/elite_galaxy_capture_stderr.txt" &
WINPID=; W=0
while [ -z "$WINPID" ] && [ $W -lt 30 ]; do
  sleep 1; W=$((W + 1))
  WINPID=$(tasklist //FI "IMAGENAME eq EliteGame.exe" //NH //FO CSV | grep -i elitegame | head -1 | cut -d, -f2 | tr -d '"')
done
[ -n "$WINPID" ] || { echo "game did not start"; exit 1; }
T=0
while [ $T -lt "${2:-60}" ]; do
  sleep 2; T=$((T + 2))
  grep -q "\[GalaxyHierarchyScript\] DONE" "$O/elite_galaxy_capture_stderr.txt" && break
done
sleep 1
taskkill //PID "$WINPID" //F > /dev/null 2>&1
grep "FrameCapture\|GalaxyHierarchyScript" "$O/elite_galaxy_capture_stderr.txt" | tail -60
