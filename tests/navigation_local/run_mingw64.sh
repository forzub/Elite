#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"
elite_require_build_toolchain

BUILD_DIR="${ELITE_TEST_BUILD_ROOT}/navigation_local"

cmake \
    -S "${ROOT_DIR}/tests/navigation_local" \
    -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "${BUILD_DIR}" --target navigation_local_tests
ctest --test-dir "${BUILD_DIR}" --output-on-failure
