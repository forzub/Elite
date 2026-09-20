#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"
elite_require_build_toolchain

if ! elite_require_python; then
    echo "Python 3 is required for the Stage-1 navigation gate." >&2
    exit 2
fi
PYTHON_BIN="${ELITE_PYTHON_BIN}"

echo "=== Stage 1 architecture ==="
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_geometric_path_planner.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_navigation_stage1_nominal_route.py"

echo "=== Stage 1 nominal route test ==="
TEST_BUILD="${ELITE_TEST_BUILD_ROOT}/navigation_runtime"
cmake -S "${ROOT_DIR}/tests/navigation_runtime" -B "${TEST_BUILD}" -G Ninja
cmake --build "${TEST_BUILD}" --target nominal_route_planner_tests
ctest --test-dir "${TEST_BUILD}" -L navigation_stage1 --output-on-failure

echo "=== Stage 1 viewer build ==="
VIEWER_BUILD="${ROOT_DIR}/build/tools/navigation_runtime"
cmake -S "${ROOT_DIR}/tools/navigation_runtime" -B "${VIEWER_BUILD}" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "${VIEWER_BUILD}"

echo
echo "STAGE 1 NAVIGATION GATE: PASS"
echo
echo "Viewer launch:"
echo "cd /d/__elite/work"
echo "./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json"
