#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/tools/navigation_runtime"
TRACE_PATH="${1:-${ROOT_DIR}/tools/navigation_runtime/last_trace_newtonian.json}"

cmake \
    -S "${ROOT_DIR}/tools/navigation_runtime" \
    -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "${BUILD_DIR}"

exec "${BUILD_DIR}/bin/navigation_runtime_viewer.exe" "${TRACE_PATH}"
