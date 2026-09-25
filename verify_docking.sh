#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

TEST_BUILD_DIR="${ROOT_DIR}/build/tests/navigation_runtime"

echo "[DOCK-VERIFY] repo=${ROOT_DIR}"
echo "[DOCK-VERIFY] head=$(git rev-parse --short HEAD)"
echo "[DOCK-VERIFY] configure standalone navigation-runtime tests"

cmake -S "${ROOT_DIR}/tests/navigation_runtime" \
    -B "${TEST_BUILD_DIR}" \
    -G Ninja

echo "[DOCK-VERIFY] build docking_advisory_tests"
cmake --build "${TEST_BUILD_DIR}" \
    --target docking_advisory_tests \
    -j 8

echo "[DOCK-VERIFY] run native docking_advisory"
ctest --test-dir "${TEST_BUILD_DIR}" \
    -R "^docking_advisory$" \
    --output-on-failure

echo "[DOCK-VERIFY] run static manual-docking contract"
python "${ROOT_DIR}/tests/architecture_contracts/check_manual_docking_advisory.py"

echo "[DOCK-VERIFY] PASS"
