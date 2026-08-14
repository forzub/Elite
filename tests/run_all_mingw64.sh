#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT_DIR}/tests/helpers/elite_server_process_guard.sh"

if ! elite_fail_if_server_running "ready harness preflight"; then
    exit 1
fi

failures=0
headless_server_ready=0
READY_SERVER_BUILD_DIR="${ROOT_DIR}/build/ready_headless_server"

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

run_headless_server_build() {
    echo
    echo "================================================================"
    echo "TEST BLOCK: HEADLESS SERVER BUILD"
    echo "================================================================"

    local build_dir="${READY_SERVER_BUILD_DIR}"

    if cmake -S "${ROOT_DIR}" -B "${build_dir}" -G Ninja \
        -DELITE_BUILD_CLIENT=OFF \
        -DELITE_BUILD_SERVER=ON \
        && cmake --build "${build_dir}" --target EliteServer \
        && (cd "${build_dir}" && ./EliteServer.exe --self-test); then
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

    if cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build" -G Ninja \
        && cmake --build "${ROOT_DIR}/build" --target EliteGame \
        && (cd "${ROOT_DIR}/build" && ./EliteGame.exe --self-test-fast-universe); then
        echo "[PASS BLOCK] MAIN GAME BUILD + FAST UNIVERSE SMOKE"
    else
        echo "[FAIL BLOCK] MAIN GAME BUILD + FAST UNIVERSE SMOKE" >&2
        failures=$((failures + 1))
    fi
}

run_main_target_build

if (( headless_server_ready )); then
    # Process/runtime tests consume the server binary that was built and smoked
    # in this ready run. This directory is deliberately separate from the
    # developer-facing build/headless_server path, which may be occupied by a
    # manually running server on Windows.
    export ELITE_TEST_SERVER_DIR="${READY_SERVER_BUILD_DIR}"
    export ELITE_TEST_CLIENT_DIR="${ROOT_DIR}/build"

    run_suite \
        "STANDALONE WINDOWS RUNTIME" \
        "tests/runtime_standalone/run_mingw64.sh"

    run_suite \
        "NETWORK PROCESS ACCEPTANCE" \
        "tests/network_process_acceptance/run_mingw64.sh"

    unset ELITE_TEST_SERVER_DIR
    unset ELITE_TEST_CLIENT_DIR
else
    echo
    echo "================================================================"
    echo "SKIPPED DEPENDENT PROCESS BLOCKS"
    echo "================================================================"
    echo "[SKIP] standalone/network process tests require a freshly built headless server; stale binaries are forbidden" >&2
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
