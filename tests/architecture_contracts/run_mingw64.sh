#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/architecture_contract_tests"

if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN="python3"
elif command -v python >/dev/null 2>&1; then
    PYTHON_BIN="python"
else
    echo "Python is required for the architecture contract check." >&2
    exit 1
fi

"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_architecture.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_hub_detail_revision.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_system_membership.py"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/architecture_contracts/check_active_system_context.py"

cmake \
    -S "${ROOT_DIR}/tests/architecture_contracts" \
    -B "${BUILD_DIR}" \
    -G Ninja

cmake --build "${BUILD_DIR}"

ctest \
    --test-dir "${BUILD_DIR}" \
    --output-on-failure
