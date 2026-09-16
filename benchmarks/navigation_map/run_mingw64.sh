#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$ROOT/work/build/benchmarks/navigation_map"

cmake -S "$ROOT/benchmarks/navigation_map" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD"

"$BUILD/navigation_map_benchmark.exe" "$@"
