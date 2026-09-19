#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"
elite_require_build_toolchain

SERVER_EXE="${ROOT_DIR}/build/headless_server/EliteServer.exe"
LOG_DIR="${ROOT_DIR}/build/logs"
mkdir -p "${LOG_DIR}"

STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_PATH="${LOG_DIR}/navigation_live_scheduler_${STAMP}.log"

now_ms() {
    date +%s%3N
}

START_MS="$(now_ms)"

if [[ ! -x "${SERVER_EXE}" ]]; then
    echo "[FAIL] missing canonical headless server: ${SERVER_EXE}" | tee "${LOG_PATH}"
    END_MS="$(now_ms)"
    echo "[TIMING] navigation_live_scheduler total_ms=$((END_MS - START_MS)) rc=127" | tee -a "${LOG_PATH}"
    if command -v cygpath >/dev/null 2>&1; then
        DISPLAY_LOG="$(cygpath -w "${LOG_PATH}")"
    else
        DISPLAY_LOG="${LOG_PATH}"
    fi
    echo "[LOG] ${DISPLAY_LOG}"
    exit 127
fi

set +e
"${SERVER_EXE}" --self-test-navigation 2>&1 | tee "${LOG_PATH}"
SERVER_RC=${PIPESTATUS[0]}
set -e

END_MS="$(now_ms)"
echo "[TIMING] navigation_live_scheduler total_ms=$((END_MS - START_MS)) rc=${SERVER_RC}" | tee -a "${LOG_PATH}"

if command -v cygpath >/dev/null 2>&1; then
    DISPLAY_LOG="$(cygpath -w "${LOG_PATH}")"
else
    DISPLAY_LOG="${LOG_PATH}"
fi

echo "[LOG] ${DISPLAY_LOG}"
exit "${SERVER_RC}"
