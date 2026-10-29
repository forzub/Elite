#!/usr/bin/env bash
# DEV: optimised build for PERFORMANCE MEASUREMENTS only (RelWithDebInfo,
# -O2 -g) in its own tree build-relwithdebinfo/. The developer workflow and
# the canonical runtime (build/, Debug -O0) are untouched; frame times of the
# Debug build must not be read as production performance.
# usage: build_relwithdebinfo.sh [target...]   (default: EliteGame)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/build-relwithdebinfo"
export PATH=/c/msys64/mingw64/bin:$PATH
if [ ! -f "$OUT/build.ninja" ]; then
  cmake -S "$ROOT" -B "$OUT" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
fi
cmake --build "$OUT" --target "${@:-EliteGame}" -j 8
echo "[perf build] $OUT/EliteGame.exe (RelWithDebInfo)"
