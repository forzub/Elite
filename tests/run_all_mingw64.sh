#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT_DIR}/tests/helpers/elite_server_process_guard.sh"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"

if ! elite_require_ready_toolchain; then
    exit 2
fi

if ! elite_fail_if_server_running "ready harness preflight"; then
    exit 1
fi

elite_cleanup_legacy_build_layout

failures=0
headless_server_ready=0
client_ready=0

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

run_suite \
    "FEATURE SURFACE CONTRACTS" \
    "tests/feature_contracts/run_mingw64.sh"

run_suite \
    "CLIENT ACCEPTANCE HARNESS" \
    "tests/client_acceptance/run_mingw64.sh"

run_suite \
    "MULTIPLAYER CLIENT ACCEPTANCE" \
    "tests/multiplayer_client_acceptance/run_mingw64.sh"

# elite_build_canonical_server owns the headless-only configure contract:
# -DELITE_BUILD_CLIENT=OFF, -DELITE_BUILD_SERVER=ON, --target EliteServer.
run_headless_server_build() {
    echo
    echo "================================================================"
    echo "TEST BLOCK: HEADLESS SERVER BUILD"
    echo "================================================================"

    if elite_build_canonical_server \
        && (cd "${ELITE_SERVER_BUILD_DIR}" && ./EliteServer.exe --self-test); then
        echo "[PASS BLOCK] HEADLESS SERVER BUILD + AUTHORITATIVE SMOKE"
        headless_server_ready=1
    else
        echo "[FAIL BLOCK] HEADLESS SERVER BUILD + AUTHORITATIVE SMOKE" >&2
        failures=$((failures + 1))
        headless_server_ready=0
    fi
}

run_headless_server_build

run_main_target_build() {
    echo
    echo "================================================================"
    echo "TEST BLOCK: MAIN GAME BUILD"
    echo "================================================================"

    if elite_build_canonical_client \
        && (cd "${ELITE_CLIENT_BUILD_DIR}" && ./EliteGame.exe --self-test-fast-universe); then
        echo "[PASS BLOCK] MAIN GAME BUILD + FAST UNIVERSE SMOKE"
        client_ready=1
    else
        echo "[FAIL BLOCK] MAIN GAME BUILD + FAST UNIVERSE SMOKE" >&2
        failures=$((failures + 1))
        client_ready=0
    fi
}

run_main_target_build

if ! elite_assert_unique_runtime_binaries; then
    failures=$((failures + 1))
fi

if (( headless_server_ready && client_ready )); then
    # Dependent runtime/process blocks consume exactly the same canonical
    # developer binaries that were built and smoked above. No alternate
    # ready/network server directories are allowed.
    export ELITE_CANONICAL_BUILDS_READY=1

    run_suite \
        "STANDALONE WINDOWS RUNTIME" \
        "tests/runtime_standalone/run_mingw64.sh"

    run_suite \
        "NETWORK PROCESS ACCEPTANCE" \
        "tests/network_process_acceptance/run_mingw64.sh"

    unset ELITE_CANONICAL_BUILDS_READY
else
    echo
    echo "================================================================"
    echo "SKIPPED DEPENDENT PROCESS BLOCKS"
    echo "================================================================"
    echo "[SKIP] standalone/network process tests require freshly built canonical EliteGame/EliteServer binaries" >&2
fi

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
