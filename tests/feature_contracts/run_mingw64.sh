#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/feature_contract_tests"

if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN="python3"
elif command -v python >/dev/null 2>&1; then
    PYTHON_BIN="python"
else
    echo "Python is required for the feature-contract check." >&2
    exit 1
fi

"${PYTHON_BIN}" "${ROOT_DIR}/tests/feature_contracts/check_feature_contracts.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/feature_contracts/check_constellation_overlay.py"

cmake \
    -S "${ROOT_DIR}/tests/feature_contracts" \
    -B "${BUILD_DIR}" \
    -G Ninja

cmake --build "${BUILD_DIR}"

ctest \
    --test-dir "${BUILD_DIR}" \
    --output-on-failure
