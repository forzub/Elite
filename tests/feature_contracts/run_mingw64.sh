#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"
BUILD_DIR="${ELITE_TEST_BUILD_ROOT}/feature_contracts"
elite_require_build_toolchain


if ! elite_require_python; then
    echo "Python 3 is required for this test block." >&2
    exit 2
fi
PYTHON_BIN="${ELITE_PYTHON_BIN}"

"${PYTHON_BIN}" "${ROOT_DIR}/tests/feature_contracts/check_feature_contracts.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/feature_contracts/check_debug_ui_compatibility.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/feature_contracts/check_constellation_overlay.py"

cmake \
    -S "${ROOT_DIR}/tests/feature_contracts" \
    -B "${BUILD_DIR}" \
    -G Ninja

cmake --build "${BUILD_DIR}"

ctest \
    --test-dir "${BUILD_DIR}" \
    --output-on-failure
