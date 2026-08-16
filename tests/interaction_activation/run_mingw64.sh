#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"
BUILD_DIR="${ELITE_TEST_BUILD_ROOT}/interaction_activation"
elite_require_build_toolchain


if ! elite_require_python; then
    exit 2
fi
PYTHON_BIN="${ELITE_PYTHON_BIN}"
"${PYTHON_BIN}" "${ROOT_DIR}/tests/interaction_activation/check_architecture.py"

cmake \
    -S "${ROOT_DIR}/tests/interaction_activation" \
    -B "${BUILD_DIR}" \
    -G Ninja

cmake --build "${BUILD_DIR}"

ctest \
    --test-dir "${BUILD_DIR}" \
    --output-on-failure
