#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/benchmarks/navigation_trajectory_continuous"

cmake -S "${ROOT}/benchmarks/navigation_trajectory_continuous" -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}"
"${BUILD_DIR}/navigation_trajectory_continuous_benchmark.exe" "$@"
