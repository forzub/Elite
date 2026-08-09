#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

failures=0

run_suite() {
    local name="$1"
    local script="$2"

    echo
    echo "================================================================"
    echo "TEST BLOCK: ${name}"
    echo "================================================================"

    if bash "${ROOT_DIR}/${script}"; then
        echo "[PASS BLOCK] ${name}"
    else
        echo "[FAIL BLOCK] ${name}" >&2
        failures=$((failures + 1))
    fi
}

run_suite \
    "WORLD RUNTIME + GLOBAL TIME CONTRACT" \
    "tests/world_runtime/run_mingw64.sh"

run_suite \
    "CROSS-TIMELINE + DIAGNOSTIC CONTRACTS" \
    "tests/architecture_contracts/run_mingw64.sh"

run_suite \
    "CLIENT PRESENTATION PIPELINE" \
    "tests/presentation_pipeline/run_mingw64.sh"

run_suite \
    "SERVER INTERACTION ACTIVATION" \
    "tests/interaction_activation/run_mingw64.sh"

run_suite \
    "SYSTEM MAP BEHAVIOR + ARCHITECTURE" \
    "tests/system_map/run_mingw64.sh"

echo
echo "================================================================"
if (( failures == 0 )); then
    echo "ALL READY BLOCKS PASSED"
    echo "================================================================"
    exit 0
fi

echo "FAILED READY BLOCKS: ${failures}" >&2
echo "================================================================" >&2
exit 1
