#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"
BUILD_DIR="${ELITE_TEST_BUILD_ROOT}/system_map"
elite_require_build_toolchain


if ! elite_require_python; then
    echo "Python 3 is required for this test block." >&2
    exit 2
fi
PYTHON_BIN="${ELITE_PYTHON_BIN}"

"${PYTHON_BIN}" "${ROOT_DIR}/tests/system_map/check_architecture.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/system_map/check_navigation_context.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/system_map/check_object_overlay.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/system_map/check_route_plan.py"

cmake \
    -S "${ROOT_DIR}/tests/system_map" \
    -B "${BUILD_DIR}" \
    -G Ninja

cmake --build "${BUILD_DIR}"

ctest \
    --test-dir "${BUILD_DIR}" \
    --output-on-failure
