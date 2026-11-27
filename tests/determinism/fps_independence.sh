#!/usr/bin/env bash
# D1 regression (audit #6 / #7): the authoritative state must not depend on the client frame rate.
#
#   tests/determinism/fps_independence.sh [build dir] [out dir]
#
# Runs the game several times with the universe time pinned (ELITE_DEV_UNIVERSE_START_S) and the same start, at
# different client FPS caps and with render stalls, and compares the FNV state hash of every ship (position,
# velocity, orientation, mass, J charge) dumped at the same server ticks ([TickDump]). PASS = identical hashes at
# every tick in every run. Also reports simulation time vs real time from the server worker ([WorkerTiming]).
# Runs are visible on screen (the author's rule); each takes ~1.5 min.
set -euo pipefail
BUILD="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/build-relwithdebinfo}"
OUT="${2:-D:/tmp/d1_fps}"
TICKS="300,600,900,1500"
mkdir -p "${OUT}"
declare -a NAMES=(fps15 fps60 fps240 uncapped stall500)
declare -a CAPS=(15 60 240 0 0)
declare -a STALLS=("" "" "" "" "40:500,50:1000")
cd "${BUILD}"
for i in "${!NAMES[@]}"; do
    n="${NAMES[$i]}"
    if [ "${ANALYZE_ONLY:-0}" = "1" ]; then echo "${n}: $(tr '\n' ' ' < "${OUT}/${n}.hash")"; continue; fi
    env ELITE_SESSION_LOG=0 ELITE_DEV_OFFSCREEN_WINDOW=0 ELITE_DEV_START=hub:earth_ring_hub@8 \
        ELITE_DEV_UNIVERSE_START_S=100000 ELITE_DEV_TICK_DUMP="${TICKS}" ELITE_DEV_WORKER_TIMING=1 \
        ELITE_ECONOMY_STATE_FILE="${OUT}/${n}_e.json" \
        ELITE_DEV_FPS_CAP="${CAPS[$i]}" ${STALLS[$i]:+ELITE_DEV_RENDER_STALL=${STALLS[$i]}} \
        ELITE_DEV_QUIT_AFTER_S=95 ./EliteGame.exe --dev-direct-flight > "${OUT}/${n}.log" 2>&1 || true
    grep -a "\[TickDump\] tick" "${OUT}/${n}.log" | sed 's/.* tick \([0-9]*\) .* hash \([0-9a-f]*\).*/\1 \2/' > "${OUT}/${n}.hash" || true
    echo "${n}: $(tr '\n' ' ' < "${OUT}/${n}.hash")"
done
ref="${OUT}/${NAMES[0]}.hash"
fail=0
[ -s "${ref}" ] || { echo "FAIL: no tick dump in ${NAMES[0]}"; exit 1; }
# compare the ticks both runs reached (a run may quit before the last dump tick); at least 3 common ticks
for n in "${NAMES[@]:1}"; do
    common="$(join "${ref}" "${OUT}/${n}.hash" | wc -l)"
    differ="$(join "${ref}" "${OUT}/${n}.hash" | awk '$2 != $3' | wc -l)"
    if [ "${common}" -lt 3 ] || [ "${differ}" -ne 0 ]; then echo "FAIL: ${n}: ${common} common ticks, ${differ} differ"; fail=1
    else echo "  ${n}: ${common} common ticks identical"; fi
done
# simulation vs real time: the worker's handed elapsed over its wall window (own clock: ~100 %)
for n in "${NAMES[@]}"; do
    awk -v n="${n}" '/\[WorkerTiming\]/ { w += $2; match($0, /elapsed handed: sum [0-9]+/); e += substr($0, RSTART + 20, RLENGTH - 20) / 1000.0 }
        END { if (w > 0) printf "  %s: simulation fed %.1f s of %.1f s wall (%.1f %%)\n", n, e, w, 100.0 * e / w }' "${OUT}/${n}.log"
done
[ ${fail} -eq 0 ] && echo "PASS: identical state hashes at ticks ${TICKS} across ${NAMES[*]}"
exit ${fail}
