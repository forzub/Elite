#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"
elite_require_build_toolchain

BUILD_DIR="${ELITE_TEST_BUILD_ROOT}/navigation_runtime"

now_ms() {
    date +%s%3N
}

elapsed_ms() {
    local start_ms="$1"
    local end_ms="$2"
    echo $((end_ms - start_ms))
}

TOTAL_START_MS="$(now_ms)"

CONFIGURE_START_MS="$(now_ms)"
cmake \
    -S "${ROOT_DIR}/tests/navigation_runtime" \
    -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release
CONFIGURE_END_MS="$(now_ms)"

BUILD_START_MS="$(now_ms)"
cmake --build "${BUILD_DIR}"
BUILD_END_MS="$(now_ms)"

TEST_START_MS="$(now_ms)"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
TEST_END_MS="$(now_ms)"

TOTAL_END_MS="$(now_ms)"

echo "[TIMING] navigation_runtime configure_ms=$(elapsed_ms "${CONFIGURE_START_MS}" "${CONFIGURE_END_MS}")"
echo "[TIMING] navigation_runtime build_ms=$(elapsed_ms "${BUILD_START_MS}" "${BUILD_END_MS}")"
echo "[TIMING] navigation_runtime tests_ms=$(elapsed_ms "${TEST_START_MS}" "${TEST_END_MS}")"
echo "[TIMING] navigation_runtime total_ms=$(elapsed_ms "${TOTAL_START_MS}" "${TOTAL_END_MS}")"
