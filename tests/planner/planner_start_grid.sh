#!/usr/bin/env bash
# Stabilization stage 3: the planner's start grid - a pad landing (B2-04 at earth_orbital_hub, the author's failed case)
# requested from a grid of start points in normal flight, at rest relative to the hub. Each run ends LANDED (success),
# REFUSED (a finite, told refusal), CRASH or HANG (neither within the budget - must be zero).
#   tests/planner/planner_start_grid.sh <out dir> [points file]
# points file: one "x,y,z" city-frame point per line (default: the built-in grid below)
# Each game is ended as soon as its outcome is in the log (its own PID only); the budget is the hang verdict.
set -u
OUT=${1:?out dir}
BIN=${ELITE_BIN:-build-relwithdebinfo}
BUDGET=${GRID_BUDGET_S:-480}
PAD=${GRID_PAD:-B2-04}
HUB=${GRID_HUB:-earth_orbital_hub}
mkdir -p "$OUT"
if [ $# -ge 2 ]; then POINTS=$(cat "$2"); else
POINTS="0,2810,5588
0,2810,-5588
5588,2810,0
-5588,2810,0
0,6000,0
0,-6000,0
0,1500,3000
3000,1500,3000
0,4000,10000
-8000,500,-8000"
fi
summary="$OUT/summary.txt"
: > "$summary"
outcome() {   # the log's verdict so far (empty: still running)
    if grep -aq "pad $PAD: LANDED" "$1"; then echo LANDED
    elif grep -aq "phase=refused\|phase=failed" "$1"; then echo REFUSED
    elif grep -aq "PilotLost\]" "$1"; then echo CRASH
    fi
}
i=0
for spec in $POINTS; do
    # a line: "x,y,z" | "x,y,z|vx,vy,vz" | "x,y,z|vx,vy,vz|yaw,pitch,roll" (navigation stage 3: the real start state)
    IFS='|' read -r p vel spin <<< "$spec"
    extra=()
    [ -n "${vel:-}" ] && extra+=("ELITE_DEV_START_VELOCITY=$vel")
    [ -n "${spin:-}" ] && extra+=("ELITE_DEV_START_SPIN=$spin")
    i=$((i + 1))
    log="$OUT/run_$i.log"
    start=$(date +%s)
    (cd "$BIN" && exec env ELITE_SESSION_LOG=0 ELITE_DEV_OFFSCREEN_WINDOW=0 ELITE_DEV_START="hub:$HUB@10" ELITE_DEV_START_CITY_AT="$p" "${extra[@]}" \
        ELITE_ECONOMY_STATE_FILE="$OUT/e_$i.json" "ELITE_DEV_SOL_MAP_SCRIPT=${GRID_SCRIPT:-15:gear;20:padland:$PAD@$HUB.city}" \
        ELITE_DEV_QUIT_AFTER_S="$((BUDGET + 30))" ./EliteGame.exe --dev-direct-flight > "$log" 2>&1) &
    runner=$!
    r=""
    while kill -0 "$runner" 2>/dev/null; do
        sleep 3
        r=$(outcome "$log")
        if [ -n "$r" ] || [ $(( $(date +%s) - start )) -ge "$BUDGET" ]; then
            # end THIS run's game: the EliteGame.exe whose parent chain is our runner (Windows: by its env.exe parent)
            pid=$(powershell -NoProfile -Command "(Get-CimInstance Win32_Process | Where-Object { \$_.Name -eq 'EliteGame.exe' -and \$_.CommandLine -like '*' } | Where-Object { (Get-CimInstance Win32_Process -Filter (\"ProcessId=\" + \$_.ParentProcessId)).CommandLine -like '*e_$i.json*' }).ProcessId" | tr -d '\r')
            [ -n "$pid" ] && taskkill //PID "$pid" //F > /dev/null 2>&1
            break
        fi
    done
    wait "$runner" 2>/dev/null
    [ -z "$r" ] && r=$(outcome "$log")
    [ -z "$r" ] && r=HANG
    replans=$(grep -ac "phase=replan" "$log")
    echo "$i $spec $r replans=$replans t=$(( $(date +%s) - start ))s $(grep -a "phase=refused\|pad $PAD: LANDED\|phase=failed\|PilotLost\]" "$log" | head -1 | cut -c1-140)" | tee -a "$summary"
done
echo "LANDED $(grep -c " LANDED " "$summary")  REFUSED $(grep -c " REFUSED " "$summary")  CRASH $(grep -c " CRASH " "$summary")  HANG $(grep -c " HANG " "$summary")" | tee -a "$summary"
