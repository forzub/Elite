#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"
BUILD_DIR="${ELITE_TEST_BUILD_ROOT}/navigation_guidance"
elite_require_build_toolchain

cmake \
    -S "${ROOT_DIR}/tests/navigation_guidance" \
    -B "${BUILD_DIR}" \
    -G Ninja

cmake --build "${BUILD_DIR}"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
