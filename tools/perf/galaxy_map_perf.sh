#!/usr/bin/env bash
# DEV: Galaxy Map frame cost. Starts the game (nav-stress:simple), opens the
# Galaxy Map after OPEN_AT seconds (ELITE_DEV_GALAXY_MAP_AT) and records
# [FrameStats] / [GalaxyMapPerf] / [HITCH] (ELITE_TRACE_RUNTIME=1) for SECONDS.
# usage: galaxy_map_perf.sh TAG [SECONDS=70] [OPEN_AT=30]
# SHOT=1: also a screenshot $ELITE_DIAG_DIR/elite_TAG_map.png 12 s after the map opens.
# output: $ELITE_DIAG_DIR/elite_TAG_mapperf.txt (summary on stdout)
T=$1; S=${2:-70}; OPEN_AT=${3:-30}
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
O="${ELITE_DIAG_DIR:-/d/tmp}"
cd "$ROOT"
tasklist | grep -qi EliteGame && { echo "EliteGame already running - abort"; exit 1; }
ELITE_TRACE_RUNTIME=1 ELITE_DEV_GALAXY_MAP_AT=$OPEN_AT ELITE_SHIP_PHYSICAL_TEST_MODEL=Newtonian-I \
  ./build/EliteGame.exe --dev-direct-flight --diagnostic-scenario nav-stress:simple \
  > /dev/null 2> "$O/elite_${T}_mapperf.txt" &
# No EliteGame ran before the launch (checked above), so the one running
# now is ours.
WINPID=; WAITED=0
while [ -z "$WINPID" ] && [ $WAITED -lt 30 ]; do
  sleep 1; WAITED=$((WAITED + 1))
  WINPID=$(tasklist //FI "IMAGENAME eq EliteGame.exe" //NH //FO CSV | grep -i elitegame | head -1 | cut -d, -f2 | tr -d '"')
done
[ -n "$WINPID" ] || echo "WARNING: game did not start"
echo "game PID $WINPID"
if [ -n "$SHOT" ]; then
  sleep "$((OPEN_AT + 12 - WAITED))"
  powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$ROOT/tools/perf/window_shot.ps1")"     -ProcessId "$WINPID" -Out "$(cygpath -w "$O")\elite_${T}_map.png"
  WAITED=$((OPEN_AT + 12))
fi
sleep "$((S - WAITED))"
taskkill //PID "$WINPID" //F > /dev/null 2>&1; sleep 2
python - "$O/elite_${T}_mapperf.txt" "$OPEN_AT" <<'PY'
import re, sys
rows = [l for l in open(sys.argv[1], errors="replace") if l.startswith(("[FrameStats]", "[GalaxyMapPerf]", "[HITCH]"))]
med = [dict(re.findall(r"(\w+)=([\d.]+)", l)) for l in rows if l.startswith("[GalaxyMapPerf]")]
fs = [dict(re.findall(r"(\w+)=([\d.]+)", l)) for l in rows if l.startswith("[FrameStats]")]
print("galaxy-map windows:", len(med))
if med:
    # frame stats of the windows while the map is open = the last len(med) windows
    open_fs = fs[-len(med):]
    a = [float(m["medium_avg_ms"]) for m in med[1:]] or [float(med[0]["medium_avg_ms"])]
    print("medium draw ms: avg %.2f  max %.2f" % (sum(a) / len(a), max(float(m["medium_max_ms"]) for m in med)))
    f = [float(x["avg_ms"]) for x in open_fs[1:]] or [float(open_fs[0]["avg_ms"])]
    print("frame ms (map open): avg %.2f  max %.2f  frames>50ms %d" % (
        sum(f) / len(f), max(float(x["max_ms"]) for x in open_fs[1:] or open_fs),
        sum(int(x["over50ms"]) for x in open_fs[1:] or open_fs)))
print("HITCH frames:", sum(1 for l in rows if l.startswith("[HITCH]")))
PY
