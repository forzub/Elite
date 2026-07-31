#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/system_map_behavior_tests"

cmake \
    -S "${ROOT_DIR}/tests/system_map" \
    -B "${BUILD_DIR}" \
    -G Ninja

cmake --build "${BUILD_DIR}"

ctest \
    --test-dir "${BUILD_DIR}" \
    --output-on-failure
