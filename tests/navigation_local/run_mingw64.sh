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

# Build the complete local-navigation test project. CTest may register more than
# one executable (horizon reference + avoidance behavior); building only the
# historical navigation_local_tests target leaves newer registered tests absent.
cmake --build "${BUILD_DIR}"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
