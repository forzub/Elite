#!/usr/bin/env bash
# Stabilization stage 7 (D1 / D2): the server's own clock - the test matrix. Each case one visible game, ~100 s,
# judged from the [ServerClock] lines (the account every 10 s: wall / simulated / dropped / debt; PAUSED / RESUMED;
# OVERLOADED / recovered).
#   tests/server_clock/clock_matrix.sh <out dir>
# Cases (own clock unless named):
#   own_baseline     nothing injected                         -> simulated ~ wall, dropped 0
#   own_pause        ELITE_DEV_PAUSE=40:10 (the menu pause)   -> PAUSED / RESUMED, simulated ~ wall - 10 s, no catch-up
#   own_render_stall render stalls 1000 + 500 ms              -> dropped 0 (the render does not drive the clock)
#   own_worker_debt  the worker blocks 300 ms once            -> a debt, caught up, dropped 0
#   own_overload     the worker blocks 3000 ms once           -> OVERLOADED, a bounded drop, recovered
#   client_render_stall  the old client-driven clock, render stall 1000 ms -> the drop the own clock removes
set -u
OUT=${1:?out dir}
BIN=${ELITE_BIN:-build-relwithdebinfo}
mkdir -p "$OUT"
report="$OUT/matrix.txt"
: > "$report"
run() {   # run <case> <own 0/1> <env...>
    local name=$1 own=$2
    shift 2
    local extra=()
    [ "$own" = 1 ] && extra+=(ELITE_SERVER_OWN_CLOCK=1)
    (cd "$BIN" && env ELITE_SESSION_LOG=0 ELITE_DEV_OFFSCREEN_WINDOW=0 ELITE_DEV_START=hub:earth_orbital_hub@50 \
        ELITE_ECONOMY_STATE_FILE="$OUT/e_$name.json" ELITE_DEV_QUIT_AFTER_S=100 "${extra[@]}" "$@" \
        ./EliteGame.exe --dev-direct-flight > "$OUT/$name.log" 2>&1)
    local last
    last=$(grep -a "\[ServerClock\] account" "$OUT/$name.log" | tail -1 | sed 's/.*account: //')
    local events
    events=$(grep -a "\[ServerClock\] \(PAUSED\|RESUMED\|OVERLOADED\|recovered\|DEV worker stall\)" "$OUT/$name.log" | sed 's/.*\[ServerClock\] //' | cut -c1-90 | tr '\n' ';')
    echo "$name | ${last:-no account (client-driven)} | $events" | tee -a "$report"
}
run own_baseline 1
run own_pause 1 ELITE_DEV_PAUSE=40:10
run own_render_stall 1 "ELITE_DEV_RENDER_STALL=40:1000,60:500"
run own_worker_debt 1 ELITE_DEV_WORKER_STALL=40:300
run own_overload 1 ELITE_DEV_WORKER_STALL=40:3000
run client_render_stall 0 "ELITE_DEV_RENDER_STALL=40:1000" ELITE_DEV_WORKER_TIMING=1
