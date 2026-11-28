#!/usr/bin/env bash
# Stabilization stage 12: the behavioural acceptance suite - the live scenarios the program accepted, each with its
# verdict from the log. Runs visible (ELITE_DEV_OFFSCREEN_WINDOW=0), one game at a time, each under its own budget.
#   tests/scenarios/run_acceptance.sh <out dir> [scenario ...]      (default: all)
# Scenarios:
#   ring_landing      ring hub A1-03 pad landing from 8 km                          -> LANDED
#   preview_landing   the same under the Preview flavour (render-independence)      -> LANDED
#   two_worlds        world 1 left mid-landing to the menu, a new world lands        -> LANDED in world 2
#   save_continue     save -> exit -> load -> continue (the world save generations)  -> generation 1 restored, 2 committed
#   b204_side         B2-04 at Vysokiy from the author's side position              -> LANDED or REFUSED (finite)
# Plus the unit checks: EliteHubSpaceCheck, EliteWorldSaveCheck.
set -u
OUT=${1:?out dir}
shift
BIN=${ELITE_BIN:-build-relwithdebinfo}
ROOT=$(pwd)
mkdir -p "$OUT"
SCEN=${*:-"ring_landing preview_landing two_worlds save_continue b204_side"}
report="$OUT/acceptance.txt"
: > "$report"
verdict() { echo "$1: $2" | tee -a "$report"; }

game() {   # game <log> <budget s> <env...>
    local log=$1 budget=$2
    shift 2
    (cd "$BIN" && env ELITE_SESSION_LOG=0 ELITE_DEV_OFFSCREEN_WINDOW=0 ELITE_DEV_QUIT_AFTER_S="$budget" "$@" \
        ./EliteGame.exe --dev-direct-flight > "$log" 2>&1)
}

"$BIN/EliteHubSpaceCheck.exe" > "$OUT/hub_space.txt" 2>&1 && verdict unit_hub_space PASS || verdict unit_hub_space FAIL
"$BIN/EliteWorldSaveCheck.exe" "$OUT/world_save_unit" > "$OUT/world_save.txt" 2>&1 && verdict unit_world_save PASS || verdict unit_world_save FAIL

for s in $SCEN; do
    case $s in
    ring_landing)
        game "$OUT/ring.log" 420 ELITE_DEV_START=hub:earth_ring_hub@8 ELITE_ECONOMY_STATE_FILE="$OUT/ring_e.json" \
            "ELITE_DEV_SOL_MAP_SCRIPT=15:gear;20:padland:garage@earth_ring_hub.city"
        grep -aq "pad A1-03: LANDED" "$OUT/ring.log" && verdict ring_landing PASS || verdict ring_landing FAIL ;;
    preview_landing)
        game "$OUT/preview.log" 420 ELITE_DEV_PREVIEW_FLAVOR=1 ELITE_DEV_START=hub:earth_ring_hub@8 \
            ELITE_ECONOMY_STATE_FILE="$OUT/preview_e.json" "ELITE_DEV_SOL_MAP_SCRIPT=15:gear;20:padland:garage@earth_ring_hub.city"
        grep -aq "pad A1-03: LANDED" "$OUT/preview.log" && verdict preview_landing PASS || verdict preview_landing FAIL ;;
    two_worlds)
        game "$OUT/worlds.log" 900 ELITE_DEV_START=hub:earth_ring_hub@8 ELITE_DEV_NEW_WORLD_AT_S=150 \
            ELITE_ECONOMY_STATE_FILE="$OUT/worlds_e.json" "ELITE_DEV_SOL_MAP_SCRIPT=15:gear;20:padland:garage@earth_ring_hub.city"
        if grep -aq "starting a NEW local world" "$OUT/worlds.log" &&
           sed -n '/starting a NEW local world/,$p' "$OUT/worlds.log" | grep -aq "pad A1-03: LANDED"; then
            verdict two_worlds PASS
        else verdict two_worlds FAIL; fi ;;
    save_continue)
        mkdir -p "$OUT/save"
        game "$OUT/save1.log" 70 ELITE_DEV_START=hub:earth_orbital_hub@10 ELITE_ECONOMY_STATE_FILE="$OUT/save/w.json" \
            "ELITE_DEV_SOL_MAP_SCRIPT=25:econ:days:1"
        # run 2 restores the LAST generation run 1 committed (run 1 commits on changes and at shutdown), then commits newer
        g1=$(grep -o '"generation": [0-9]*' "$OUT/save/w.json.manifest.json" | head -1 | grep -o '[0-9]*$')
        game "$OUT/save2.log" 70 ELITE_DEV_START=hub:earth_orbital_hub@10 ELITE_ECONOMY_STATE_FILE="$OUT/save/w.json" \
            "ELITE_DEV_SOL_MAP_SCRIPT=25:econ:days:1"
        g2=$(grep -o '"generation": [0-9]*' "$OUT/save/w.json.manifest.json" | head -1 | grep -o '[0-9]*$')
        if [ -n "$g1" ] && grep -aq "restored from .* generation $g1 " "$OUT/save2.log" && [ "${g2:-0}" -gt "$g1" ]; then
            verdict save_continue PASS
        else verdict save_continue FAIL; fi ;;
    b204_side)
        printf '0,2810,5588\n' > "$OUT/b204.txt"
        bash "$ROOT/tests/planner/planner_start_grid.sh" "$OUT/b204" "$OUT/b204.txt" > /dev/null
        grep -q " LANDED \| REFUSED " "$OUT/b204/summary.txt" && verdict b204_side "PASS ($(head -1 "$OUT/b204/summary.txt" | cut -d' ' -f3))" \
            || verdict b204_side FAIL ;;
    *) verdict "$s" "UNKNOWN SCENARIO" ;;
    esac
done
echo "---"; grep -c "PASS" "$report" | xargs echo "passed:"; grep -c "FAIL" "$report" | xargs echo "failed:"
grep -q "FAIL" "$report" && exit 1
exit 0
