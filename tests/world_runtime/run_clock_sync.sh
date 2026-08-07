#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/world_runtime_tests"

cmake \
    -S "${ROOT_DIR}/tests/world_runtime" \
    -B "${BUILD_DIR}" \
    -G Ninja

cmake --build "${BUILD_DIR}" --target clock_sync_tests

ctest \
    --test-dir "${BUILD_DIR}" \
    --output-on-failure \
    -R '^clock_sync$'
