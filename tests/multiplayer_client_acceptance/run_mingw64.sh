#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN="python3"
elif command -v python >/dev/null 2>&1; then
    PYTHON_BIN="python"
else
    echo "Python is required for multiplayer client acceptance architecture checks." >&2
    exit 1
fi

"${PYTHON_BIN}" "${ROOT_DIR}/tests/multiplayer_client_acceptance/check_architecture.py"

cmake \
    -S "${ROOT_DIR}" \
    -B "${BUILD_DIR}" \
    -G Ninja

cmake --build "${BUILD_DIR}" --target EliteGame

(
    cd "${BUILD_DIR}"
    ./EliteGame.exe --self-test-multiplayer-client
)
